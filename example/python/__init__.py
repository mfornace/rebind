import logging

from .cpp import schema
from .ara import Schema, set_logger

################################################################################

log = logging.getLogger(__name__ + '.render')
logging.basicConfig(level=logging.INFO)
set_logger(log)

schema = Schema('example', schema)

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
