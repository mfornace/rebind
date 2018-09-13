import sys, types, importlib, functools, inspect, logging, enum

log = logging.getLogger(__name__)
logging.basicConfig(level=logging.INFO)

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

def render_module(pkg: str, doc: dict):
    log.info('rendering document into module %s', repr(pkg))

    Function = type('Function', (doc['Function'],), {
        '__call__': _call, '__module__': pkg, 'raw_call': doc['Function'].__call__,
        'convert': classmethod(_convert), 'overload': classmethod(_overload),
    })
    collapsed = collapse_overloads([(name, (k, meth)) for k, (name, meth) in doc['types'].items() if name])
    collapsed = {k: [[i[0] for i in v], [m for i in v for m in i[1]]] for k, v in collapsed.items()}

    types = {}
    for k, (idx, meth) in collapsed.items():
        methods = {n: _overload(Function, m) for n, m in collapse_overloads(meth).items()}
        cls = set_global_type(pkg, (doc['Value'],), k, methods)
        types.update({i: cls for i in idx})

    Function.types = types

    objects = {k: set_global_object(pkg, k, Function.overload(v))
        for k, v in collapse_overloads(doc['objects']).items()}

    find = lambda x, n: next(t for c, t, s in doc['scalars'] if c == x and s == n)
    scalars = {
        'float32': find(Scalar.Float, 32),
        'float64': find(Scalar.Float, 64),
        'uint32': find(Scalar.Unsigned, 32),
        'uint64': find(Scalar.Unsigned, 64),
        'int32': find(Scalar.Signed, 32),
        'int64': find(Scalar.Signed, 64),
    }

    out = dict(TypeIndex=doc['TypeIndex'], Value=doc['Value'], Function=Function, scalars=scalars,
        types=types, objects=objects, set_type_names=Function(doc['set_type_names']))

    mod = importlib.import_module(pkg)
    for k, v in out.items():
        setattr(mod, k, v)
    log.info('finished rendering document into module %s', repr(pkg))

    return out


################################################################################

def _call(self, *args):
    '''Call a function from a cpy.Document and convert its output'''
    return self.convert(super().__call__(*args))

def _convert(cls, obj):
    '''Convert the output of a function from a cpy.Document'''
    if callable(obj):
        return cls().move_from(obj)
    if isinstance(obj, tuple):
        return tuple(map(cls.convert, obj))
    try:
        t = cls.types[obj.type()]
        return t.__new__(t).move_from(obj)
    except (KeyError, AttributeError):
        return obj

def _wrapped(*args):
    '''Call a function from a cpy.Document'''
    pass

def _overload(cls, v):
    if all(map(callable, v)):
        if len(v) == 1:
            def call(*args, _cpy_dispatch=v[0], **kwargs):
                return cls.convert(_cpy_dispatch(*args, **kwargs))
        else:
            def call(*args, _cpy_dispatch=tuple(v), **kwargs):
                for f in _cpy_dispatch:
                    try:
                        out = f(*args, **kwargs)
                        break
                    except Exception as e:
                        print(e)
                else:
                    raise TypeError('No overloads worked')
                return cls.convert(out)
        functools.update_wrapper(call, _wrapped)
        call.__qualname__ = 'cpy.dispatched_%d' % len(v)
        call.__name__ = 'dispatched_%d' % len(v)
        call.__module__ = 'cpy'
        return call
    else:
        assert len(v) == 1, 'Cannot overload objects which are not functions'
        return v[0]

################################################################################

def collapse_overloads(items):
    out = {}
    for k, v in items:
        out.setdefault(k, []).append(v)
    return out

################################################################################

def translate(props, old, new):
    put = props.pop(old, None)
    if put is not None:
        props[new] = put

def set_global_type(pkg: str, bases: tuple, name: str, methods):
    '''Define a new type in pkg'''
    scope, name = '{}.{}'.format(pkg, name).rsplit('.', maxsplit=1)
    mod = importlib.import_module(scope)
    try:
        props = dict(getattr(mod, name).__dict__)
    except AttributeError:
        props = {'__module__': scope}

    translate(methods, '{}', '__str__')
    translate(methods, '[]', '__getitem__')
    translate(methods, '()', '__call__')

    new = methods.pop('new', None)
    if new is None:
        def __init__(self):
            raise TypeError('No __init__ is possible')
    else:
        def __init__(self, *args, _new=new):
            self.move_from(_new(*args))
    methods['__init__'] = __init__

    for k, v in methods.items():
        old = props.get(k)
        props[k] = dispatch(v, old)
        if old is not None:
            log.info("deriving method '%s.%s.%s' from %s", pkg, name, k, repr(old))

    cls = type(name, bases, props)
    log.info("rendering class '%s.%s'", scope, name)
    setattr(mod, name, cls)
    return cls

################################################################################

def discard_parameter(sig, key):
    return inspect.Signature([v for v in sig.parameters.values() if v.name != key],
            return_annotation=sig.return_annotation)

def dispatch(fun, old):
    '''
    Replace a declared function's contents with the one from the document
    - If the declared function takes '_out', call the document function and pass its
    output to the declared function as '_out', with the other arguments filled appropriately
    - Otherwise if the declared function takes '_fun', call the declared function
    with the document function passed as '_fun' and the other arguments filled appropiately
    - Otherwise, call the document function
    '''
    if old is None:
        return fun
    sig = inspect.signature(old)
    if '_fun' in sig.parameters:
        assert '_out' not in sig.parameters
        sig = discard_parameter(sig, '_fun')
        def bound(*args, _orig=fun, _bind=sig.bind, _old=old, gil=None, **kwargs):
            bound = _bind(*args, **kwargs)
            bound.apply_defaults()
            return _old(*bound.args, _fun=_orig)
    elif '_out' in sig.parameters:
        assert '_fun' not in sig.parameters
        sig = discard_parameter(sig, '_out')
        def bound(*args, _orig=fun, _bind=sig.bind, _old=old, gil=None, **kwargs):
            args = _bind(*args, **kwargs).args
            return _old(*args, _out=_orig(*args))
    else:
        def bound(*args, _orig=fun, _bind=sig.bind, gil=None, **kwargs):
            return _orig(*_bind(*args, **kwargs).args)
    return functools.update_wrapper(bound, old)

def set_global_object(pkg, k, v):
    '''put in the module level functions and objects'''
    mod, key = '{}.{}'.format(pkg, k).rsplit('.', maxsplit=1)
    log.info("rendering object '%s.%s'", mod, k)
    mod = importlib.import_module(mod)
    old = getattr(mod, key, None)
    if old is not None:
        log.info("deriving object '%s.%s' from %s", mod.__name__, key, repr(old))
    if isinstance(v, types.FunctionType):
        v = dispatch(v, old)
    else:
        assert old is None
    setattr(mod, key, v)
    return v

################################################################################

try:
    import cxxfilt
    def _demangle(s):
        try:
            out = cxxfilt.demangle(s)
            if out != s:
                return out
        except cxxfilt.InvalidName:
            pass
        return None

    def demangle(s):
        return _demangle(s) or _demangle('_Z' + s) or s

except ImportError:
    def demangle(s):
        return s

    def _demangle(s):
        return s