from .cpp import document
from .rebind import render_module
from . import submodule

class Goo:
    x: float

    def __init__(self, value):
        pass

    def add(self) -> 'Goo':
        pass

rendered_document = render_module(__name__, document)
