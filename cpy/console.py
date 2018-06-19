from .common import events, colored, readable_message

################################################################################

class ConsoleHandler:
    def __init__(self, file, info, **kwargs):
        self.file = file
        self.file.write('Compiler info: {} ({}, {})\n'.format(*info))
        self.kwargs = kwargs

    def __call__(self, index, info):
        return ConsoleTestHandler(index, info, self.file, **self.kwargs)

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

class ConsoleTestHandler:
    def __init__(self, index, info, file, footer='\n', indent='    ', format_scope=None):
        self.format_scope = format_scope
        self.footer = footer
        self.indent = indent
        self.index = index
        self.info = info
        self.file = file

    def write(self, *args):
        for a in args:
            self.file.write(a)

    def __call__(self, event, scopes, logs):
        self.file.write(readable_message(events(True)[event], scopes, logs, self.indent, self.format_scope))
        self.file.write(self.footer)
        self.file.flush()

    def finalize(self, counts, out, err):
        if out:
            self.write('Contents of std::cout:\n', '-' * 22, '\n', out, '-' * 22, '\n\n')
        if err:
            self.write('Contents of std::cerr:\n', '-' * 22, '\n', err, '-' * 22, '\n\n')
        if any(counts):
            s = ', '.join('%s: %d' % (e, c) for e, c in zip(events(True), counts) if c)
            self.file.write('Counts for test %d: {%s}\n' % (self.index, s))

    def __enter__(self):
        if self.info[1]:
            info = repr(self.info[0]) + ' (%s:%d): ' % self.info[1:3] + repr(self.info[3])
        else:
            info = repr(self.info[0])
        self.file.write('=' * 80)
        self.file.write(colored('\nTest %d' % self.index, 'blue', attrs=['bold']))
        self.file.write(': {}\n\n'.format(info))
        self.file.flush()
        return self

    def __exit__(self, value, cls, traceback):
        pass

################################################################################
