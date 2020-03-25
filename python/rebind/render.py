from types import FunctionType

import inspect, importlib, functools, logging, typing, atexit, collections
from . import common, ConversionError

################################################################################

log = logging.getLogger(__name__)

DUMMY = False

################################################################################

def set_logger(logger):
    global log
    log = logger

################################################################################

class Config:
    def __init__(self, methods):
        self._set_debug = methods['set_debug']
        self._get_debug = methods['debug']
        self.set_type_error = methods['set_type_error']
        self.set_type = methods['set_type']
        self.set_output_conversion = methods['set_output_conversion']
        self.set_input_conversion = methods['set_input_conversion']
        self.set_translation = methods['set_translation']

    @property
    def debug(self):
        return self._get_debug().cast(bool)

    @debug.setter
    def debug(self, value):
        self._set_debug(bool(value))

################################################################################

class Schema:

    def __init__(self, module_name, obj):
        self.clear = obj['clear_global_objects']
        atexit.register(common.finalize, self.clear, log)

        self.config = Config(obj)
        self.config.set_type_error(ConversionError)

        self.module_name = str(module_name)
        self.Value = obj['Value']
        self.Ref = obj['Ref']
        self.contents = dict(obj['contents'])
        self.scalars = common.find_scalars(obj['scalars'])
        # self.classes = set()
        # self.modules = set()


    def __getitem__(self, key):
        return self.contents[key]


    def type(self, cls):
        mod, name = cls.__module__, cls.__name__
        source = self.contents[name]
        # mod, name = common.split_module(self.module_name, name)
        log.info("rendering class '%s.%s'", mod, name)

        props = class_properties(cls)

        assert '__new__' not in props
        props.setdefault('__init__', no_init)
        props.setdefault('indices', tuple(d for d, _ in source))

        for k, v in props['__annotations__'].items():
            log.info("deriving member '%s.%s.%s' from %r", mod, name, k, v)
            props[k] = render_member(self.Ref, key=k, cast=v)

        ref_props = props.copy()
        props.setdefault('copy', copy)
        props['Ref'] = type('Ref', (self.Ref,), ref_props)

        cls = type(name, (self.Value,), props)

        log.info("rendered class '%s.%s'", mod, name)
        return cls


    def init(self, obj, key=None):
        if isinstance(obj, str):
            return lambda o: self.init(o, obj)
        if key is None:
            key = obj.__qualname__
        log.info('rendering init %r: %r', key, obj)

        key = '.'.join(key.split('.')[:-1] + ['new'])
        impl = self.contents[key]
        def __init__(self, *args, _impl=impl, return_type=None, signature=None):
            _impl(*args, output=self)

        return __init__


    def method(self, obj, key=None):
        if isinstance(obj, str):
            return lambda o: self.method(o, obj)
        if key is None:
            key = obj.__name__
        log.info('rendering method %r: %r', key, obj)

        key = common.translations.get(key, key)

        def impl(self, *args, **kwargs):
            print(repr(key))
            return self.call_method(key, *args, **kwargs)

        return functools.update_wrapper(impl, obj)


    def function(self, obj, key=None):
        if isinstance(obj, str):
            return lambda o: self.function(o, obj)
        if key is None:
            key = obj.__name__
        log.info('rendering function %r: %r', key, obj)

        try:
            impl = self.contents[key]
        except KeyError:
            log.warning('undefined function %r', key)
            return obj
        return render_function(impl, obj, {})


    def object(self, key, cast=None):
        log.info('rendering init %r: %r', key, cast)

        try:
            impl = self.contents[key]
        except KeyError:
            log.warning('undefined object %r', key)
            return None

        return render_object(impl, cast)

################################################################################

def no_init(self):
    raise TypeError('No __init__ is possible since no constructor was declared')

################################################################################

def copy(self):
    '''Make a copy of self using the C++ copy constructor'''
    other = self.__new__(type(self))
    other.copy_from(self)
    return other

################################################################################

