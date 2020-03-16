from .cpp import document
from .rebind import render_module
from . import submodule

rendered_document = render_module(__name__, document)
