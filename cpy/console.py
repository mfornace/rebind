import io, sys
from .common import Report, readable_message, Event

################################################################################

def not_colored(s, *args, **kwargs):
    return s

try:
    from termcolor import colored
except ImportError:
    colored = not_colored

################################################################################

class Colorer:
    event_colors = ['red', 'green', 'red', 'yellow', 'grey']

    def __init__(self, colors):
        self.colored = colored if colors else not_colored
        self.stream_footer = '_' * 22 + '\n'
        self.footer = '_' * 80 + '\n'
        self.stderr = self.colored('Contents of std::err', 'magenta')
        self.stdout = self.colored('Contents of std::out', 'magenta')
        self.test_duration = self.colored('Test duration', 'yellow')
        self.total_duration = self.colored('Total duration', 'yellow')

    def events(self):
        out = list(map(Event.name, Event))
        for i, c in enumerate(self.event_colors):
            out[i] = self.colored(out[i], c)
        return out

    def test_name(self, index):
        return self.colored('Test %d ' % index, 'blue', attrs=['bold'])

################################################################################

class ConsoleReport(Report):
    def __init__(self, file, info, timing=False, indent='    ', color=None, **kwargs):
        if isinstance(color, Colorer):
            self.color = color
        else:
            self.color = Colorer(color or (color is None and file.isatty()))

        self.events = self.color.events()
        self.file, self.timing, self.indent = file, timing, indent

        if info[0]:
            self.file.write('Compiler: {}\n'.format(info[0]))
        if info[1] and info[2]:
            self.file.write('Compile time: {}, {}\n'.format(info[2], info[1]))
        self.kwargs = kwargs

    def __call__(self, index, args, info):
        return ConsoleTestReport(index, args, info, self.file, events=self.events,
            indent=self.indent, color=self.color, timing=self.timing, **self.kwargs)

    def finalize(self, n, time, counts, out, err):
        s = self.color.footer + 'Total results for {} tests:\n'.format(n)

        spacing = max(map(len, self.events)) + 1
        for e, c in zip(self.events, counts):
            s += self.indent + '{} {}\n'.format((e + ':').ljust(spacing), c)

        if self.timing:
            if self.color.footer: s += '\n'
            s += self.color.total_duration + ': %.7e\n' % time

        self.file.write(s)

    def __exit__(self, value, cls, traceback):
        self.file.write(self.color.footer)

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
        self.write(self.color.footer, self.color.test_name(index), info, '\n')

    def write(self, *args):
        tuple(map(self.file.write, args))
        self.file.flush()

    def __call__(self, event, scopes, logs):
        self.write('\n', readable_message(self.events[event], scopes, logs, self.indent))

    def finalize(self, value, time, counts, out, err):
        for o, s in zip((out, err), (self.color.stdout, self.color.stderr)):
            if o:
                self.write(s, ':\n', self.color.stream_footer, o, self.color.stream_footer, '\n')

        if value is not None:
            self.write('\n' if self.color.footer else '', 'Return value: ', repr(value), '\n')

        if any(counts):
            s = ', '.join('%s: %d' % (e, c) for e, c in zip(self.events, counts) if c)
            self.write('\n' if self.color.footer else '', 'Results: {%s}\n' % s)

        if self.timing:
            self.write(self.color.test_duration, ': %.7e\n' % time)

    def __exit__(self, value, cls, traceback):
        if self.output is not None:
            self.output.write(self.file.getvalue())
            self.output.flush()

################################################################################
