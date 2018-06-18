from .common import Events, colored, find

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

    def finalize(self, counts):
        s = '=' * 80 + '\nTotal counts:\n'

        spacing = max(map(len, Events))
        for e, c in zip(Events, counts):
            s += '    {} {}\n'.format(e.ljust(spacing), c)
        self.file.write(s)

    def __exit__(self, value, cls, traceback):
        self.file.write('=' * 80 + '\n')

################################################################################

class ConsoleTestHandler:
    def __init__(self, index, info, file, footer='\n', indent='    ', format_scope=None):
        if format_scope is None:
            self.format_scope = lambda s: repr('.'.join(s))
        else:
            self.format_scope = format_scope
        self.footer = footer
        self.indent = indent
        self.index = index
        self.info = info
        self.file = file

    def __call__(self, event, scopes, logs):
        keys, values = map(list, zip(*logs))
        line, path = (find(k, keys, values) for k in ('line', 'file'))
        scopes = self.format_scope(scopes)

        # first line
        if path is None:
            s = '{} {}\n'.format(Events[event], scopes)
        else:
            desc = '({})'.format(path) if line is None else '({}:{})'.format(path, line)
            s = '{} {} {}\n'.format(Events[event], scopes, desc)

        # comments
        while 'comment' in keys:
            s += '{}comment: {}\n'.format(self.indent, repr(find('comment', keys, values)))

        # comparisons
        comp = ('lhs', 'op', 'rhs')
        while all(c in keys for c in comp):
            lhs, op, rhs = (find(k, keys, values) for k in comp)
            s += self.indent + 'required: {} {} {}\n'.format(lhs, op, rhs)

        # all other logged keys and values
        for k, v in zip(keys, values):
            if k:
                s += self.indent + '{}: {}\n'.format(k, repr(v))
            else:
                s += self.indent + '{}\n'.format(repr(v))

        s += self.footer
        self.file.write(s)
        self.file.flush()

    def finalize(self, counts):
        if any(counts):
            s = ', '.join('%s %d' % (e, c) for e, c in zip(Events, counts) if c)
            self.file.write('Test counts: {%s}\n' % s)

    def __enter__(self):
        if self.info[1]:
            info = repr(self.info[0]) + ' (%s:%d): ' % self.info[1:3] + repr(self.info[3])
        else:
            info = repr(self.info[0])
        self.file.write('=' * 80 +
            colored('\nTest %d: ' % self.index, 'blue', attrs=['bold']) + info + '\n\n')
        self.file.flush()
        return self

    def __exit__(self, value, cls, traceback):
        pass

################################################################################
