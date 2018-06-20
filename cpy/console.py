from .common import events, colored, readable_message

################################################################################

class ConsoleReport:
    def __init__(self, file, info, **kwargs):
        self.file = file
        self.file.write('Compiler info: {} ({}, {})\n'.format(*info))
        self.kwargs = kwargs

    def __call__(self, index, info):
        return ConsoleTestReport(index, info, self.file, **self.kwargs)

    def __enter__(self):
        return self

    def finalize(self, counts, out, err):
        s = '=' * 80 + '\nTotal counts:\n'

        spacing = max(map(len, events(True))) + 1
        for e, c in zip(events(True), counts):
            s += '    {} {}\n'.format((e + ':').ljust(spacing), c)
        self.file.write(s)

    def __exit__(self, value, cls, traceback):
        self.file.write('=' * 80 + '\n')

################################################################################

class ConsoleTestReport:
    def __init__(self, index, info, file, indent='    '):
        self.indent = indent
        self.index = index
        self.info = info
        self.file = file

    def write(self, *args):
        for a in args:
            self.file.write(a)
        self.file.flush()

    def __call__(self, event, scopes, logs):
        self.write(readable_message(events(True)[event], scopes, logs, self.indent), '\n')

    def finalize(self, value, counts, out, err):
        for o, s in zip((out, err), ('cout', 'cerr')):
            if o:
                self.write(colored('Contents of std::%s' % s, 'magenta'), ':\n',
                       '=' * 22, '\n', o, '=' * 22, '\n\n')
        if value is not None:
            self.write(colored('Return value', 'blue'), ': {}\n'.format(value))
        if any(counts):
            s = ', '.join('%s: %d' % (e, c) for e, c in zip(events(True), counts) if c)
            self.write('Counts for test %d: {%s}\n' % (self.index, s))

    def __enter__(self):
        if self.info[1]:
            info = repr(self.info[0]) + ' (%s:%d): ' % self.info[1:3] + repr(self.info[3])
        else:
            info = repr(self.info[0])
        self.write('=' * 80,
                   colored('\nTest %d' % self.index, 'blue', attrs=['bold']),
                   ': {}\n\n'.format(info))
        return self

    def __exit__(self, value, cls, traceback):
        pass

################################################################################
