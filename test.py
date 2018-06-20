from cpy import cli

args = cli.parser().parse_args()

def hmm(*args):
    print(args)

lib = cli.import_library('libcpy')
lib.add_test('hmm', hmm)

cli.main(**vars(args))
