from .common import events, run_test, ExitStack
from .console import ConsoleHandler
from .junit import XMLFileHandler
from io import StringIO

def go(lib, indices, suite_masks):
    totals = [0] * len(events())
    stdout, stderr = StringIO(), StringIO()

    for i in indices:
        info = lib.test_info(i)
        test_masks = [(h(i, info), m) for h, m in suite_masks]
        counts, out, err = run_test(lib, i, test_masks)
        stdout.write(out)
        stderr.write(err)
        totals = [x + y for x, y in zip(totals, counts)]
        for h, _ in test_masks:
            h.finalize(counts, out, err)

    for h, _ in suite_masks:
        h.finalize(totals, stdout.getvalue(), stderr.getvalue())

################################################################################

def parser():
    from argparse import ArgumentParser
    out = ArgumentParser(description='Run unit tests')
    out.add_argument('--lib',         '-a', type=str, default='libcpy', help='file path for test library')
    out.add_argument('--list',        '-l', action='store_true', help='list all test names')
    out.add_argument('--quiet',       '-q', action='store_true', help='no command line output')
    out.add_argument('--output',      '-o', type=str, default='stdout', help='output file')
    out.add_argument('--output-mode', '-m', type=str, default='w', help='output file open mode')
    out.add_argument('--failure',     '-f', action='store_true', help='show failures')
    out.add_argument('--success',     '-s', action='store_true', help='show successes')
    out.add_argument('--exception',   '-e', action='store_true', help='show exceptions (also enabled if --failure)')
    out.add_argument('--timing',      '-t', action='store_true', help='show timings')
    out.add_argument('--xml',         '-x', type=str, default='', help='XML file path')
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
    info = lib.compile_info()

    with ExitStack() as stack:
        masks = []
        if not args.quiet:
            if args.output in ('stderr', 'stdout'):
                o = getattr(sys, args.output)
            else:
                o = stack.enter_context(open(args.output, args.output_mode))
            masks.append((stack.enter_context(ConsoleHandler(o, info)), mask))

        if args.xml:
            o = stack.enter_context(open(args.xml, 'wb'))
            masks.append((stack.enter_context(XMLFileHandler(o, info)), mask))

        go(lib, indices, masks)


if __name__ == '__main__':
    import sys
    sys.exit(main(parser().parse_args()))
