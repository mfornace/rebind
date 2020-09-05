cmake_minimum_required(VERSION 3.8)
project(sfb_example VERSION 0.0.0 LANGUAGES CXX)
# add_subdirectory(../ ${CMAKE_CURRENT_BINARY_DIR}/sfb)

################################################################################

add_library(example_source SHARED EXCLUDE_FROM_ALL Example.cc)
set_target_properties(example_source PROPERTIES OUTPUT_NAME "sfb_example")
target_link_libraries(example_source PRIVATE libsfb)
# set_target_properties(example_module PROPERTIES PREFIX "" LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/example)

################################################################################

add_executable(example_exe main.cc)
target_link_libraries(example_exe PRIVATE example_source libsfb)
set_target_properties(example_exe PROPERTIES OUTPUT_NAME main)

################################################################################

set(link_outputs)
link_file(python/__init__.py example/__init__.py)
link_file(python/submodule.py example/submodule.py)
link_file(python/test.py example/test.py)
foreach(f ${SFB_PYTHON_FILES})
    link_file(../python/sfb/${f} example/sfb/${f})
endforeach()

################################################################################

add_library(example_rust SHARED
    Example.cc
    ../source/Rust.cc
)

# if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
#     set_target_properties(example_rust PROPERTIES LINK_FLAGS "-Wl,-flat_namespace,-undefined,dynamic_lookup")
# else()
#     set_target_properties(example_rust PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
# endif()

target_link_libraries(example_rust PUBLIC libsfb)

################################################################################
