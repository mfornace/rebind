def colored_dummy(s, *args, **kwargs):
    return s

try:
    from termcolor import colored
except ImportError:
    colored = colored_dummy

from contextlib import ExitStack

Events = [
    'Failure',
    'Success',
    'Exception',
    'Timing',
]

ColoredEvents = [
    colored('Failure',   'red'),
    colored('Success',   'green'),
    colored('Exception', 'red'),
    colored('Timing',    'yellow')
]

def events(color=False):
    return ColoredEvents if color else Events

################################################################################

def pop_value(key, keys, values, default=None):
    try:
        idx = keys.index(key)
        keys.pop(idx)
        return values.pop(idx)
    except ValueError:
        return default

################################################################################

class MultiReport:
    def __init__(self, reports):
        self.reports = reports

    def __call__(self, index, scopes, logs):
        for r in self.reports:
            r(index, scopes, logs)

def multireport(*reports):
    if not reports:
        return None
    if len(reports) == 1:
        return reports[0]
    return MultiReport(reports)

################################################################################

def run_test(lib, index, test_masks, *, gil=False, cout=False, cerr=False):
    lists = [[] for _ in events()]
    with ExitStack() as stack:
        for r, mask in test_masks:
            stack.enter_context(r)
            for m, l in zip(mask, lists):
                if m:
                    l.append(r)
        reports = [multireport(*l) for l in lists]
        return lib.run_test(index, reports, (), gil, cout, cerr)

################################################################################

def readable_message(kind, scopes, logs, indent='    ', format_scope=None):
    keys, values = map(list, zip(*logs))
    line, path = (pop_value(k, keys, values) for k in ('line', 'file'))
    scopes = repr('.'.join(scopes)) if format_scope is None else format_scope(scopes)

    # first line
    if path is None:
        s = '{}: {}\n'.format(kind, scopes)
    else:
        desc = '({})'.format(path) if line is None else '({}:{})'.format(path, line)
        s = '{}: {} {}\n'.format(kind, scopes, desc)

    # comments
    while 'comment' in keys:
        s += '{}comment: {}\n'.format(indent, repr(pop_value('comment', keys, values)))

    # comparisons
    comp = ('lhs', 'op', 'rhs')
    while all(c in keys for c in comp):
        lhs, op, rhs = (pop_value(k, keys, values) for k in comp)
        s += indent + 'required: {} {} {}\n'.format(lhs, op, rhs)

    # all other logged keys and values
    for k, v in zip(keys, values):
        if k:
            s += indent + '{}: {}\n'.format(k, repr(v))
        else:
            s += indent + '{}\n'.format(repr(v))

    return s
