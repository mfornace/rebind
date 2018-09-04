import sys, types, importlib, functools, inspect
from .cpp import document

Value = document['Value']
TypeIndex = document['TypeIndex']
set_type_names = document['set_type_names']

class Function(document['Function']):
    def __call__(self, *args):
        return convert_output(super().__call__(*args))

################################################################################

def convert_output(obj):
    if callable(obj):
        return Function().move_from(obj)
    if isinstance(obj, tuple):
        return tuple(map(convert_output, obj))
    try:
        t = CLASSES[obj.type()]
        return t.__new__(t).move_from(obj)
    except (KeyError, AttributeError):
        return obj

################################################################################

def collapse_overloads(items):
    out = {}
    for k, v in items:
        out.setdefault(k, []).append(v)
    return out

def overload(v):
    if all(map(callable, v)):
        if len(v) == 1:
            def call(*args, _cpy_dispatch=v[0], **kwargs):
                return convert_output(_cpy_dispatch(*args, **kwargs))
            return call
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
                return convert_output(out)
            return call
    else:
        assert len(v) == 1, 'Cannot overload objects which are not functions'
        return v[0]

################################################################################

def set_global_type(name, methods):
    scope, name = ('package.' + name).rsplit('.', maxsplit=1)
    mod = importlib.import_module(scope)
    try:
        props = dict(getattr(mod, name).__dict__)
    except AttributeError:
        props = {'__module__': scope}
    methods = collapse_overloads(methods)
    props.update(dict(zip(methods.keys(), map(overload, methods.values()))))

    put = props.pop('{}', None)
    if put is not None:
        props['__str__'] = put

    new = props.pop('new', None)
    if new is not None:
        def __init__(self, *args, **kwargs):
            self.move_from(new(*args, **kwargs))
        props['__init__'] = __init__

    cls = type(name, (Value,), props)
    print('new class:', scope, name)
    setattr(mod, name, cls)
    return cls

################################################################################

def dispatch(fun, old):
    if old is None:
        return fun
    else:
        sig = inspect.signature(old)
        def bound(*args, _orig=fun, _bind=sig.bind, **kwargs):
            return _orig(*_bind(*args, **kwargs).args)
        return functools.update_wrapper(bound, old)

def set_global_object(k, v):
    print('new', k, v)
    # put in the module level functions and objects
    mod, key = ('package.' + k).rsplit('.', maxsplit=1)
    mod = importlib.import_module(mod)
    old = getattr(mod, key, None)
    if isinstance(v, types.FunctionType):
        v = dispatch(v, old)
    else:
        print(k, v, old)
        print(v.type())
        assert old is None
    print('new object:', mod.__name__, key, v)
    setattr(mod, key, v)
    return v

################################################################################

TYPES = document['types']
CLASSES = dict(t for t in TYPES.items() if t[1][0])
CLASSES = dict(zip(CLASSES.keys(), map(set_global_type, *zip(*CLASSES.values()))))

OBJECTS = collapse_overloads(document['objects'])
OBJECTS = dict(zip(OBJECTS.keys(), map(overload, OBJECTS.values())))
OBJECTS = dict(zip(OBJECTS.keys(), map(set_global_object, *zip(*OBJECTS.items()))))
print('DONE!')

################################################################################
