from lxml import etree

fname = "streamed.xml"
suite_name = 'suite'
package = 'package'
with open(fname, "wb") as f, etree.xmlfile(f) as xf:
    with xf.element("testsuites"):
        with xf.element('testsuite', id='0', name=suite_name, package=package, timestamp="2002-02-23T09:11:12", hostname="host", tests="1", failures="0", errors="0", time="0.001"):
            with xf.element('properties'):
                xf.write(etree.Element('property', name='aa', value='4'))

            with xf.element('testcase', name='case', classname='cls', time='0.1'):
                xf.write(etree.Element('error', message='msg', type='type'))

            with xf.element('system-out'):
                xf.write('hey some output')

            with xf.element('system-err'):
                xf.write('hey some output')


schema = etree.XMLSchema(file='Junit.xsd')
parser = etree.XMLParser(schema=schema)
root = etree.fromstring(open('streamed.xml', 'rb').read(), parser)


from .common import Events, colored, find

################################################################################

class XMLHandler:
    def __init__(self, file, info, **kwargs):
        self.file = file
        # self.file.write('Compiler info: {} ({}, {})\n'.format(*info))
        self.kwargs = kwargs

    def __call__(self, index, info):
        return XMLTestHandler(index, info, self.file, **self.kwargs)

    def __enter__(self):
        self.xmlfile = etree.xmlfile(self.file)
        self.testsuites = self.xmlfile.__enter__().element('testsuites')
        self.testsuite = self.testsuites.__enter__().element('testsuite')

    def finalize(self, counts):
        pass

    def __exit__(self, value, cls, traceback):
        self.testsuite.__exit__(value, cls, traceback)
        self.testsuites.__exit__(value, cls, traceback)
        self.xmlfile.__exit__(value, cls, traceback)

################################################################################

class XMLTestHandler:
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
        return self

    def __exit__(self, value, cls, traceback):
        pass

################################################################################
