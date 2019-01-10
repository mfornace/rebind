import inspect, importlib, functools, logging, typing, atexit
from . import common

log = logging.getLogger(__name__)
logging.basicConfig(level=logging.INFO)

################################################################################

def render_module(pkg: str, doc: dict):
    log.info('rendering document into module %s', repr(pkg))
    out = doc.copy()
    modules, translate = set(), {}

    out['types'] = {k: v for k, v in doc['contents'] if isinstance(v, tuple)}
    for k, (meth, data) in out['types'].items():
        mod, old, cls = render_type(pkg, (doc['Variable'],), k, dict(meth), {})
        if old is not None:
            translate[old] = cls
        modules.add(mod)
        cls.metadata = {k: v or None for k, v in data}
        for k, v in data:
            doc['set_type'](k, cls)

    names = tuple((o[0], k) for k, v in out['types'].items() for o in v[1])
    # out['set_type_names'](names)

    out['objects'] = {k: render_object(pkg, k, v, out)
        for k, v in doc['contents'] if not isinstance(v, tuple)}

    out['scalars'] = common.find_scalars(doc['scalars'])

    mod = importlib.import_module(pkg)
    for k, v in out.items():
        setattr(mod, k, v)
    log.info('finished rendering document into module %s', repr(pkg))

    for k, v in translate.items():
        assert isinstance(k, type), k
        assert isinstance(v, type), v
        print(k, v)

    for mod in modules:
        for k in dir(mod):
            if isinstance(getattr(mod, k), type):
                print(mod, k, getattr(mod, k) in translate)
                try:
                    setattr(mod, k, translate[getattr(mod, k)])
                    print('GOOD')
                except (TypeError, KeyError):
                    pass

    atexit.register(common.finalize, doc['_finalize'], log)
    return out

################################################################################

def render_init(init):
    if init is None:
        def __init__(self):
            raise TypeError('No __init__ is possible')
    else:
        def __init__(self, *args, _new=init, return_type=None, signature=None):
            self.assign(_new(*args, return_type=return_type, signature=signature))
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

def copy(self):
    other = self.__new__(type(self))
    other.copy_from(self)
    return other

################################################################################

def render_type(pkg: str, bases: tuple, name: str, methods, lookup={}):
    '''Define a new type in pkg'''
    mod, name = common.split_module(pkg, name)
    assert not lookup
    globalns = mod.__dict__#.copy()
    # globalns.update(lookup)
    try:
        old_cls = getattr(mod, name)
        props = dict(old_cls.__dict__)
    except AttributeError:
        old_cls = None
        props = {'__module__': mod.__name__}

    methods['__init__'] = render_init(methods.pop('new', None))

    for k, v in methods.items():
        k = common.translations.get(k, k)
        if k.startswith('.'):
            old = props.get('__annotations__', {}).get(k[1:])
            log.info("deriving member '%s.%s%s' from %s", mod.__name__, name, k, repr(old))
            props[k[1:]] = render_member(k[1:], v, old, globalns)
        else:
            old = props.get(k, common.default_methods.get(k))
            log.info("deriving method '%s.%s.%s' from %s", mod.__name__, name, k, repr(old))
            props[k] = render_function(v, old, globalns)

    props.setdefault('copy', copy)

    cls = type(name, bases, props)
    log.info("rendering class '%s.%s'", mod.__name__, name)
    setattr(mod, name, cls)
    return mod, old_cls, cls

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

def render_callback(_orig, _types):
    def callback(*args):
        return _orig(*(a.request(t) for a, t in zip(args, _types)))
    return callback

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

    ev = lambda t: common.eval_type(t, globalns, localns)

    sig = inspect.signature(old)

    has_fun = '_fun_' in sig.parameters
    if has_fun:
        sig = common.discard_parameter(sig, '_fun_')

    types = [p.annotation for p in sig.parameters.values()]
    types = [tuple(map(ev, p.__args__[:-1])) if isinstance(p, typing.CallableMeta) else ev(p) for p in types]

    if '_old' in sig.parameters:
        raise ValueError('Function {} was already wrapped'.format(fun))

    if has_fun:
        def wrap(*args, _orig=fun, _bind=sig.bind, _old=old, gil=None, signature=None, **kwargs):
            bound = _bind(*args, **kwargs)
            bound.apply_defaults()
            args = (a if t is inspect._empty else render_callback(a, t) for a, t in zip(bound.args, types))
            return _old(*args, _fun_=_orig)
    else:
        ret = sig.return_annotation
        if ret is not inspect._empty:
            ret = ev(typing._type_check(ret, 'expected type'))
        def wrap(*args, _orig=fun, _bind=sig.bind, _return=ret, gil=None, signature=None, **kwargs):
            bound = _bind(*args, **kwargs)
            bound.apply_defaults()
            out = _orig(*(render_callback(a, t) if isinstance(t, tuple) else a for a, t in zip(bound.args, types)))
            return (out or None) if ret is inspect._empty else out.request(_return)

    return functools.update_wrapper(wrap, old)
