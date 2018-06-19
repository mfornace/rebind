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

class MultiHandler:
    def __init__(self, handlers):
        self.handlers = handlers

    def __call__(self, index, scopes, logs):
        for h in self.handlers:
            h(index, scopes, logs)

def multihandler(*handlers):
    if not handlers:
        return None
    if len(handlers) == 1:
        return handlers[0]
    return MultiHandler(handlers)

################################################################################

def run_test(lib, index, test_masks, *, cout=False, cerr=False):
    lists = [[] for _ in events()]
    with ExitStack() as stack:
        for h, mask in test_masks:
            stack.enter_context(h)
            for m, l in zip(mask, lists):
                if m:
                    l.append(h)
        handlers = [multihandler(*l) for l in lists]
        return lib.run_test(index, handlers, (), cout, cerr)

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
