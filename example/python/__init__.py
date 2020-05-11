import logging
from .cpp import call, Variable

print(dir(cpp))
# print(call(None,'easy').load(Variable).load(float))
# print(call(int,'easy'))

from .ara import Schema, set_logger

################################################################################

log = logging.getLogger(__name__ + '.ara')
logging.basicConfig(level=logging.INFO)
set_logger(log)

# from .cpp import schema
# schema = Schema('example', schema)

################################################################################

def easy():
    return call('easy', out=Variable)

class Float(Variable):
    def __init__(self):
        raise NotImplementedError

    def value(self):
        return self.load(float)

    def __repr__(self):
        return 'Float({})'.format(self.value())


class Goo(Variable):
    def __init__(self, value: float):
        call('Goo.new', value, out=self)

    def get_x(self):
        return self.method('get_x', out=Float, mode='r')

    @property
    def x(self):
        return self.method('.x', out=float)

    @x.setter
    def x(self, value):
        self.method('.x=', value, out=None, mode='w')


print('running easy()')
print(easy())
print('making Goo()')
goo = Goo(1.5)
print('goo', goo)
print('get_x()', goo.get_x())
print('get_x().lock()', goo.get_x().lock())
print('get_x', goo.get_x().load(float))

a1 = Goo(1.5)
a2 = Goo(1.5)

print('uses', goo.use_count(), a1.use_count(), a2.use_count())

try:
    blah = goo.method('add', a1, a1, out=Variable, gil=True, mode='r:ww')
    raise ValueError('bad')
except TypeError as e:
    print('works:', e)

print('uses', goo.use_count(), a1.use_count(), a2.use_count())

blah = goo.method('add', a1, a2, out=Variable, gil=True, mode='r:rr')
blah = goo.method('add', a1, a2, out=Goo, gil=True, mode='r')
# blah = goo.method('add', a1, a2, out=Variable, gil=True, mode='w')

################################################################################
