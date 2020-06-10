import logging
from .cpp import call, Variable, Meta, Index

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
import inspect
print(type(Variable))
print(Variable.mro())
print(type(Variable).mro(type(Variable)))
assert isinstance(Variable, type)
assert isinstance(Meta, type)
assert issubclass(Meta, type)

################################################################################

def easy():
    return call('easy', out=Variable)

class Float(Variable):
    hmm: float = 'Doc string'

    def __init__(self):
        # raise NotImplementedError
        pass

    def value(self):
        return self.load(float)

    def __repr__(self):
        return 'Float({})'.format(self.value())

# Index.__hash__.__text_signature__ = ''
print(inspect.signature(Variable.index))
print(Variable.state(1))
print(repr(inspect.signature(Variable.index)))
# print(help(Variable))

# input('hmm')
# print(help(Variable.method))

f = Float()
print('instance_method', Float.instance_method)
print('hash', Float.__hash__)
print(Float.mro())
print('hash', hash(Float()))
print('set', set([Float()]))


class Goo(Variable):
    def __init__(self, value: float):
        call('Goo.new', value, out=self)

    def get_x(self):
        return self.method('get_x', out=Float, mode='r')

    # def __hash__(self):
    #     return 123

    @property
    def x(self):
        return self.attribute('x', out=float)

    @x.setter
    def x(self, value):
        self.method('.x=', value, out=None, mode='w')

print('Goo name:', Goo.__name__)
print('Goo mro:', Goo.mro())
print('Goo type:', type(Goo))

print('running easy()')
print(easy())
print('making Goo()')
goo = Goo(1.5)
print('goo', goo)
print('goo.x', goo.x)
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

s = call('string_argument', 'uhh2').load(str)

vec3 = call('vec3', [1,2,3])

assert call('bool', True, out=bool)

from typing import Dict
call('dict', {}).load(Dict[str, str])

call('dict', {'a': 'a'}).load(dict)

################################################################################
