import inspect, importlib, enum

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

def find_scalars(info):
    find = lambda x, n: next(t for c, t, s in info if c == x and s == n)
    return {'float32': find(Scalar.Float, 32),
            'float64': find(Scalar.Float, 64),
            'uint32':  find(Scalar.Unsigned, 32),
            'uint64':  find(Scalar.Unsigned, 64),
            'int32':   find(Scalar.Signed, 32),
            'int64':   find(Scalar.Signed, 64)}

################################################################################

translations = {
    '{}': '__str__',
    '[]': '__getitem__',
    '()': '__call__',
    '^': '__xor__',
    '+': '__add__',
    '-': '__sub__',
    '/': '__div__',
    '*': '__mul__',
}

################################################################################

def opaque_signature(*args):  # this is just used for its signature
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

def finalize(func, log):
    '''Finalize C++ held Python objects from cpy'''
    log.info('cleaning up Python C++ resources')
    func()

################################################################################

def split_module(pkg, name):
    for i in range(1, 10):
        x, y = '{}.{}'.format(pkg, name).rsplit('.', maxsplit=i)
        try:
            return importlib.import_module(x), y
        except ModuleNotFoundError:
            pass

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