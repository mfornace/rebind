cmake_minimum_required(VERSION 3.8)
project(cpy VERSION 0.0.0 LANGUAGES CXX)

find_package(PythonLibs 3.3 REQUIRED) # 2.7 actually OK though...

################################################################################

add_library(cpy_interface INTERFACE)
target_compile_features(cpy_interface INTERFACE cxx_std_17)
target_include_directories(cpy_interface INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

################################################################################

add_library(libcpy STATIC ${CMAKE_CURRENT_SOURCE_DIR}/Source.cc)
set_property(TARGET libcpy PROPERTY OUTPUT_NAME cpy)
target_link_libraries(libcpy PUBLIC cpy_interface)

################################################################################

# Module.cc has to be recompiled based on the exported module name
# Could just build Python.cc as its own library, but here it's built together with Module.cc
set_property(GLOBAL PROPERTY cpy_module_files ${CMAKE_CURRENT_SOURCE_DIR}/Module.cc ${CMAKE_CURRENT_SOURCE_DIR}/Python.cc)

function(cpy_module module_name)
    get_property(files GLOBAL PROPERTY cpy_module_files)
    add_library(${module_name} MODULE ${files})
    set_target_properties(${module_name} PROPERTIES PREFIX "")

    if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
        set_target_properties(${module_name} PROPERTIES LINK_FLAGS "-undefined,dynamic_lookup")
    else()
        set_target_properties(${module_name} PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
    endif()

    target_compile_definitions(${module_name} PRIVATE CPY_MODULE=${module_name})
    target_link_libraries(${module_name} PRIVATE cpy_interface ${ARGN})
    target_include_directories(${module_name} PRIVATE ${PYTHON_INCLUDE_DIR})
endfunction(cpy_module)

################################################################################

add_library(testcpy STATIC EXCLUDE_FROM_ALL Test.cc)
set_property(TARGET testcpy PROPERTY OUTPUT_NAME test)
target_link_libraries(testcpy PRIVATE libcpy)
cpy_module(mymodule testcpy)

################################################################################
