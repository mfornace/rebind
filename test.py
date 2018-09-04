from cpy import cli

parser = cli.parser()
parser.add_argument('--time', type=float, default=10.0, help='maximum test time')

args = vars(parser.parse_args())

lib = cli.import_suite(args['lib'])
lib.add_value('max_time', args.pop('time'))

cli.main(**args)
