from cpy import cli

lib = cli.import_library('libcpy')

def hmm(*args):
    print(args)

lib.add_test('hmm', hmm)

cli.main(**vars(cli.parser().parse_args()))