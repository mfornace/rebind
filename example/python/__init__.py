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

dump = lambda *args: print('\n******', *args)

def f():
    import inspect
    dump(type(Variable))
    dump(Variable.mro())
    assert isinstance(Variable, type)
    assert isinstance(Meta, type)
    assert issubclass(Meta, type)
f()

################################################################################

def easy():
    return call('easy', out=Variable)

class Float(Variable):
    hmm = Member(float, 'Doc string')

    def __init__(self):
        dump('python __init__')
        # raise NotImplementedError

    def value(self):
        return self.load(float)

    def __repr__(self):
        return 'Float({})'.format(self.value())

import sys

def f():
    dump(type(Variable))
    dump('make Variable', Variable())
    dump('make Variable', type(Variable()))
    v = Variable()
    dump('checking index temporary')
    dump(sys.getrefcount(v.index()))
    dump('checking stuff')
    i = v.index()
    dump('got index, refcount =', sys.getrefcount(i))
    dump('int of index', int(i))
    dump('int of index', int(i))
    dump('int of index', int(i))
    dump('index str', i)
    dump('index repr', repr(Variable().index()))
f()

def f():
    # dump(inspect.signature(Variable.index))
    # dump(repr(inspect.signature(Variable.index)))

    dump('Declared float')
    dump('Float mro', Float.mro())

    dump('making float')
    f = Float()
    # dump('instance_method', Float.instance_method)
    dump('Float hash', Float.__hash__)
    dump('Float() ref count', sys.getrefcount(Float()))
    dump(hash(f))
    # dump('hash', hash(Float()))
    # dump('set', set([Float()]))
f()

class Goo(Variable):
    def __init__(self, value: float):
        call('Goo.new', value, out=self)

    def get_x(self):
        return self.method('get_x', out=Float, mode='r')

    @forward('r', 'underlying name')
    def blah(self):
        pass

    x = Member('x')

def f():
    dump('Goo name:', Goo.__name__)
    dump('Goo mro:', Goo.mro())
    dump('Goo type:', type(Goo))

    dump('running easy()')
    dump(easy())
    dump('making Goo()')
    goo = Goo(1.5)
    dump('goo', goo)
    dump('goo.x', goo.x)
    dump('get_x()', goo.get_x())
    dump('get_x().lock()', goo.get_x().lock())
    dump('get_x', goo.get_x().load(float))

    a1 = Goo(1.5)
    a2 = Goo(1.5)

    dump('uses', goo.use_count(), a1.use_count(), a2.use_count())

    try:
        blah = goo.method('add', a1, a1, out=Variable, gil=True, mode='r:ww')
        raise ValueError('bad')
    except TypeError as e:
        dump('works:', e)

    dump('uses', goo.use_count(), a1.use_count(), a2.use_count())

    blah = goo.method('add', a1, a2, out=Variable, gil=True, mode='r:rr')
    blah = goo.method('add', a1, a2, out=Goo, gil=True, mode='r')
    # blah = goo.method('add', a1, a2, out=Variable, gil=True, mode='w')
f()

def f():
    s = call('string_argument', 'uhh2').load(str)
    vec3 = call('vec3', [1,2,3])
    assert call('bool', True, out=bool)
f()

from typing import Dict

def f():
    dump('try to call with empty dict')
    out = call('dict', {})
    dump('try to load empty')
    dump(out.load(Dict[str, str]))
f()

def f():
    dump('try to load with non-empty dict')
    out = call('dict', {'a': 'b'})
    dump('try to load non-empty')
    dump(out.load(Dict[str, str]))
f()

dump('finished!')

################################################################################
