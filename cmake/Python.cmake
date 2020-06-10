
################################################################################

set(ARA_PYTHON "python" CACHE STRING "Specified Python executable used to deduce include directory")
set(ARA_PYTHON_INCLUDE "" CACHE STRING "Specified include directory containing Python.h")

################################################################################

if (${ARA_PYTHON_INCLUDE})
    message("-- Using specified Python include")
    set_property(GLOBAL PROPERTY ara_python_include ${ARA_PYTHON_INCLUDE})
else()
    execute_process(
        COMMAND ${ARA_PYTHON} -c "import sys, sysconfig; sys.stdout.write(sysconfig.get_paths()['include'])"
        RESULT_VARIABLE python_stat OUTPUT_VARIABLE python_include
    )
    if (python_stat)
        message(FATAL_ERROR "Failed to deduce include directory from '${ARA_PYTHON}' executable.\nMaybe specify ARA_PYTHON_INCLUDE directly.")
    endif()
    message("-- Using Python include directory deduced from ARA_PYTHON=${ARA_PYTHON}")
    set_property(GLOBAL PROPERTY ara_python_include ${python_include})
endif()

message("-- Using Python include directory ${python_include}")

################################################################################

# Module.cc has to be recompiled based on the exported module name
# Could just build Python.cc as its own library, but here it's built together with Module.cc
set_property(GLOBAL PROPERTY ara_module_files
    # ${CMAKE_CURRENT_SOURCE_DIR}/source/python/Python.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/source/python/Module.cc
    # ${CMAKE_CURRENT_SOURCE_DIR}/source/python/Source.cc
    # ${CMAKE_CURRENT_SOURCE_DIR}/source/python/Load.cc
    # ${CMAKE_CURRENT_SOURCE_DIR}/source/python/Call.cc
    # ${CMAKE_CURRENT_SOURCE_DIR}/source/python/Cast.cc
    # ${CMAKE_CURRENT_SOURCE_DIR}/source/python/Globals.cc
)

################################################################################

# Make a Python module target with the given target name and output file name
# Any additional arguments are passed into target_link_libraries on the created target
function(ara_module target_name output_name)
    get_property(files GLOBAL PROPERTY ara_module_files)
    get_property(include GLOBAL PROPERTY ara_python_include)

    add_library(${target_name} MODULE ${files})
    set_target_properties(${target_name} PROPERTIES PREFIX "" OUTPUT_NAME ${output_name})

    if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
        set_target_properties(${target_name} PROPERTIES LINK_FLAGS "-Wl,-flat_namespace,-undefined,dynamic_lookup")
    else()
        set_target_properties(${target_name} PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
    endif()

    target_compile_definitions(${target_name} PRIVATE ARA_MODULE=${output_name})
    target_link_libraries(${target_name} PRIVATE ara::headers ${ARGN})
    target_include_directories(${target_name} PRIVATE ${include})
endfunction(ara_module)

################################################################################

set(ARA_PYTHON_ROOT python/ara
    CACHE INTERNAL "Directory containing ara module Python files")

set(ARA_PYTHON_FILES
    __init__.py
    blank.py
    common.py
    dispatch.py
    render.py
    types.py
    CACHE INTERNAL "List of Python files in the ara module"
)

################################################################################