cmake_minimum_required(VERSION 3.0)
project(cpy VERSION 0.0.0 LANGUAGES CXX)

find_package(PythonLibs 3 REQUIRED)

add_library(cpy MODULE Test.cc Python.cc)
set_property(TARGET cpy PROPERTY CXX_STANDARD 17)
target_include_directories(cpy PRIVATE ${PYTHON_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include/cpy)

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    set(CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS "${CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS} -undefined,dynamic_lookup")
else()
    set(CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS "${CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS} -undefined dynamic_lookup")
endif()