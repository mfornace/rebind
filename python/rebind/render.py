import inspect, importlib, functools, logging, typing, atexit, collections
from . import Config, common, ConversionError

log = logging.getLogger(__name__)
logging.basicConfig(level=logging.INFO)

################################################################################

def render_module(pkg: str, doc: dict, set_type_names=False):
    clear = doc['clear_global_objects']
    try:
        record = Record(pkg, doc['Value'], doc['Pointer'])
        out, config = _render_module(record, doc, set_type_names)
        monkey_patch(record, config)
    except BaseException:
        clear()
        raise
    finally:
        atexit.register(common.finalize, clear, log)

################################################################################

class Record:
    def __init__(self, module_name, Value, Pointer):
        self.module_name = str(module_name)
        self.classes = set()
        self.modules = set()
        self.translations = {}
        self.Value = Value
        self.Pointer = Pointer

    def translate(self, old, new):
        if old is not None:
            self.translations[old] = new
        return new

################################################################################

def _render_module(record, doc, set_type_names):
    log.info('rendering document into module %s', repr(record.module_name))
    config, out = Config(doc), doc.copy()
    log.info('setting type error')
    config.set_type_error(ConversionError)

    log.info('rendering classes and methods')
    out['types'] = {k: v for k, v in doc['contents'] if isinstance(v, tuple)}
    log.info(str(out['types']))

    for name, overloads in out['types'].items():
        mod, cls = render_type(record, name, overloads)
        record.modules.add(mod)
        record.classes.add(cls)
        # cls._metadata_ = {k: v or None for k, v in data}
        for index, _ in overloads:
            config.set_type(index, cls, cls.Reference)

    # put type names if desired
    if set_type_names:
        names = tuple((o[0], k) for k, v in out['types'].items() for o in v[1])
        config.set_type_names(names)

    # render global objects (including free functions)
    out['objects'] = {k: render_object(record, k, v)
        for k, v in doc['contents'] if not isinstance(v, tuple)}

    out['scalars'] = common.find_scalars(doc['scalars'])
    return out, config

################################################################################

def monkey_patch(record, config):
    # Monkey-patch modules based on redefined types (takes care of simple cases at least)
    mod = importlib.import_module(record.module_name)

    for mod in tuple(record.modules):
        parts = mod.__name__.split('.')
        for i in range(1, len(parts)):
            try:
                record.modules.add(importlib.import_module('.'.join(parts[:i])))
            except ImportError:
                pass

    for mod in record.modules.union(record.classes):
        log.info('rendering monkey-patching namespace {}'.format(mod))
        for k in dir(mod):
            try:
                setattr(mod, k, record.translations[getattr(mod, k)])
                log.info('monkey-patching namespace {} attribute {}'.format(mod, k))
            except (TypeError, KeyError):
                pass

    for k, v in record.translations.items():
        if isinstance(k, type) and isinstance(v, type):
            config.set_translation(k, v)

    log.info('finished rendering document into module %r', record.module_name)

################################################################################

def render_init(init):
    if init is None:
        def __init__(self):
            raise TypeError('No __init__ is possible since no C++ constructors were declared')
    else:
        def __init__(self, *args, _new=init, return_type=None, signature=None):
            _new(*args, output=self)
    return __init__

################################################################################

def render_member(record, key, value, old):
    if old is None:
        def fget(self, _impl=value, _P=record.Pointer):
            ptr = _P()
            _impl(self, output=ptr)
            ptr._set_ward(self)
            return ptr
    else:
        def fget(self, _impl=value, _R=old, _P=record.Pointer):
            ptr = _P()
            _impl(self, output=ptr)
            ptr._set_ward(self) # not sure if needed...?
            return ptr.cast(_R)

    def fset(self, other, _impl=value, _P=record.Pointer):
        ptr = _P()
        _impl(self, output=ptr)
        ptr.copy_from(other)

    name = getattr(old, '__name__', old)

    doc = 'C++ member variable' if name is None else 'Member of type `{}`'.format(name)
    return property(fget=fget, fset=fset, doc=doc)

################################################################################

def copy(self):
    '''Make a copy of self using the C++ copy constructor'''
    other = self.__new__(type(self))
    other.copy_from(self)
    return other

################################################################################

def find_class(mod, name):
    '''Find an already declared class and return its deduced properties'''
    old = common.unwrap(getattr(mod, name, None))
    annotations = {}
    props = {'__module__': mod.__name__}

    if old is not None:
        for c in reversed(old.mro()[:-1]): # skip base class 'object'
            props.update(c.__dict__)
            annotations.update(getattr(c, '__annotations__', {}))
    props['__annotations__'] = annotations
    return old, props

################################################################################

