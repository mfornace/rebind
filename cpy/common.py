import sys, io

try:
    from contextlib import ExitStack
except ImportError:
    from contextlib2 import ExitStack

try:
    FileError = FileNotFoundError
except NameError:
    FileError = IOError

################################################################################

DELIMITER = '/'

EVENTS = ['Failure', 'Success', 'Exception', 'Timing']

################################################################################

def events():
    return EVENTS

################################################################################

def foreach(function, *args):
    return tuple(map(function, args))

################################################################################

class Report:
    '''Basic interface for a Report object'''
    def __enter__(self):
        return self

    def finalize(self, *args):
        pass

    def __exit__(self, value, cls, traceback):
        pass

################################################################################

def pop_value(key, keys, values, default=None):
    try:
        idx = keys.index(key)
        keys.pop(idx)
        return values.pop(idx)
    except ValueError:
        return default

################################################################################

def import_library(lib, name='libcpy'):
    import os, importlib
    sys.path.insert(0, os.path.dirname(os.path.abspath(lib)))
    try:
        return importlib.import_module(lib)
    except ImportError as e:
        if sys.version_info >= (3, 4):
            spec = importlib.util.find_spec(lib) # Python module has different name than its file name
            spec.name, spec.loader.name = name, name
            return importlib.util.module_from_spec(spec)
        raise e

################################################################################

def open_file(stack, name, mode):
    '''Open file with ExitStack()'''
    if name in ('stderr', 'stdout'):
        return getattr(sys, name)
    else:
        return stack.enter_context(open(name, mode))

################################################################################

def load_parameters(params):
    if not params:
        return {}
    elif not isinstance(params, str):
        return dict(params)
    try:
        with open(params) as f:
            import json
            return dict(json.load(f))
    except FileError:
        return eval(params)

################################################################################

def test_indices(lib, exclude=False, tests=None, regex=''):
    '''
    Return list of indices of tests to run
        exclude: whether to include or exclude the specified tests
        tests: list of manually specified tests
        regex: pattern to specify tests
    '''
    if tests:
        out = set(lib.find_test(t) for t in tests)
    else:
        out = set() if (regex or exclude) else set(range(lib.n_tests()))

    if regex:
        import re
        pattern = re.compile(regex)
        out.update(i for i, t in enumerate(lib.test_names()) if pattern.match(t))

    if exclude:
        out = set(range(lib.n_tests())).difference(out)
    return sorted(out)

################################################################################

def parametrized_indices(lib, indices, params=(None,), default=(None,)):
    '''
    Yield tuple of (index, parameter_pack) for each test to run
        lib: the cpy library object
        indices: the possible indices to yield from
        params: dict or list of specified parameters (e.g. from load_parameters())
    If params is not dict-like, it is assumed to give the default parameters for all tests.
    A valid argument list is either:
        a tuple of arguments
        an index to preregistered arguments
        None, meaning all preregistered arguments
    '''
    names = lib.test_names()
    if not hasattr(params, 'get'):
        params, default = {}, params
    for i in indices:
        ps = list(params.get(names[i], default))
        n = lib.n_parameters(i)
        while None in ps:
            ps.remove(None)
            ps.extend(range(n))
        for p in ps:
            if isinstance(p, int) and p >= n:
                raise IndexError("Parameter pack index {} is out of range for test '{}' (n={})".format(p, names[i], n))
            yield i, p

################################################################################

class MultiReport:
    '''Simple wrapper to call multiple reports from C++ as if they are one'''
    def __init__(self, reports):
        self.reports = reports

    def __call__(self, index, scopes, logs):
        for r in self.reports:
            r(index, scopes, logs)

def multireport(reports):
    '''Wrap multiple reports for C++ to look like they are one'''
    if not reports:
        return None
    if len(reports) == 1:
        return reports[0]
    return MultiReport(reports)

################################################################################

def run_test(lib, index, test_masks, args=(), gil=False, cout=False, cerr=False):
    lists = [[] for _ in EVENTS]

    with ExitStack() as stack:
        for r, mask in test_masks:
            stack.enter_context(r)
            [l.append(r) for m, l in zip(mask, lists) if m]
        reports = tuple(map(multireport, lists))
        return lib.run_test(index, reports, args, gil, cout, cerr)

################################################################################

def readable_header(keys, values, kind, scopes):
    '''Return string with basic event information'''
    scopes = repr(DELIMITER.join(scopes))
    line, path = (pop_value(k, keys, values) for k in ('line', 'file'))
    if path is None:
        return '{}: {}\n'.format(kind, scopes)
    desc = '({})'.format(path) if line is None else '({}:{})'.format(path, line)
    return '{}: {} {}\n'.format(kind, scopes, desc)

################################################################################

OPS = {
    '~~': u'\u2248\u2248'
}

def readable_logs(keys, values, indent):
    '''Return readable string of key value pairs'''
    s = io.StringIO()
    while 'comment' in keys: # comments
        foreach(s.write, indent, 'comment: ', repr(pop_value('comment', keys, values)), '\n')

    comp = ('lhs', 'op', 'rhs') # comparisons
    while all(c in keys for c in comp):
        lhs, op, rhs = (pop_value(k, keys, values) for k in comp)
        foreach(s.write, indent, 'required: {} {} {}\n'.format(lhs, OPS.get(op, op), rhs))

    for k, v in zip(keys, values): # all other logged keys and values
        foreach(s.write, indent, (k + ': ' if k else 'info: '), v, '\n')
    return s.getvalue()

################################################################################

def readable_message(kind, scopes, logs, indent='    '):
    '''Return readable string for a C++ cpy callback'''
    keys, values = map(list, zip(*logs))
    return readable_header(keys, values, kind, scopes) + readable_logs(keys, values, indent)
