import os
from setuptools import setup

def read(fname):
    return open(os.path.join(os.path.dirname(__file__), fname)).read()

setup(
    name="cpy",
    version="0.0.4",
    author="Mark Fornace",
    description=("Python bound C++ unit testing"),
    license="BSD",
    keywords="example documentation tutorial",
    url="http://packages.python.org/an_example_pypi_project",
    packages=['cpy'],
    long_description=read('README.md'),
)