def render_type(record, name: str, overloads):
    '''Define a new type in pkg'''
    mod, name = common.split_module(record.module_name, name)
    old_cls, props = find_class(mod, name)

    new = props.pop('__new__', None)
    if callable(new):
        log.warning('{}.{}.__new__ will not be rendered'.format(record.module_name, name))

    data, methods = overloads[0]
    methods = dict(methods)
    methods['__init__'] = render_init(methods.pop('new', None))

    for k, v in methods.items():
        k = common.translations.get(k, k)
        if k.startswith('.'):
            key = k[1:]
            old = common.unwrap(props['__annotations__'].get(key))
            log.info("deriving member '%s.%s%s' from %r", mod.__name__, name, k, old)
            props[key] = render_member(record, key, v, old)
            record.translate('%s.%s' % (name, old), props[key])
        else:
            old = common.unwrap(props.get(k, common.default_methods.get(k)))
            log.info("deriving method '%s.%s.%s' from %r", mod.__name__, name, k, old)
            props[k] = render_function(v, old, namespace=mod.__dict__)
            record.translate(old, props[k])

    props.setdefault('copy', copy)

    ref_props = props.copy()
    props['Reference'] = type('Reference', (record.Pointer,), ref_props)

    cls = type(name, (record.Value,), props)
    record.translate(old_cls, cls)

    log.info("rendering class '%s.%s'", mod.__name__, name)
    setattr(mod, name, cls)

    return mod, cls

################################################################################

def render_object(record, key, value):
    '''put in the module level functions and objects'''
    mod, key = common.split_module(record.module_name, key)
    log.info("rendering object '%s.%s'", mod.__name__, key)

    old = common.unwrap(getattr(mod, key, None))
    if old is None:
        pass
    elif callable(value):
        log.info("deriving function '%s.%s' from %r", mod.__name__, key, old)
        assert callable(old), 'expected annotation to be a function'
        value = render_function(value, old, namespace=mod.__dict__)
    else:
        log.info("deriving object '%s.%s' from %r", mod.__name__, key, old)
        if not isinstance(old, type):
            print(value.type())
            raise TypeError('expected placeholder {} to be a type'.format(old))
        value = value.cast(old)

    setattr(mod, key, value)
    record.translate(old, value)
    return value

################################################################################

def make_callback(origin, types):
    '''Make a callback that calls the wrapped one with arguments casted to the given types'''
    if not callable(origin):
        return origin
    def callback(*args):
        return origin(*(a.cast(t) for a, t in zip(args, types)))
    return callback

def is_callable_type(t):
    '''Detect whether a parameter to a C++ function is a callback'''
    t = getattr(t, '__origin__', None)
    return t is typing.Callable or t is collections.abc.Callable
# if not hasattr(typing, 'CallableMeta'):
#     raise ImportError('Python 3.7 has a bug where typing.CallableMeta is missing')

################################################################################

def render_function(fun, old, namespace):
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
        return property(render_function(fun, old.fget, namespace))

    sig = inspect.signature(old)

    has_fun = '_fun_' in sig.parameters
    if has_fun:
        sig = common.discard_parameter(sig, '_fun_')

    types = tuple(p.annotation.__args__ if is_callable_type(p.annotation) else None for p in sig.parameters.values())
    empty = inspect.Parameter.empty

    if '_old' in sig.parameters:
        raise ValueError('Function {} was already wrapped'.format(old))

    # Eventually all of the computation in wrap() could be moved into C++
    # (1) binding of args and kwargs with default arguments
    # (2) for each arg that is annotated with Callback, make a callback out of it
    # (3) either cast the return type or invoke the fun that is given
    # (4) ...
    if has_fun:
        def wrap(*args, _orig=fun, _bind=sig.bind, _old=old, gil=None, signature=None, **kwargs):
            bound = _bind(*args, **kwargs)
            bound.apply_defaults()
            # Convert args and kwargs separately
            args = (a if t is None or a is None else make_callback(a, t) for a, t in zip(bound.args, types))
            kwargs = {k: (v if t is None or v is None else make_callback(v, t)) for (k, v), t in zip(bound.kwargs.items(), types[len(bound.args):])}
            return _old(*args, _fun_=_orig, **kwargs)
    else:
        ret = sig.return_annotation

        if isinstance(ret, str):
            try:
                ret = eval(ret, {}, namespace)
            except NameError:
                raise NameError('Return annotation %s could not be resolved on function %r' % (ret, old))

        for k, p in sig.parameters.items():
            if p.kind == p.VAR_KEYWORD or p.kind == p.VAR_POSITIONAL:
                raise TypeError('Parameter {} cannot be variadic (e.g. like *args or **kwargs)'.format(k))

        def wrap(*args, _orig=fun, _bind=sig.bind, _return=ret, gil=None, signature=None, **kwargs):
            bound = _bind(*args, **kwargs)
            bound.apply_defaults()
            # Convert any keyword arguments into positional arguments
            out = _orig(*(make_callback(a, t) if t is not None else a for a, t in zip(bound.arguments.values(), types)))
            if _return is empty:
                return out # no cast
            if _return is None or _return is type(None):
                return # return None regardless of output
            if out is None:
                raise TypeError('Expected {} but was returned object None'.format(_return))
            return out.cast(_return)

    return functools.update_wrapper(wrap, old)

