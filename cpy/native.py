from .common import events, Report
import time, datetime, json

################################################################################

class NativeReport(Report):
    def __init__(self, file, info, indent=None):
        self.file = file
        self.indent = indent
        self.contents = {
            'compile-info': dict(name=info[0], date=info[1], time=info[2]),
            'events': events(False),
            'tests': [],
        }

    def __call__(self, index, info):
        c = {}
        self.contents['tests'].append(c)
        return NativeTestReport(c, index, info[0])

    def __exit__(self, value, cls, traceback):
        if self.file is not None:
            json.dump(self.contents, self.file, indent=self.indent)

################################################################################

class NativeTestReport(Report):
    def __init__(self, contents, index, name):
        self.contents = contents
        self.contents['name'] = name
        self.contents['index'] = index
        self.contents['events'] = []

    def __call__(self, event, scopes, logs):
        self.contents['events'].append(dict(event=event, scopes=scopes, logs=logs))

    def finalize(self, value, counts, out, err):
        self.contents.update(dict(counts=counts, out=out, err=err, value=value))

################################################################################
