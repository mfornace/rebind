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

    types = {k: set_global_type(pkg, Function.overload, (doc['Value'],), *v)
        for k, v in doc['types'].items() if v[0]} # ignore empty names
    Function.types = types

    objects = {k: set_global_object(pkg, k, Function.overload(v))
        for k, v in collapse_overloads(doc['objects']).items()}

    out = dict(TypeIndex=doc['TypeIndex'], Value=doc['Value'], Function=Function,
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

def _wrapped(*args, **kwargs):
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
                    except Exception:
                        pass
                else:
                    raise TypeError('No overloads worked')
                return cls.convert(out)
        functools.update_wrapper(call, _wrapped)
        call.__qualname__ = 'dispatched'
        call.__name__ = 'dispatched'
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

def set_global_type(pkg: str, overload, bases: tuple, name: str, methods):
    '''Define a new type in pkg'''
    scope, name = '{}.{}'.format(pkg, name).rsplit('.', maxsplit=1)
    mod = importlib.import_module(scope)
    try:
        props = dict(getattr(mod, name).__dict__)
    except AttributeError:
        props = {'__module__': scope}
    methods = collapse_overloads(methods)
    props.update({k: overload(v) for k, v in methods.items()})

    put = props.pop('{}', None)
    if put is not None:
        props['__str__'] = put

    new = props.pop('new', None)
    if new is not None:
        def __init__(self, *args, **kwargs):
            self.move_from(new(*args, **kwargs))
        props['__init__'] = __init__

    cls = type(name, bases, props)
    log.info('rendering class (scope=%s, name=%s)', repr(scope), repr(name))
    setattr(mod, name, cls)
    return cls

################################################################################

def dispatch(fun, old):
    '''Replace a declared function's contents with the one from the document'''
    if old is None:
        return fun
    else:
        sig = inspect.signature(old)
        def bound(*args, _orig=fun, _bind=sig.bind, **kwargs):
            return _orig(*_bind(*args, **kwargs).args)
        return functools.update_wrapper(bound, old)

def set_global_object(pkg, k, v):
    '''put in the module level functions and objects'''
    mod, key = '{}.{}'.format(pkg, k).rsplit('.', maxsplit=1)
    log.info('rendering object (module=%s, name=%s)', mod, repr(k))
    mod = importlib.import_module(mod)
    old = getattr(mod, key, None)
    if old is not None:
        log.info('replacing object (module=%s, name=%s, old=%s)', mod.__name__, repr(key), repr(old))
    if isinstance(v, types.FunctionType):
        v = dispatch(v, old)
    else:
        assert old is None
    setattr(mod, key, v)
    return v

################################################################################
