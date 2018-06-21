from .common import readable_message, events, Report
from teamcity.messages import TeamcityServiceMessages
import time, datetime

################################################################################

class TeamCityReport(Report):
    def __init__(self, file, info, **kwargs):
        self.messages = TeamcityServiceMessages(file)
        self.messages.message('compile-info', name=info[0], date=info[1], time=info[2])

    def __call__(self, index, args, info):
        return TeamCityTestReport(self.messages, args, info[0])

    def __enter__(self):
        self.messages.testSuiteStarted('default-suite')
        return self

    def __exit__(self, value, cls, traceback):
        self.messages.testSuiteFinished('default-suite')

################################################################################

class TeamCityTestReport(Report):
    def __init__(self, messages, args, name):
        self.messages = messages
        self.name = name
        self.messages.testStarted(self.name)

    def __call__(self, event, scopes, logs):
        assert event == 0 or event == 2
        self.messages.testFailed(self.name, readable_message(events(False)[event], scopes, logs))

    def finalize(self, value, time, counts, out, err):
        self.messages.message('counts', errors=str(counts[0]), exceptions=str(counts[2]))
        self.messages.testStdOut(self.name, out)
        self.messages.testStdErr(self.name, err)
        self.messages.testFinished(self.name, testDuration=datetime.timedelta(seconds=time))

################################################################################
