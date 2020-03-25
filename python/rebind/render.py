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

        return render_cast_method(obj, key, {})

    # def property(self, obj, key=None):
    #     return property(render_cast_method(fun, obj, namespace))

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
        return render_cast_function(obj, impl, {})


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

def cast_return_type(obj, namespace):
    sig = inspect.signature(obj)
    ret = sig.return_annotation

    if isinstance(ret, str):
        try:
            ret = eval(ret, {}, namespace)
        except NameError:
            log.info('return annotation %r cannot be resolved yet', ret)

    for k, p in sig.parameters.items():
        if p.kind == p.VAR_KEYWORD or p.kind == p.VAR_POSITIONAL:
            raise TypeError('Parameter {} cannot be variadic (e.g. like *args or **kwargs)'.format(k))

    return sig, ret

################################################################################

def render_cast_function(obj, impl, namespace):
    sig, ret = cast_return_type(obj, namespace)
    log.info('defining cast function %r: %r -> %r', obj, impl, ret)

    if ret is inspect.Parameter.empty:
        def wrap(*args, _F=impl, _B=sig.bind, gil=None, **kwargs):
            bound = _B(*args, **kwargs)
            bound.apply_defaults()
            return _F(*bound.arguments.values())

    elif ret is None or ret is type(None):
        def wrap(*args, _F=impl, _B=sig.bind, gil=None, **kwargs):
            bound = _B(*args, **kwargs)
            bound.apply_defaults()
            _F(*bound.arguments.values())

    elif isinstance(ret, str):
        def wrap(*args, _F=impl, _B=sig.bind, _R=ret, _N=namespace, gil=None, **kwargs):
            return_type = eval(_R, {}, _N)
            bound = _B(*args, **kwargs)
            bound.apply_defaults()
            out = _F(*bound.arguments.values())
            if out is None:
                raise TypeError('Expected {} but was returned object None'.format(_R))
            return out.cast(return_type)

    else:
        def wrap(*args, _F=impl, _B=sig.bind, _R=ret, gil=None, **kwargs):
            bound = _B(*args, **kwargs)
            bound.apply_defaults()
            out = _F(*bound.arguments.values())
            if out is None:
                raise TypeError('Expected {} but was returned object None'.format(_R))
            return out.cast(_R)

    return functools.update_wrapper(wrap, obj)

################################################################################

def render_cast_method(obj, key, namespace):
    sig, ret = cast_return_type(obj, namespace)
    log.info('defining cast method %r: %r -> %r', obj, key, ret)

    if ret is inspect.Parameter.empty:
        def wrap(self, *args, _K=key, _B=sig.bind, gil=None, **kwargs):
            bound = _B(*args, **kwargs)
            bound.apply_defaults()
            return self.call_method(_K, *bound.arguments.values())

    elif ret is None or ret is type(None):
        def wrap(self, *args, _K=key, _B=sig.bind, gil=None, **kwargs):
            bound = _B(*args, **kwargs)
            bound.apply_defaults()
            self.call_method(_K, *bound.arguments.values())

    elif isinstance(ret, str):
        def wrap(self, *args, _K=key, _B=sig.bind, _R=ret, _N=namespace, gil=None, **kwargs):
            return_type = eval(_R, {}, _N)
            bound = _B(*args, **kwargs)
            bound.apply_defaults()
            out = self.call_method(_K, *bound.arguments.values())
            if out is None:
                raise TypeError('Expected {} but was returned object None'.format(_R))
            return out.cast(return_type)

    else:
        def wrap(self, *args, _K=key, _B=sig.bind, _R=ret, gil=None, **kwargs):
            bound = _B(*args, **kwargs)
            bound.apply_defaults()
            out = self.call_method(_K, *bound.arguments.values())
            if out is None:
                raise TypeError('Expected {} but was returned object None'.format(_R))
            return out.cast(_R)

    return functools.update_wrapper(wrap, obj)

################################################################################

def render_delegating_function(obj, impl, namespace):
    def wrap(*args, _F=obj, **kwargs):
        return _F(*args, **kwargs, _fun_=_F)

    return functools.update_wrapper(wrap, obj)

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
