cmake_minimum_required(VERSION 3.8)
project(rebind_example VERSION 0.0.0 LANGUAGES CXX)
# add_subdirectory(../ ${CMAKE_CURRENT_BINARY_DIR}/rebind)

################################################################################

add_library(libexample SHARED EXCLUDE_FROM_ALL Example.cc)
set_target_properties(libexample PROPERTIES OUTPUT_NAME "rebind_example")
target_link_libraries(libexample PRIVATE librebind)
rebind_module(example_module cpp libexample)
set_target_properties(example_module PROPERTIES PREFIX "" LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/example)

################################################################################

set(link_outputs)
link_file(python/__init__.py example/__init__.py)
link_file(python/submodule.py example/submodule.py)
link_file(python/test.py example/test.py)
foreach(f ${REBIND_PYTHON_FILES})
    link_file(../python/rebind/${f} example/rebind/${f})
endforeach()

################################################################################

add_custom_target(example_python ALL DEPENDS ${link_outputs} example_module)
