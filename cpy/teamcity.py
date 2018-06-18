from .common import readable_message, events
from teamcity.messages import TeamcityServiceMessages
import time, datetime

################################################################################

class TeamCityHandler:
    def __init__(self, file, info, **kwargs):
        self.messages = TeamcityServiceMessages(file)
        self.messages.message('compile-info', name=info[0], date=info[1], time=info[2])
        self.time = time.time()

    def __call__(self, index, info):
        return TeamCityTestHandler(self.messages, info[0])

    def __enter__(self):
        self.messages.testSuiteStarted('default-suite')
        self.time = time.time()
        return self

    def finalize(self, counts, out, err):
        pass

    def __exit__(self, value, cls, traceback):
        self.messages.testSuiteFinished('default-suite')

################################################################################

class TeamCityTestHandler:
    def __init__(self, messages, name):
        self.messages = messages
        self.name = name
        self.time = None

    def __call__(self, event, scopes, logs):
        assert event == 0 or event == 2
        self.messages.testFailed(self.name, readable_message(events(False)[event], scopes, logs))

    def finalize(self, counts, out, err):
        self.messages.message('counts', errors=str(counts[0]), exceptions=str(counts[2]))
        self.messages.testStdOut(self.name, out)
        self.messages.testStdErr(self.name, err)

    def __enter__(self):
        self.time = time.time()
        self.messages.testStarted(self.name)
        return self

    def __exit__(self, value, cls, traceback):
        t = datetime.timedelta(seconds=time.time() - self.time)
        self.messages.testFinished(self.name, testDuration=t)

################################################################################
