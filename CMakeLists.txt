cmake_minimum_required(VERSION 3.8)
project(rebind VERSION 0.0.0 LANGUAGES CXX)

################################################################################

set(REBIND_PYTHON "python" CACHE STRING "Python executable to use in deducing include directory")
# set(REBIND_PYTHON_OLD CACHE STRING INTERNAL)
# set(REBIND_PYTHON_INCLUDE "" CACHE STRING "Include directory containing Python.h")
set(REBIND_MIN_PYTHON 2.7 CACHE STRING "Minimum python version to look for (if REBIND_PYTHON and REBIND_PYTHON_INCLUDE not defined)")

################################################################################

if (${REBIND_PYTHON_INCLUDE})
    message("-- Using pre-specified Python include ${REBIND_PYTHON_INCLUDE}")
elseif (NOT REBIND_PYTHON STREQUAL REBIND_PYTHON_OLD)
    execute_process(
        COMMAND ${REBIND_PYTHON} -c "import sys, sysconfig; sys.stdout.write(sysconfig.get_paths()['include'])"
        RESULT_VARIABLE python_stat OUTPUT_VARIABLE python_include
    )
    if (python_stat)
        message(FATAL_ERROR "Failed to deduce include directory from '${REBIND_PYTHON}' executable.\nMaybe specify REBIND_PYTHON_INCLUDE directly.")
    endif()
    message("-- Using Python include ${python_include} deduced from REBIND_PYTHON=${REBIND_PYTHON}")
    set (REBIND_PYTHON_INCLUDE ${python_include} CACHE STRING "Python include directory for rebind")
    set (REBIND_PYTHON_OLD ${REBIND_PYTHON} CACHE INTERNAL "Last value of REBIND_PYTHON")
else()
    find_package(PythonLibs ${REBIND_MIN_PYTHON} REQUIRED)
    set (REBIND_PYTHON_INCLUDE ${PYTHON_INCLUDE_DIR} CACHE STRING "Python include directory for rebind")
endif()

################################################################################

add_library(rebind_interface INTERFACE)
target_compile_features(rebind_interface INTERFACE cxx_std_17)
target_include_directories(rebind_interface INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

################################################################################

# Maybe change in future to user provided interface library?
add_library(librebind STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/source/Source.cc
)
set_target_properties(librebind PROPERTIES OUTPUT_NAME rebind ${REBIND_STATIC_PROPERTIES})
target_link_libraries(librebind PUBLIC rebind_interface)

################################################################################

# Module.cc has to be recompiled based on the exported module name
# Could just build Python.cc as its own library, but here it's built together with Module.cc
set_property(GLOBAL PROPERTY rebind_module_files
    ${CMAKE_CURRENT_SOURCE_DIR}/include/rebind-python/Python.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/include/rebind-python/Module.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/include/rebind-python/Cast.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/include/rebind-python/Globals.cc
)

function(rebind_module module_name)
    get_property(files GLOBAL PROPERTY rebind_module_files)
    add_library(${module_name} MODULE ${files})
    set_target_properties(${module_name} PROPERTIES PREFIX "")

    if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
        set_target_properties(${module_name} PROPERTIES LINK_FLAGS "-Wl,-flat_namespace,-undefined,dynamic_lookup")
    else()
        set_target_properties(${module_name} PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
    endif()

    target_compile_definitions(${module_name} PRIVATE REBIND_MODULE=${module_name})
    target_link_libraries(${module_name} PRIVATE rebind_interface ${ARGN})
    target_include_directories(${module_name} PRIVATE ${REBIND_PYTHON_INCLUDE})
endfunction(rebind_module)

################################################################################

set(REBIND_PYTHON_ROOT python/rebind
    CACHE INTERNAL "Directory containing rebind module Python files")

set(REBIND_PYTHON_FILES
    __init__.py
    blank.py
    common.py
    dispatch.py
    render.py
    types.py
    CACHE INTERNAL "List of Python files in the rebind module"
)

################################################################################

add_library(rebind_rust SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/source/Rust.cc
)

target_link_libraries(rebind_rust PUBLIC librebind)

add_subdirectory(example)