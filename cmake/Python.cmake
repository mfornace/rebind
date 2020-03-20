
################################################################################

set(REBIND_PYTHON "python" CACHE STRING "Specified Python executable used to deduce include directory")
set(REBIND_PYTHON_INCLUDE "" CACHE STRING "Specified include directory containing Python.h")

################################################################################

if (${REBIND_PYTHON_INCLUDE})
    message("-- Using specified Python include")
    set_property(GLOBAL PROPERTY rebind_python_include ${REBIND_PYTHON_INCLUDE})
else()
    execute_process(
        COMMAND ${REBIND_PYTHON} -c "import sys, sysconfig; sys.stdout.write(sysconfig.get_paths()['include'])"
        RESULT_VARIABLE python_stat OUTPUT_VARIABLE python_include
    )
    if (python_stat)
        message(FATAL_ERROR "Failed to deduce include directory from '${REBIND_PYTHON}' executable.\nMaybe specify REBIND_PYTHON_INCLUDE directly.")
    endif()
    message("-- Using Python include directory deduced from REBIND_PYTHON=${REBIND_PYTHON}")
    set_property(GLOBAL PROPERTY rebind_python_include ${python_include})
endif()

message("-- Using Python include directory ${python_include}")

################################################################################

# Module.cc has to be recompiled based on the exported module name
# Could just build Python.cc as its own library, but here it's built together with Module.cc
set_property(GLOBAL PROPERTY rebind_module_files
    ${CMAKE_CURRENT_SOURCE_DIR}/include/rebind-python/Python.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/include/rebind-python/Module.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/include/rebind-python/Cast.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/include/rebind-python/Globals.cc
)

################################################################################

# Make a Python module target with the given target name and output file name
# Any additional arguments are passed into target_link_libraries on the created target
function(rebind_module target_name output_name)
    get_property(files GLOBAL PROPERTY rebind_module_files)
    add_library(${target_name} MODULE ${files})
    set_target_properties(${target_name} PROPERTIES PREFIX "" OUTPUT_NAME ${output_name})

    if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
        set_target_properties(${target_name} PROPERTIES LINK_FLAGS "-Wl,-flat_namespace,-undefined,dynamic_lookup")
    else()
        set_target_properties(${target_name} PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
    endif()

    target_compile_definitions(${target_name} PRIVATE REBIND_MODULE=${output_name})
    target_link_libraries(${target_name} PRIVATE rebind::headers ${ARGN})
    target_include_directories(${target_name} PRIVATE ${REBIND_PYTHON_INCLUDE})
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