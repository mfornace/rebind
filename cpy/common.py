def not_colored(s, *args, **kwargs):
    return s

try:
    from termcolor import colored
except ImportError:
    colored = not_colored

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

def run_test(lib, suite, index, test_masks, *, args=(), gil=False, cout=False, cerr=False):
    lists = [[] for _ in events()]
    with ExitStack() as stack:
        for r, mask in test_masks:
            stack.enter_context(r)
            [l.append(r) for m, l in zip(mask, lists) if m]
        reports = [multireport(*l) for l in lists]
        return lib.run_test(suite, index, reports, args, gil, cout, cerr)

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
        s += indent + 'comment: ' + repr(pop_value('comment', keys, values)) + '\n'

    # comparisons
    comp = ('lhs', 'op', 'rhs')
    while all(c in keys for c in comp):
        lhs, op, rhs = (pop_value(k, keys, values) for k in comp)
        s += indent + 'required: {} {} {}\n'.format(lhs, op, rhs)

    # all other logged keys and values
    for k, v in zip(keys, values):
        s += indent + (k + ': ' if k else '') + repr(v) + '\n'

    return s
