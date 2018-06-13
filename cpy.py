try:
    from termcolor import colored
except ImportError:
    def colored(s, *args, **kwargs):
        return s

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
    def __init__(self, file, footer='\n', indent='    ', format_scope=None):
        self.count = 0
        self.footer = footer
        if format_scope is None:
            self.format_scope = lambda s: repr('.'.join(s))
        else:
            self.format_scope = format_scope
        self.file = file
        self.indent = indent

    def __call__(self, event, scopes, logs):
        self.count += 1
        keys, values = map(list, zip(*logs))
        line, file = (find(k, keys, values) for k in ('line', 'file'))

        # first line
        if file is None:
            s = '{} {}\n'.format(Events[event], self.format_scope(scopes))
        else:
            desc = '({})'.format(file) if line is None else '({}:{})'.format(file, line)
            s = '{} {} {}\n'.format(Events[event], self.format_scope(scopes), desc)

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

################################################################################

def go(lib, file, indices, handlers):
    counts = [0] * len(Events)

    for i in indices:
        file.write('=' * 80 +
                   colored('\nTest %d: ' % i, 'blue', attrs=['bold']) +
                   repr(lib.test_name(i)) + '\n\n')
        file.flush()
        counts = [x + y for x, y in zip(counts, lib.run_test(i, handlers))]

    s = '=' * 80 + '\nTotal counts:\n'

    spacing = max(map(len, Events))
    for e, c in zip(Events, counts):
        s += '    {} {}\n'.format(e.ljust(spacing), c)

    file.write(s + '\n')
    file.flush()

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

    flags = (args.failure, args.success, args.exception, args.timing)

    run = lambda o: go(lib, o, indices, [ConsoleHandler(o) if f else None for f in flags])
    if args.output in ('stderr', 'stdout'):
        run(getattr(sys, args.output))
    else:
        with open(args.output, args.output_mode) as o:
            run(o)


if __name__ == '__main__':
    import sys
    sys.exit(main(parser().parse_args()))
