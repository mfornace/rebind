from .cpp import document
from .rebind import render_module, forward, set_logger
from . import submodule
import logging

@forward
class Goo:
    x: float

    @forward
    def undefined(self):
        pass

    @forward
    def __init__(self, value):
        pass

    @forward
    def add(self) -> 'Goo':
        pass

log = logging.getLogger(__name__ + '.init')
logging.basicConfig(level=logging.INFO)
rebind.set_logger(log)

rendered_document = render_module(__name__, document)
