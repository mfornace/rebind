cmake_minimum_required(VERSION 3.8)
project(cpy VERSION 0.0.0 LANGUAGES CXX)

################################################################################

set(CPY_PYTHON "python" CACHE STRING "Python executable to use in deducing include directory")
# set(CPY_PYTHON_INCLUDE "" CACHE STRING "Include directory containing Python.h")
set(CPY_MIN_PYTHON 2.7 CACHE STRING "Minimum python version to look for (if CPY_PYTHON and CPY_PYTHON_INCLUDE not defined)")

################################################################################

if (${CPY_PYTHON_INCLUDE})
    message("-- Using pre-specified Python include ${CPY_PYTHON_INCLUDE}")
elseif (CPY_PYTHON)
    execute_process(
        COMMAND ${CPY_PYTHON} -c "import sys, sysconfig; sys.stdout.write(sysconfig.get_paths()['include'])"
        RESULT_VARIABLE python_stat OUTPUT_VARIABLE python_include
    )
    if (python_stat)
        message(FATAL_ERROR "Failed to deduce include directory from '${CPY_PYTHON}' executable.\nMaybe specify CPY_PYTHON_INCLUDE directly.")
    endif()
    message("-- Using Python include ${python_include} deduced from ${CPY_PYTHON}")
    set (CPY_PYTHON_INCLUDE ${python_include} CACHE STRING "Python include directory for cpy")
else()
    find_package(PythonLibs ${CPY_MIN_PYTHON} REQUIRED)
    set (CPY_PYTHON_INCLUDE ${PYTHON_INCLUDE_DIR} CACHE STRING "Python include directory for cpy")
endif()

################################################################################

add_library(cpy_interface INTERFACE)
target_compile_features(cpy_interface INTERFACE cxx_std_17)
target_include_directories(cpy_interface INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

################################################################################

# Maybe change in future to user provided interface library?
add_library(libcpy STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/source/cpy/Value.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/source/lilwil/Source.cc
)
set_target_properties(libcpy PROPERTIES OUTPUT_NAME cpy ${CPY_STATIC_PROPERTIES})
target_link_libraries(libcpy PUBLIC cpy_interface)

################################################################################

# Module.cc has to be recompiled based on the exported module name
# Could just build Python.cc as its own library, but here it's built together with Module.cc
set_property(GLOBAL PROPERTY cpy_module_files
    # ${CMAKE_CURRENT_SOURCE_DIR}/source/lilwil/Module.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/source/cpy/Python.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/source/cpy/Module.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/source/cpy/Globals.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/source/lilwil/Runner.cc
    # ${CMAKE_CURRENT_SOURCE_DIR}/source/lilwil/Python.cc
)

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
    target_include_directories(${module_name} PRIVATE ${CPY_PYTHON_INCLUDE})
endfunction(cpy_module)

################################################################################

add_library(testcpy SHARED EXCLUDE_FROM_ALL source/Test.cc source/lilwil/Runner.cc)
set_property(TARGET testcpy PROPERTY OUTPUT_NAME test)
target_link_libraries(testcpy PRIVATE libcpy)
cpy_module(mymodule testcpy)

################################################################################

add_library(erased SHARED EXCLUDE_FROM_ALL source/Erased.cc)
target_link_libraries(erased PRIVATE libcpy)
cpy_module(cpp erased)