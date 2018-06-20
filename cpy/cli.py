from .common import events, run_test, ExitStack
from io import StringIO

def go(lib, indices, suite_masks, gil, cout, cerr):
    totals = [0] * len(events())
    stdout, stderr = StringIO(), StringIO()

    for i in indices:
        info = lib.test_info(i)
        test_masks = [(r(i, info), m) for r, m in suite_masks]
        val, counts, out, err = run_test(lib, i, test_masks, args=(), gil=gil, cout=cout, cerr=cerr)
        stdout.write(out)
        stderr.write(err)
        totals = [x + y for x, y in zip(totals, counts)]
        for r, _ in test_masks:
            r.finalize(val, counts, out, err)

    for r, _ in suite_masks:
        r.finalize(totals, stdout.getvalue(), stderr.getvalue())

################################################################################

def parser():
    from argparse import ArgumentParser
    out = ArgumentParser(description='Run unit tests')
    out.add_argument('--lib',         '-a', type=str, default='libcpy', help='file path for test library')
    out.add_argument('--failure',     '-f', action='store_true', help='show failures')
    out.add_argument('--success',     '-s', action='store_true', help='show successes')
    out.add_argument('--exception',   '-e', action='store_true', help='show exceptions')
    out.add_argument('--timing',      '-t', action='store_true', help='show timings')
    out.add_argument('--list',        '-l', action='store_true', help='list all test names')
    out.add_argument('--quiet',       '-q', action='store_true', help='no command line output')
    out.add_argument('--capture',     '-c', action='store_true', help='capture std::cerr and std::cout')
    out.add_argument('--gil',         '-g', action='store_true', help='keep Python interpeter lock on')

    out.add_argument('--regex',       '-r', type=str, default='', help='include test names matching a given regex')
    out.add_argument('--out',         '-o', type=str, default='stdout', help='output file')
    out.add_argument('--out-mode',          type=str, default='w', help='output file open mode')
    out.add_argument('--xml',               type=str, default='', help='XML file path')
    out.add_argument('--xml-mode',          type=str, default='a+b', help='XML file open mode')
    out.add_argument('--suite',             type=str, default='cpy', help='test suite name (e.g. for XML output)')
    out.add_argument('--teamcity',          type=str, default='', help='TeamCity file path')

    out.add_argument('tests', type=str, default=[], nargs='*', help='test names (if not given, run all tests)')
    return out

################################################################################

def find_tests(lib, tests, regex):
    if tests:
        indices = [lib.find_test(t) for t in tests]
    elif not regex:
        indices = list(range(lib.n_tests()))

    if regex:
        import re
        pattern = re.compile(regex)
        indices += [i for i, t in enumerate(lib.test_names()) if pattern.match(t)]

    return sorted(set(indices))

################################################################################

def open_file(stack, name, mode):
    if name in ('stderr', 'stdout'):
        return getattr(sys, name)
    else:
        return stack.enter_context(open(name, mode))

################################################################################

def main(args):
    import sys, os, importlib
    sys.path.insert(0, os.path.dirname(os.path.abspath(args.lib)))
    lib = importlib.import_module(args.lib)

    indices = find_tests(lib, args.tests, args.regex)

    if args.list:
        print('\n'.join(lib.test_info(i)[0] for i in indices))
        return

    mask = (args.failure, args.success, args.exception, args.timing)
    info = lib.compile_info()


    with ExitStack() as stack:
        masks = []
        if not args.quiet:
            from .console import ConsoleReport
            r = ConsoleReport(open_file(stack, args.out, args.out_mode), info)
            masks.append((stack.enter_context(r), mask))

        if args.xml:
            from .junit import XMLFileReport
            r = XMLFileReport(open_file(stack, args.xml, args.xml_mode), info, args.suite)
            masks.append((stack.enter_context(r), (1, 0, 1, 0)))

        if args.teamcity:
            from .teamcity import TeamCityReport
            r = TeamCityReport(open_file(stack, args.teamcity, 'w'), info)
            masks.append((stack.enter_context(r), (1, 0, 1, 0)))

        go(lib, indices, masks, gil=args.gil, cout=args.capture, cerr=args.capture)


if __name__ == '__main__':
    import sys
    sys.exit(main(parser().parse_args()))
