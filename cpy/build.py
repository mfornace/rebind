import sys, types, importlib, functools, inspect, logging, enum, typing

log = logging.getLogger(__name__)
# logging.basicConfig(level=logging.INFO)

################################################################################

class Scalar(enum.IntEnum):
    Bool = 0
    Char = 1
    SignedChar = 2
    UnsignedChar = 3
    Unsigned = 4
    Signed = 5
    Float = 6
    Pointer = 7

################################################################################

def render_scalars(info):
    find = lambda x, n: next(t for c, t, s in info if c == x and s == n)
    return {'float32': find(Scalar.Float, 32),
            'float64': find(Scalar.Float, 64),
            'uint32':  find(Scalar.Unsigned, 32),
            'uint64':  find(Scalar.Unsigned, 64),
            'int32':   find(Scalar.Signed, 32),
            'int64':   find(Scalar.Signed, 64)}

def render_module(pkg: str, doc: dict):
    log.info('rendering document into module %s', repr(pkg))
    out = doc.copy()

    out['types'] = {k: v for k, v in doc['contents'] if isinstance(v, tuple)}
    for k, (meth, data) in out['types'].items():
        cls = set_global_type(pkg, (doc['Value'],), k, dict(meth), out)
        cls.metadata = {k: v or None for k, v in data}

    out['objects'] = {k: set_global_object(pkg, k, v, out)
        for k, v in doc['contents'] if not isinstance(v, tuple)}

    out['scalars'] = render_scalars(doc['scalars'])

    mod = importlib.import_module(pkg)
    for k, v in out.items():
        setattr(mod, k, v)
    log.info('finished rendering document into module %s', repr(pkg))

    return out


################################################################################

def _wrapped(*args): # this is just used for its signature
    '''Call a function from a cpy.Document'''
    pass

################################################################################

def signatures(function):
    '''Get signatures out of a Function or a wrapped Function'''
    try:
        function = function.__kwdefaults__['_orig']
    except AttributeError:
        pass
    return function.signatures()

################################################################################

translations = {'{}': '__str__', '[]': '__getitem__', '()': '__call__'}

def split_module(pkg, name):
    x, y = '{}.{}'.format(pkg, name).rsplit('.', maxsplit=1)
    return importlib.import_module(x), y

def set_global_type(pkg: str, bases: tuple, name: str, methods, lookup={}):
    '''Define a new type in pkg'''
    mod, name = split_module(pkg, name)
    globalns = mod.__dict__.copy()
    globalns.update(lookup)
    try:
        props = dict(getattr(mod, name).__dict__)
    except AttributeError:
        props = {'__module__': mod.__name__}

    for k, v in translations.items():
        try:
            props[v] = props.pop(k)
        except KeyError:
            pass

    new = methods.pop('new', None)
    if new is None:
        def __init__(self):
            raise TypeError('No __init__ is possible')
    else:
        def __init__(self, *args, _new=new, signature=None):
            self.move_from(_new(*args, signature=signature))
    methods['__init__'] = __init__

    for k, v in methods.items():
        old = props.get(k)
        log.info("deriving method '%s.%s.%s' from %s", pkg, name, k, repr(old))
        props[k] = dispatch(v, old, globalns)

    cls = type(name, bases, props)
    log.info("rendering class '%s.%s'", mod.__name__, name)
    setattr(mod, name, cls)
    return cls

################################################################################

def discard_parameter(sig, key):
    return inspect.Signature([v for v in sig.parameters.values() if v.name != key],
            return_annotation=sig.return_annotation)

################################################################################

def eval_type(type, globalns={}, localns={}):
    try:
        return type._eval_type(globalns, localns)
    except AttributeError:
        return type

################################################################################

def dispatch(fun, old, globalns={}, localns={}):
    '''
    Replace a declared function's contents with the one from the document
    - If the declared function takes '_out', call the document function and pass its
    output to the declared function as '_out', with the other arguments filled appropriately
    - Otherwise if the declared function takes '_fun', call the declared function
    with the document function passed as '_fun' and the other arguments filled appropiately
    - Otherwise, call the document function
    '''
    if old is None:
        def bound(*args, _orig=fun):
            return _orig(*args)
        return functools.update_wrapper(bound, _wrapped)
    elif isinstance(old, property):
        return property(dispatch(fun, old.fget))
    sig = inspect.signature(old)
    if '_fun' in sig.parameters:
        assert '_out' not in sig.parameters
        sig = discard_parameter(sig, '_fun')
        def bound(*args, _orig=fun, _bind=sig.bind, _old=old, gil=None, signature=None, **kwargs):
            bound = _bind(*args, **kwargs)
            bound.apply_defaults()
            return _old(*bound.args, _fun=_orig)
    elif '_out' in sig.parameters:
        assert '_fun' not in sig.parameters
        sig = discard_parameter(sig, '_out')
        def bound(*args, _orig=fun, _bind=sig.bind, _old=old, gil=None, **kwargs):
            args = _bind(*args, **kwargs).args
            return _old(*args, _out=_orig(*args))
    elif sig.return_annotation is inspect._empty:
        def bound(*args, _orig=fun, _bind=sig.bind, gil=None, **kwargs):
            return _orig(*_bind(*args, **kwargs).args) or None
    else:
        ret = eval_type(typing._type_check(sig.return_annotation, 'expected type'), globalns, localns)
        def bound(*args, _orig=fun, _bind=sig.bind, _return=ret, gil=None, **kwargs):
            # print(_orig, sig, args, kwargs)
            out = _orig(*_bind(*args, **kwargs).args)
            # print('requesting return type', out.type(), _return, type(_orig))
            # print(_return, type(_return), out.type())
            return out.request(_return)
    return functools.update_wrapper(bound, old)

################################################################################

def set_global_object(pkg, key, value, lookup={}):
    '''put in the module level functions and objects'''
    mod, key = split_module(pkg, key)
    log.info("rendering object '%s.%s'", mod.__name__, key)
    globalns = mod.__dict__.copy()
    globalns.update(lookup)

    old = getattr(mod, key, None)
    if old is None:
        pass
    elif callable(value):#isinstance(value, types.FunctionType):
        log.info("deriving function '%s.%s' from %s", mod.__name__, key, repr(old))
        assert callable(old), 'expected annotation to be a function'
        value = dispatch(value, old, globalns)
    else:
        log.info("deriving object '%s.%s' from %s", mod.__name__, key, repr(old))
        assert isinstance(old, type), 'expected annotation to be a type'
        value = value.request(eval_type(old, globalns))

    setattr(mod, key, value)
    return value

################################################################################
