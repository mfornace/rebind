
################################################################################

set(ARA_PYTHON "python" CACHE STRING "Specified Python executable used to deduce include directory")
set(ARA_PYTHON_INCLUDE "" CACHE STRING "Specified include directory containing Python.h")

################################################################################

if (${ARA_PYTHON_INCLUDE})
    message("-- Using specified Python include")
else()
    execute_process(
        COMMAND ${ARA_PYTHON} -c "import sys, sysconfig; sys.stdout.write(sysconfig.get_paths()['include'])"
        RESULT_VARIABLE python_stat OUTPUT_VARIABLE ARA_PYTHON_INCLUDE
    )
    if (python_stat)
        message(FATAL_ERROR "Failed to deduce include directory from '${ARA_PYTHON}' executable.\nMaybe specify ARA_PYTHON_INCLUDE directly.")
    endif()
    message("-- Using Python include directory deduced from ARA_PYTHON=${ARA_PYTHON}")
endif()

message("-- Using Python include directory ${ARA_PYTHON_INCLUDE}")

################################################################################

add_library(sfb_python MODULE ${CMAKE_CURRENT_SOURCE_DIR}/source/python/Module.cc)
target_link_libraries(sfb_python PRIVATE libara)
target_include_directories(sfb_python PRIVATE ${ARA_PYTHON_INCLUDE})
set_target_properties(sfb_python PROPERTIES OUTPUT_NAME sfb PREFIX "")

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    set_target_properties(sfb_python PROPERTIES LINK_FLAGS "-Wl,-flat_namespace,-undefined,dynamic_lookup")
else()
    set_target_properties(sfb_python PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
endif()

################################################################################
