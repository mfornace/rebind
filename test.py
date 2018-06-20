from cpy import cli

parser = cli.parser()
parser.add_argument('--time', type=float, default=1.0, help='maximum test time')

args = vars(parser.parse_args())

lib = cli.import_library(args['lib'])
lib.add_value('allowed_time', args.pop('time'))

cli.main(**args)
