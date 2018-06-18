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

class ConsoleHandler:
    def __init__(self, file, info, **kwargs):
        self.file = file
        self.file.write('Compiler info: {} ({}, {})\n'.format(*info))
        self.kwargs = kwargs

    def __call__(self, index, info):
        return ConsoleTestHandler(index, info, self.file, **self.kwargs)

    def __enter__(self):
        return self

    def finalize(self, counts):
        s = '=' * 80 + '\nTotal counts:\n'

        spacing = max(map(len, Events))
        for e, c in zip(Events, counts):
            s += '    {} {}\n'.format(e.ljust(spacing), c)
        self.file.write(s)

    def __exit__(self, value, cls, traceback):
        self.file.write('=' * 80 + '\n')

################################################################################

class ConsoleTestHandler:
    def __init__(self, index, info, file, footer='\n', indent='    ', format_scope=None):
        if format_scope is None:
            self.format_scope = lambda s: repr('.'.join(s))
        else:
            self.format_scope = format_scope
        self.footer = footer
        self.indent = indent
        self.index = index
        self.info = info
        self.file = file

    def __call__(self, event, scopes, logs):
        keys, values = map(list, zip(*logs))
        line, path = (find(k, keys, values) for k in ('line', 'file'))
        scopes = self.format_scope(scopes)

        # first line
        if path is None:
            s = '{} {}\n'.format(Events[event], scopes)
        else:
            desc = '({})'.format(path) if line is None else '({}:{})'.format(path, line)
            s = '{} {} {}\n'.format(Events[event], scopes, desc)

        # comments
        while 'comment' in keys:
            s += '{}comment: {}\n'.format(self.indent, repr(find('comment', keys, values)))

        # comparisons
        comp = ('lhs', 'op', 'rhs')
        while all(c in keys for c in comp):
            lhs, op, rhs = (find(k, keys, values) for k in comp)
            s += self.indent + 'required: {} {} {}\n'.format(lhs, op, rhs)

        # all other logged keys and values
        for k, v in zip(keys, values):
            if k:
                s += self.indent + '{}: {}\n'.format(k, repr(v))
            else:
                s += self.indent + '{}\n'.format(repr(v))

        s += self.footer
        self.file.write(s)
        self.file.flush()

    def finalize(self, counts):
        if any(counts):
            s = ', '.join('%s %d' % (e, c) for e, c in zip(Events, counts) if c)
            self.file.write('Test counts: {%s}\n' % s)

    def __enter__(self):
        if self.info[1]:
            info = repr(self.info[0]) + ' (%s:%d): ' % self.info[1:3] + repr(self.info[3])
        else:
            info = repr(self.info[0])
        self.file.write('=' * 80 +
            colored('\nTest %d: ' % self.index, 'blue', attrs=['bold']) + info + '\n\n')
        self.file.flush()
        return self

    def __exit__(self, value, cls, traceback):
        pass

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

def go(lib, file, indices, suite_masks):
    totals = [0] * len(Events)

    for i in indices:
        info = lib.test_info(i)
        with ExitStack() as stack:
            test_masks = [(h(i, info), m) for h, m in suite_masks]
            counts, out, err = run_test(lib, i, test_masks)
            totals = [x + y for x, y in zip(totals, counts)]
            for h, _ in test_masks:
                h.finalize(counts)

    for h, _ in suite_masks:
        h.finalize(totals)

################################################################################

def parser():
    from argparse import ArgumentParser
    out = ArgumentParser(description='Run unit tests')
    out.add_argument('--lib',         '-a', type=str, default='cpy', help='file path for test library')
    out.add_argument('--list',        '-l', action='store_true', help='list all test names')
    out.add_argument('--output',      '-o', type=str, default='stdout', help='output file')
    out.add_argument('--output-mode', '-m', type=str, default='w', help='output file open mode')
    out.add_argument('--failure',     '-f', action='store_true', help='show failures')
    out.add_argument('--success',     '-s', action='store_true', help='show successes')
    out.add_argument('--exception',   '-e', action='store_true', help='show exceptions (also enabled if --failure)')
    out.add_argument('--timing',      '-t', action='store_true', help='show timings')
    out.add_argument('tests', type=str, default=[], nargs='*', help='test names (if not given, run all tests)')
    return out

################################################################################

def main(args):
    import sys, os, importlib
    sys.path.insert(0, os.path.dirname(os.path.abspath(args.lib)))
    lib = importlib.import_module(args.lib)

    args.exception = args.exception or args.timing

    if args.list:
        print(*lib.test_names(), sep='\n')
        return

    if args.tests:
        indices = tuple(lib.find_test(t) for t in args.tests)
    else:
        indices = tuple(range(lib.n_tests()))

    mask = (args.failure, args.success, args.exception, args.timing)

    with ExitStack() as stack:
        if args.output in ('stderr', 'stdout'):
            o = getattr(sys, args.output)
        else:
            o = stack.enter_context(open(args.output, args.output_mode))
        console = stack.enter_context(ConsoleHandler(o, lib.compile_info()))
        go(lib, o, indices, [(console, mask)])


if __name__ == '__main__':
    import sys
    sys.exit(main(parser().parse_args()))
