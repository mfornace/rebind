import logging
from . import cpp

print(dir(cpp))
# print(cpp.call(None,'easy').load(cpp.Variable).load(float))
# print(cpp.call(int,'easy'))

from .ara import Schema, set_logger

################################################################################

log = logging.getLogger(__name__ + '.ara')
logging.basicConfig(level=logging.INFO)
set_logger(log)

# from .cpp import schema
# schema = Schema('example', schema)

################################################################################

def easy():
    return cpp.call(cpp.Variable, 'easy')

class Goo(cpp.Variable):
    def __init__(self, value: float):
        cpp.call(self, 'Goo.new', value)

    def get_x(self):
        print(repr(self.index()), bool(self))
        return self(cpp.Variable, 'get_x')

print(easy())
goo = Goo(1.5)
print(goo)
print(goo.get_x())
print(goo.get_x().load(float))

################################################################################

from . import submodule

################################################################################

@schema.type
class Goo:
    x: float

    @schema.init
    def __init__(self, value):
        pass

    @schema.method
    def undefined(self):
        pass

    @schema.method
    def __str__(self) -> str:
        pass

    @schema.method
    def add(self) -> 'Goo':
        pass

global_value = schema.object('global_value', int)

################################################################################
