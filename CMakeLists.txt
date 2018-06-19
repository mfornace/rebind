cmake_minimum_required(VERSION 3.8)
project(cpy VERSION 0.0.0 LANGUAGES CXX)

find_package(PythonLibs 3 REQUIRED)

################################################################################

add_library(libcpy STATIC Python.cc)
target_compile_features(libcpy PUBLIC cxx_std_17)
set_property(TARGET libcpy PROPERTY OUTPUT_NAME cpy)
target_include_directories(libcpy PRIVATE ${PYTHON_INCLUDE_DIR})
target_include_directories(libcpy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/cpy)

################################################################################

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
set(CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS "${CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS} -undefined,dynamic_lookup")
else()
set(CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS "${CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS} -undefined dynamic_lookup")
endif()

################################################################################

add_library(testcpy MODULE EXCLUDE_FROM_ALL Test.cc)
set_property(TARGET testcpy PROPERTY OUTPUT_NAME cpy)
target_link_libraries(testcpy PRIVATE libcpy)