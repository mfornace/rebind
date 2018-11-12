import inspect, importlib, functools, logging, typing, atexit
from . import common

log = logging.getLogger(__name__)
logging.basicConfig(level=logging.INFO)

################################################################################

def render_module(pkg: str, doc: dict):
    log.info('rendering document into module %s', repr(pkg))
    out = doc.copy()

    out['types'] = {k: v for k, v in doc['contents'] if isinstance(v, tuple)}
    for k, (meth, data) in out['types'].items():
        cls = render_type(pkg, (doc['Variable'],), k, dict(meth), out)
        cls.metadata = {k: v or None for k, v in data}

    names = tuple((o[0], k) for k, v in out['types'].items() for o in v[1])
    # out['set_type_names'](names)

    out['objects'] = {k: render_object(pkg, k, v, out)
        for k, v in doc['contents'] if not isinstance(v, tuple)}

    out['scalars'] = common.find_scalars(doc['scalars'])

    mod = importlib.import_module(pkg)
    for k, v in out.items():
        setattr(mod, k, v)
    log.info('finished rendering document into module %s', repr(pkg))

    atexit.register(common.finalize, doc['_finalize'], log)
    return out

################################################################################

def render_init(init):
    if init is None:
        def __init__(self):
            raise TypeError('No __init__ is possible')
    else:
        def __init__(self, *args, _new=init, signature=None):
            self.assign(_new(*args, signature=signature))
    return __init__

################################################################################

def render_member(key, value, old, globalns):
    def fget(self, _old=value, _return=old):
        out = _old(self)
        try:
            if _return is not None:
                ret = common.eval_type(typing._type_check(_return, 'expected type'), globalns, {})
                out = out.request(ret)
            return out._set_ward(self)
        except AttributeError:
            return out

    def fset(self, other, _old=value, _return=old):
        ref = _old(self)
        ref.assign(other)

    return property(fget=fget, fset=fset, doc='C++ member of type {}'.format(old))

################################################################################

def render_type(pkg: str, bases: tuple, name: str, methods, lookup={}):
    '''Define a new type in pkg'''
    mod, name = common.split_module(pkg, name)
    globalns = mod.__dict__.copy()
    globalns.update(lookup)
    try:
        props = dict(getattr(mod, name).__dict__)
    except AttributeError:
        props = {'__module__': mod.__name__}

    for k, v in common.translations.items():
        try:
            props[v] = props.pop(k)
        except KeyError:
            pass

    methods['__init__'] = render_init(methods.pop('new', None))

    for k, v in methods.items():
        if k.startswith('.'):
            old = props.get('__annotations__', {}).get(k[1:])
            log.info("deriving member '%s.%s%s' from %s", pkg, name, k, repr(old))
            props[k[1:]] = render_member(k[1:], v, old, globalns)
        else:
            old = props.get(k)
            log.info("deriving method '%s.%s.%s' from %s", pkg, name, k, repr(old))
            props[k] = render_function(v, old, globalns)

    cls = type(name, bases, props)
    log.info("rendering class '%s.%s'", mod.__name__, name)
    setattr(mod, name, cls)
    return cls

################################################################################

def render_object(pkg, key, value, lookup={}):
    '''put in the module level functions and objects'''
    mod, key = common.split_module(pkg, key)
    log.info("rendering object '%s.%s'", mod.__name__, key)
    globalns = mod.__dict__.copy()
    globalns.update(lookup)

    old = getattr(mod, key, None)
    if old is None:
        pass
    elif callable(value):
        log.info("deriving function '%s.%s' from %s", mod.__name__, key, repr(old))
        assert callable(old), 'expected annotation to be a function'
        value = render_function(value, old, globalns)
    else:
        log.info("deriving object '%s.%s' from %s", mod.__name__, key, repr(old))
        if not isinstance(old, type):
            print(value.type())
            raise TypeError('expected placeholder {} to be a type'.format(old))
        value = value.request(common.eval_type(old, globalns))

    setattr(mod, key, value)
    return value

################################################################################

def render_function(fun, old, globalns={}, localns={}):
    '''
    Replace a declared function's contents with the one from the document
    - Otherwise if the declared function takes 'function', call the declared function
    with the document function passed as 'function' and the other arguments filled appropiately
    - Otherwise, call the document function
    '''
    if old is None:
        def bound(*args, _orig=fun):
            return _orig(*args)
        return functools.update_wrapper(bound, common.opaque_signature)

    if isinstance(old, property):
        return property(render_function(fun, old.fget))

    sig = inspect.signature(old)
    ret = sig.return_annotation
    if ret is not inspect._empty:
        ret = typing._type_check(ret, 'expected type')
        ret = common.eval_type(ret, globalns, localns)

    if '_old' in sig.parameters:
        raise ValueError('Function {} was already wrapped'.format(fun))

    if '_fun_' in sig.parameters:
        sig = common.discard_parameter(sig, '_fun_')
        def wrap(*args, _orig=fun, _bind=sig.bind, _old=old, _return=ret, gil=None, signature=None, **kwargs):
            bound = _bind(*args, **kwargs)
            bound.apply_defaults()
            out = _old(*bound.args, _fun_=_orig)
            return out if _return is inspect._empty else out.request(_return)
    else:
        def wrap(*args, _orig=fun, _bind=sig.bind, _return=ret, gil=None, signature=None, **kwargs):
            bound = _bind(*args, **kwargs)
            bound.apply_defaults()
            print(sig)
            print(bound)
            out = _orig(*bound.args)
            return (out or None) if ret is inspect._empty else out.request(_return)

    return functools.update_wrapper(wrap, old)
