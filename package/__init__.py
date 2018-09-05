from .cpp import document
from cpy.build import render
from . import submodule

rendered = render(__name__, document)