def render_function(fun, old, namespace):
    '''
    Replace a declared function's contents with the one from the schema
    - Otherwise if the declared function takes 'function', call the declared function
    with the schema function passed as 'function' and the other arguments filled appropiately
    - Otherwise, call the schema function
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

################################################################################

def render_member(cls, key, cast):
    key = '.' + key

    if cast is None:
        def fget(self, _K=key, _P=cls):
            ptr = _P()
            self.call_method(_K, output=ptr)
            ptr._set_ward(self)
            return ptr
    else:
        def fget(self, _K=key, _R=cast, _P=cls):
            ptr = _P()
            self.call_method(_K, output=ptr)
            ptr._set_ward(self) # not sure if needed...?
            return ptr.cast(_R)

    def fset(self, other, _K=key, _P=cls):
        ptr = _P()
        self.call_method(_K, output=ptr)
        ptr.copy_from(other)

    name = getattr(cast, '__name__', cast)

    doc = 'Member variable' if name is None else 'Member of type `{}`'.format(name)
    return property(fget=fget, fset=fset, doc=doc)

################################################################################

def class_properties(cls):
    '''Find an already declared class and return its deduced properties'''
    annotations = {}
    props = {'__annotations__': annotations}

    for c in reversed(cls.mro()[:-1]): # skip base class 'object'
        props.update(c.__dict__)
        annotations.update(getattr(c, '__annotations__', {}))

    return props

################################################################################

def render_object(impl, decltype=None):
    '''put in the module level functions and objects'''
    if decltype is None:
        return impl
    elif not isinstance(decltype, type):
        raise TypeError('expected placeholder %r to be a type (original=%r)' % (decltype, impl.type()))
    else:
        return impl.cast(decltype)

################################################################################

def make_callback(origin, types):
    '''Make a callback that calls the wrapped one with arguments casted to the given types'''
    if not callable(origin):
        return origin
    def callback(*args):
        return origin(*(a.cast(t) for a, t in zip(args, types)))
    return callback

################################################################################

def is_callable_type(t):
    '''Detect whether a parameter to a C++ function is a callback'''
    t = getattr(t, '__origin__', None)
    return t is typing.Callable or t is collections.abc.Callable

################################################################################

# def _render_module(record, schema):
    # config, out = Config(schema), schema.copy()
    # log.info('setting type error')
    # config.set_type_error(ConversionError)

    # log.info('rendering classes and methods')
    # out['types'] = {k: v for k, v in schema['contents'] if isinstance(v, tuple)}
    # log.info(str(out['types']))

    # for name, overloads in out['types'].items():
    #     mod, cls = render_type(record, name, overloads)
    #     record.modules.add(mod)
    #     record.classes.add(cls)
        # cls._metadata_ = {k: v or None for k, v in data}
    # for index, _ in overloads:
    #     config.set_type(index, cls, cls.Reference)

    # render global objects (including free functions)
    # out['objects'] = {k: render_object(record, k, v)
    #     for k, v in schema['contents'] if not isinstance(v, tuple)}
    # return out, config

################################################################################

# def monkey_patch(record, config):
#     # Monkey-patch modules based on redefined types (takes care of simple cases at least)
#     mod = importlib.import_module(record.module_name)

#     for mod in tuple(record.modules):
#         parts = mod.__name__.split('.')
#         for i in range(1, len(parts)):
#             try:
#                 record.modules.add(importlib.import_module('.'.join(parts[:i])))
#             except ImportError:
#                 pass

#     for mod in record.modules.union(record.classes):
#         log.info('rendering monkey-patching namespace {}'.format(mod))
#         for k in dir(mod):
#             try:
#                 setattr(mod, k, record.translations[getattr(mod, k)])
#                 log.info('monkey-patching namespace {} attribute {}'.format(mod, k))
#             except (TypeError, KeyError):
#                 pass

#     for k, v in record.translations.items():
#         if isinstance(k, type) and isinstance(v, type):
#             config.set_translation(k, v)

#     for k in _declared_objects.difference(record.translations):
#         log.warning('Placeholder {} was declared but not defined'.format(k))

#     log.info('finished rendering schema into module %r', record.module_name)