from .common import events, run_test, find_tests, open_file
from .common import ExitStack, import_library, load_parameters

################################################################################

def parser(prog='cpy', description='Run C++ unit tests from Python', **kwargs):
    from argparse import ArgumentParser
    out = ArgumentParser(prog=prog, description=description, **kwargs)
    add = out.add_argument
    add('--lib',       '-a', type=str, default='libcpy', metavar='PATH', help="file path for test library (default 'libcpy')")
    add('--list',      '-l', action='store_true',  help='list all test names')
    add('--failure',   '-f', action='store_true',  help='show failures')
    add('--success',   '-s', action='store_true',  help='show successes')
    add('--exception', '-e', action='store_true',  help='show exceptions')
    add('--timing',    '-t', action='store_true',  help='show timings')
    add('--quiet',     '-q', action='store_true',  help='prevent command line output (from cpy at least)')
    add('--capture',   '-c', action='store_true',  help='capture std::cerr and std::cout')
    add('--gil',       '-g', action='store_true',  help='keep Python interpeter lock on')
    add('--jobs',      '-j', type=int, default=1,  metavar='INT', help='number of threads (default 1)', )
    add('--params',    '-p', type=str,             metavar='STR', help='JSON file path or Python eval-able parameter string')

    f = lambda t, m, *args, **kws: out.add_argument(*args, type=t, metavar=m, **kws)
    f(str, 'RE',   '--regex',  '-r',  help="include test names matching a given regex")
    f(str, 'PATH', '--out',    '-o',  help="output file path (default 'stdout')", default='stdout')
    f(str, 'MODE', '--out-mode',      help="output file open mode (default 'w')", default='w')
    f(str, 'PATH', '--xml',           help="XML file path")
    f(str, 'MODE', '--xml-mode',      help="XML file open mode (default 'a+b')", default='a+b')
    f(str, 'NAME', '--suite',         help="test suite output name (default 'cpy')", default='cpy')
    f(str, 'PATH', '--teamcity',      help="TeamCity file path")
    f(str, 'PATH', '--json',          help="JSON file path")
    f(int, 'INT',  '--json-indent',   help="JSON indentation (default None)")

    add('tests', type=str, nargs='*', help='test names (if not given, run all tests)')
    return out

################################################################################

def run_index(lib, masks, out, err, gil, cout, cerr, params, i):
    info = lib.test_info(i)
    args = tuple(params.get(info[0], ()))
    test_masks = [(r(i, info), m) for r, m in masks]
    val, time, counts, o, e = run_test(lib, i, test_masks, args=args, gil=gil, cout=cout, cerr=cerr)
    out.write(o)
    err.write(e)
    for r, _ in test_masks:
        r.finalize(val, time, counts, o, e)
    return (time,) + counts

def run_suite(lib, indices, masks, *, gil, cout, cerr, params={}, exe=map):
    from io import StringIO
    from functools import partial

    totals = [0] * len(events())
    out, err = StringIO(), StringIO()
    f = partial(run_index, lib, masks, out, err, gil, cout, cerr, params)
    counts = tuple(exe(f, indices))
    totals = tuple(map(sum, zip(*counts)))

    for r, _ in masks:
        r.finalize(totals[0], totals[1:], out.getvalue(), err.getvalue())

################################################################################

def main(run=run_suite, lib='libcpy', list=False, failure=False, success=False,
    exception=False, timing=False, quiet=False, capture=False, gil=False,
    regex=None, out='stdout', out_mode='w', xml=None, xml_mode='a+b', suite='cpy',
    teamcity=None, json=None, json_indent=None, jobs=1, tests=None, params=None):

    lib = import_library(lib)
    indices = find_tests(lib, tests, regex)
    params = load_parameters(params)

    if list:
        print('\n'.join(lib.test_info(i)[0] for i in indices))
        return

    mask = (failure, success, exception, timing)
    info = lib.compile_info()

    with ExitStack() as stack:
        masks = []
        if not quiet:
            from .console import ConsoleReport
            r = ConsoleReport(open_file(stack, out, out_mode), info, timing=timing, sync=jobs > 1)
            masks.append((stack.enter_context(r), mask))

        if xml:
            from .junit import XMLFileReport
            r = XMLFileReport(open_file(stack, xml, xml_mode), info, suite)
            masks.append((stack.enter_context(r), (1, 0, 1))) # failures & exceptions

        if teamcity:
            from .teamcity import TeamCityReport
            r = TeamCityReport(open_file(stack, teamcity, 'w'), info)
            masks.append((stack.enter_context(r), (1, 0, 1))) # failures & exceptions

        if json:
            from .native import NativeReport
            r = NativeReport(open_file(stack, json, 'w'), info, indent=json_indent)
            masks.append((stack.enter_context(r), mask))

        if jobs > 1:
            from multiprocessing.pool import ThreadPool
            exe = ThreadPool().map
        else:
            exe = map

        return run(lib=lib, indices=indices, masks=masks, params=params,
                   gil=gil, cout=capture, cerr=capture, exe=exe)


if __name__ == '__main__':
    import sys
    sys.exit(main(**vars(parser().parse_args())))
