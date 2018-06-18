from .common import find, readable_message, events
import xml.etree.ElementTree as ET
import time, datetime

################################################################################

class XMLHandler:
    def __init__(self, info, index=0, name='cpy', package='', **kwargs):
        self.root = ET.Element('testsuites')
        time = datetime.datetime.now().isoformat(timespec='seconds')

        import socket
        host = socket.gethostname()

        self.suite = ET.SubElement(self.root, 'testsuite', id=str(index), name=name,
                                   package=package, hostname=host, timestamp=time)
        props = ET.SubElement(self.suite, 'properties')

        for k, v in zip(['compiler', 'compile-date', 'compile-time'], info):
            ET.SubElement(props, 'property', name=k, value=v)

        self.kwargs = kwargs
        self.cases = []
        self.time = time

    def __call__(self, index, info):
        c = XMLTestHandler(index, info)
        self.cases.append(c)
        return c

    def __enter__(self):
        self.time = time.time()
        return self

    def finalize(self, counts, out, err):
        self.suite.set('failures', str(counts[0]))
        self.suite.set('errors', str(counts[2]))
        for c in self.cases:
            self.suite.append(c.element)
        ET.SubElement(self.suite, 'system-out').text = out
        ET.SubElement(self.suite, 'system-err').text = err

    def __exit__(self, value, cls, traceback):
        self.suite.set('time', '%f' % (time.time() - self.time))
        self.suite.set('tests', str(len(self.cases)))

################################################################################

class XMLFileHandler(XMLHandler):
    def __init__(self, file, *args, **kwargs):
        self.file = file
        super().__init__(*args, **kwargs)

    def __exit__(self, value, cls, traceback):
        super().__exit__(value, cls, traceback)
        ET.ElementTree(self.root).write(self.file, xml_declaration=True)

################################################################################

class XMLTestHandler:
    def __init__(self, index, info):
        self.element = ET.Element('testcase', name=info[0], classname=info[0])
        self.time = None
        self.sub = None
        self.message = ''

    def __call__(self, event, scopes, logs):
        self.message += readable_message(events(False)[event], scopes, logs)
        if self.sub is None:
            if event == 0:
                self.sub = ET.SubElement(self.element, 'failure', message='', type='2')
            if event == 2:
                self.sub = ET.SubElement(self.element, 'error', message='', type='1')
        self.sub.set('message', self.message + readable_message(event, scopes, logs))

    def finalize(self, counts, out, err):
        pass

    def __enter__(self):
        self.time = time.time()
        return self

    def __exit__(self, value, cls, traceback):
        self.element.set('time', '%f' % (time.time() - self.time))

################################################################################
