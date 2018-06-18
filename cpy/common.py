try:
    from termcolor import colored
except ImportError:
    def colored(s, *args, **kwargs):
        return s

from contextlib import ExitStack

Events = [
    colored('Failure:',   'red'),
    colored('Success:',   'green'),
    colored('Exception:', 'red'),
    colored('Timing:',    'yellow'),
]

################################################################################

def find(key, keys, values, default=None):
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

def run_test(lib, index, test_masks):
    lists = [[] for _ in Events]
    with ExitStack() as stack:
        for h, mask in test_masks:
            stack.enter_context(h)
            for m, l in zip(mask, lists):
                if m:
                    l.append(h)
        handlers = [multihandler(*l) for l in lists]
        return lib.run_test(index, handlers, (), True, True)
