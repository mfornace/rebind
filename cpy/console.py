import io, sys
from .common import Report, readable_message, Event

################################################################################

def not_colored(s, *args, **kwargs):
    return s
try:
    assert sys.stdout.isatty()
    from termcolor import colored
except (ImportError, AssertionError):
    colored = not_colored

################################################################################

STREAM_FOOTER = '_' * 22 + '\n'
FOOTER = '_' * 80 + '\n'

COLORS =  [
    lambda e: colored(e, 'red'),
    lambda e: colored(e, 'green'),
    lambda e: colored(e, 'red'),
    lambda e: colored(e, 'yellow'),
    lambda e: colored(e, 'grey')
]

STDERR = colored('Contents of std::err', 'magenta')
STDOUT = colored('Contents of std::out', 'magenta')
TEST_DURATION = colored('Test duration', 'yellow')
TOTAL_DURATION = colored('Total duration', 'yellow')

################################################################################

class ConsoleReport(Report):
    def __init__(self, file, info, timing=False, indent='    ', colors=True, **kwargs):
        events = tuple(Event)
        if colors:
            events = tuple(c(Event.name(e)) for c, e in zip(COLORS, events)) + events[len(COLORS):]
        self.events, self.file, self.timing, self.indent = events, file, timing, indent
        if info[0]:
            self.file.write('Compiler: {}\n'.format(info[0]))
        if info[1] and info[2]:
            self.file.write('Compile time: {}, {}\n'.format(info[2], info[1]))
        self.kwargs = kwargs

    def __call__(self, index, args, info):
        return ConsoleTestReport(index, args, info, self.file, events=self.events,
            indent=self.indent, timing=self.timing, **self.kwargs)

    def finalize(self, n, time, counts, out, err):
        s = FOOTER + 'Total results for {} tests:\n'.format(n)

        spacing = max(map(len, self.events)) + 1
        for e, c in zip(self.events, counts):
            s += self.indent + '{} {}\n'.format((e + ':').ljust(spacing), c)

        if self.timing:
            if FOOTER: s += '\n'
            s += TOTAL_DURATION + ': %.7e\n' % time

        self.file.write(s)

    def __exit__(self, value, cls, traceback):
        self.file.write(FOOTER)

################################################################################

class ConsoleTestReport(Report):
    def __init__(self, index, args, info, file, events, timing=False, sync=False, indent='    '):
        self.indent, self.timing, self.events = indent, timing, events
        if sync:
            self.file, self.output = io.StringIO(), file
        else:
            self.file, self.output = file, None

        if info[1]:
            info = repr(info[0]) + ' (%s:%d) ' % info[1:3] + (repr(info[3]) if info[3] else '')
        else:
            info = repr(info[0])
        self.write(FOOTER, colored('Test %d ' % index, 'blue', attrs=['bold']), info, '\n')

    def write(self, *args):
        tuple(map(self.file.write, args))
        self.file.flush()

    def __call__(self, event, scopes, logs):
        self.write('\n', readable_message(self.events[event], scopes, logs, self.indent))

    def finalize(self, value, time, counts, out, err):
        for o, s in zip((out, err), (STDOUT, STDERR)):
            if o:
                self.write(s, ':\n', STREAM_FOOTER, o, STREAM_FOOTER, '\n')

        if value is not None:
            self.write('\n' if FOOTER else '', 'Return value: ', repr(value), '\n')

        if any(counts):
            s = ', '.join('%s: %d' % (e, c) for e, c in zip(self.events, counts) if c)
            self.write('\n' if FOOTER else '', 'Results: {%s}\n' % s)

        if self.timing:
            self.write(TEST_DURATION, ': %.7e\n' % time)

    def __exit__(self, value, cls, traceback):
        if self.output is not None:
            self.output.write(self.file.getvalue())
            self.output.flush()

################################################################################
