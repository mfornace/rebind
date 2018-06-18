from .common import Events, run_test, ExitStack
from .console import ConsoleHandler

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
    out.add_argument('--lib',         '-a', type=str, default='libcpy', help='file path for test library')
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
