import io
from .common import events, colored, readable_message, Report

################################################################################

STREAM_FOOTER = '_' * 22 + '\n'
FOOTER = '_' * 80 + '\n'

################################################################################

class ConsoleReport(Report):
    def __init__(self, file, info, timing=False, indent='    ', **kwargs):
        self.file, self.timing, self.indent = file, timing, indent
        if info[0]:
            self.file.write('Compiler: {}\n'.format(info[0]))
        if info[1] and info[2]:
            self.file.write('Compile time: {}, {}\n'.format(info[2], info[1]))
        self.kwargs = kwargs

    def __call__(self, index, args, info):
        return ConsoleTestReport(index, args, info, self.file,
            indent=self.indent, timing=self.timing, **self.kwargs)

    def finalize(self, time, counts, out, err):
        s = FOOTER + 'Total results:\n'

        spacing = max(map(len, events(True))) + 1
        for e, c in zip(events(True), counts):
            s += self.indent + '{} {}\n'.format((e + ':').ljust(spacing), c)

        if self.timing:
            if FOOTER: s += '\n'
            s += colored('Total duration', 'yellow') + ': %.7e\n' % time

        self.file.write(s)

    def __exit__(self, value, cls, traceback):
        self.file.write(FOOTER)

################################################################################

class ConsoleTestReport(Report):
    def __init__(self, index, args, info, file, timing=False, sync=False, indent='    '):
        self.indent = indent
        self.timing = timing
        if sync:
            self.file, self.output = io.StringIO(), file
        else:
            self.file, self.output = file, None

        if info[1]:
            info = repr(info[0]) + ' (%s:%d): ' % info[1:3] + (repr(info[3]) if info[3] else '')
        else:
            info = repr(info[0])
        self.write(FOOTER, colored('Test %d' % index, 'blue', attrs=['bold']),
                   ': {}\n'.format(info))

    def write(self, *args):
        for a in args:
            self.file.write(a)
        self.file.flush()

    def __call__(self, event, scopes, logs):
        self.write('\n', readable_message(events(True)[event], scopes, logs, self.indent))

    def finalize(self, value, time, counts, out, err):
        for o, s in zip((out, err), ('cout', 'cerr')):
            if o:
                self.write(colored('Contents of std::%s' % s, 'magenta'), ':\n',
                       STREAM_FOOTER, o, STREAM_FOOTER, '\n')
        if value is not None:
            self.write('\n' if FOOTER else '', 'Return value: ', repr(value), '\n')

        if any(counts):
            s = ', '.join('%s: %d' % (e, c) for e, c in zip(events(True), counts) if c)
            self.write('\n' if FOOTER else '', 'Results: {%s}\n' % s)

        if self.timing:
            self.write(colored('Test duration', 'yellow'), ': %.7e\n' % time)

    def __exit__(self, value, cls, traceback):
        if self.output is not None:
            self.output.write(self.file.getvalue())
            self.output.flush()

################################################################################
