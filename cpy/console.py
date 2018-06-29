import io, sys
from .common import Report, readable_message, Event

################################################################################

def not_colored(s, *args, **kwargs):
    return s
try:
    from termcolor import colored
except (ImportError, AssertionError):
    colored = not_colored

################################################################################

_both = lambda n, c: (n, colored(n, c))

class STRINGS:
    stream_footer = '_' * 22 + '\n'
    footer = '_' * 80 + '\n'

    colors =  [
        lambda e: colored(e, 'red'),
        lambda e: colored(e, 'green'),
        lambda e: colored(e, 'red'),
        lambda e: colored(e, 'yellow'),
        lambda e: colored(e, 'grey')
    ]

    stderr = _both('Contents of std::err', 'magenta')
    stdout = _both('Contents of std::out', 'magenta')

    test_duration = _both('Test duration', 'yellow')
    total_duration = _both('Total duration', 'yellow')

    test_name = (lambda c, i: colored('Test %d ' % i, 'blue', attrs=['bold']) if c else 'Test %d ' % i)

################################################################################

class ConsoleReport(Report):
    def __init__(self, file, info, timing=False, indent='    ', colors=None, **kwargs):
        events = tuple(map(Event.name, Event))
        self.color = colors or (colors is None and file.isatty())
        if self.color:
            fs = STRINGS.colors
            events = tuple(c(Event.name(e)) for c, e in zip(fs, Event)) + events[len(fs):]
        self.events, self.file, self.timing, self.indent = events, file, timing, indent

        if info[0]:
            self.file.write('Compiler: {}\n'.format(info[0]))
        if info[1] and info[2]:
            self.file.write('Compile time: {}, {}\n'.format(info[2], info[1]))
        self.kwargs = kwargs

    def __call__(self, index, args, info):
        return ConsoleTestReport(index, args, info, self.file, events=self.events,
            indent=self.indent, color=self.color, timing=self.timing, **self.kwargs)

    def finalize(self, n, time, counts, out, err):
        s = STRINGS.footer + 'Total results for {} tests:\n'.format(n)

        spacing = max(map(len, self.events)) + 1
        for e, c in zip(self.events, counts):
            s += self.indent + '{} {}\n'.format((e + ':').ljust(spacing), c)

        if self.timing:
            if STRINGS.footer: s += '\n'
            s += STRINGS.total_duration[self.color] + ': %.7e\n' % time

        self.file.write(s)

    def __exit__(self, value, cls, traceback):
        self.file.write(STRINGS.footer)

################################################################################

class ConsoleTestReport(Report):
    def __init__(self, index, args, info, file, events, color=False, timing=False, sync=False, indent='    '):
        self.color, self.indent, self.timing, self.events = color, indent, timing, events
        if sync:
            self.file, self.output = io.StringIO(), file
        else:
            self.file, self.output = file, None

        if info[1]:
            info = repr(info[0]) + ' (%s:%d) ' % info[1:3] + (repr(info[3]) if info[3] else '')
        else:
            info = repr(info[0])
        self.write(STRINGS.footer, STRINGS.test_name(self.color, index), info, '\n')

    def write(self, *args):
        tuple(map(self.file.write, args))
        self.file.flush()

    def __call__(self, event, scopes, logs):
        self.write('\n', readable_message(self.events[event], scopes, logs, self.indent))

    def finalize(self, value, time, counts, out, err):
        for o, s in zip((out, err), (STRINGS.stdout[self.color], STRINGS.stderr[self.color])):
            if o:
                self.write(s, ':\n', STRINGS.stream_footer, o, STRINGS.stream_footer, '\n')

        if value is not None:
            self.write('\n' if STRINGS.footer else '', 'Return value: ', repr(value), '\n')

        if any(counts):
            s = ', '.join('%s: %d' % (e, c) for e, c in zip(self.events, counts) if c)
            self.write('\n' if STRINGS.footer else '', 'Results: {%s}\n' % s)

        if self.timing:
            self.write(STRINGS.test_duration[self.color], ': %.7e\n' % time)

    def __exit__(self, value, cls, traceback):
        if self.output is not None:
            self.output.write(self.file.getvalue())
            self.output.flush()

################################################################################
