from .common import events, run_test, find_tests, open_file, ExitStack

################################################################################

def parser(prog='cpy', description='Run C++ unit tests from Python', **kwargs):
    from argparse import ArgumentParser
    out = ArgumentParser(prog=prog, description=description, **kwargs)
    f = out.add_argument
    f('--lib',       '-a', type=str, default='libcpy', help='file path for test library')
    f('--failure',   '-f', action='store_true',  help='show failures')
    f('--success',   '-s', action='store_true',  help='show successes')
    f('--exception', '-e', action='store_true',  help='show exceptions')
    f('--timing',    '-t', action='store_true',  help='show timings')
    f('--list',      '-l', action='store_true',  help='list all test names')
    f('--quiet',     '-q', action='store_true',  help='prevent command line output from cpy')
    f('--capture',   '-c', action='store_true',  help='capture std::cerr and std::cout')
    f('--gil',       '-g', action='store_true',  help='keep Python interpeter lock on')
    f('--jobs',      '-j', type=int, default=1,  help='number of threads')
    f('--params',    '-p', type=str, default='', help='JSON file or string containing test parameters')

    f('--re',        '-r', type=str, default='',       help='include test names matching a given regex')
    f('--out',       '-o', type=str, default='stdout', help='output file')
    f('--out-mode',        type=str, default='w',      help='output file open mode')
    f('--xml',             type=str, default='',       help='XML file path')
    f('--xml-mode',        type=str, default='a+b',    help='XML file open mode')
    f('--suite',           type=str, default='cpy',    help='test suite name (e.g. for XML output)')
    f('--teamcity',        type=str, default='',       help='TeamCity file path')
    f('--json',            type=str, default='',       help='JSON file path')

    f('tests', type=str, default=[], nargs='*', help='test names (if not given, run all tests)')
    return out

################################################################################

def run_index(lib, masks, out, err, gil, cout, cerr, params, i):
    info = lib.test_info(i)
    args = tuple(params.get(info[0], ()))
    test_masks = [(r(i, info), m) for r, m in masks]
    val, counts, o, e = run_test(lib, i, test_masks, args=args, gil=gil, cout=cout, cerr=cerr)
    out.write(o)
    err.write(e)
    for r, _ in test_masks:
        r.finalize(val, counts, o, e)
    return counts

def run_suite(lib, indices, masks, *, gil, cout, cerr, params={}, exe=map):
    from io import StringIO
    from functools import partial

    totals = [0] * len(events())
    out, err = StringIO(), StringIO()
    f = partial(run_index, lib, masks, out, err, gil, cout, cerr, params)
    counts = tuple(exe(f, indices))
    totals = tuple(map(sum, zip(*counts)))

    for r, _ in masks:
        r.finalize(totals, out.getvalue(), err.getvalue())

################################################################################

def import_library(lib):
    import sys, os, importlib
    sys.path.insert(0, os.path.dirname(os.path.abspath(lib)))
    return importlib.import_module(lib)

def load_parameters(params):
    if not params:
        return {}
    elif not isinstance(params, str):
        return dict(params)
    try:
        with open(params) as f:
            import json
            return dict(json.load(f))
    except FileNotFoundError:
        return eval('dict(%s)' % params)

################################################################################

def main(run=run_suite, lib='libcpy', list=False, failure=False, success=False,
    exception=False, timing=False, quiet=False, capture=False, gil=False,
    re='', out='stdout', out_mode='w', xml='', xml_mode='a+b', suite='cpy',
    teamcity='', json='', jobs=1, tests=(), params=''):

    lib = import_library(lib)
    indices = find_tests(lib, tests, re)
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
            r = ConsoleReport(open_file(stack, out, out_mode), info, sync=jobs > 1)
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
            r = NativeReport(open_file(stack, json, 'w'), info, len(indices))
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
