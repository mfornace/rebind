from . import schema

@schema.function
def fun(i, d) -> float:
    '''Example docstring for fun'''

@schema.function
def fun2(i, d, function=None):
    '''Example docstring for fun2'''
    return function(i, d)