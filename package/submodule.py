
def fun(i, d, _out=None):
    '''Example docstring for fun'''
    print('OUTPUT', _out, i, d)
    return _out + 5

def fun2(i, d, _fun=None):
    '''Example docstring for fun2'''
    return _fun(i, d)