cmake_minimum_required(VERSION 3.8)
project(rebind_example VERSION 0.0.0 LANGUAGES CXX)
# add_subdirectory(../ ${CMAKE_CURRENT_BINARY_DIR}/rebind)

################################################################################

add_library(librebind_example SHARED EXCLUDE_FROM_ALL Example.cc)
set_target_properties(librebind_example PROPERTIES OUTPUT_NAME "rebind_example")
target_link_libraries(librebind_example PRIVATE librebind)
rebind_module(example_cpp librebind_example)
set_target_properties(example_cpp PROPERTIES PREFIX "" LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/example)

################################################################################

# This function preferentially symlinks a file and records the output in ${link_outputs}
# If on Windows, a copy is done instead of symlinking
function(link_file in out)
    get_filename_component(dir "${CMAKE_CURRENT_BINARY_DIR}/${out}" DIRECTORY)
    if(WIN32)
        set(link_file_flag "copy")
    else()
        set(link_file_flag "create_symlink")
    endif()
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${out}"
        COMMAND ${CMAKE_COMMAND} -E make_directory ${dir}
        COMMAND ${CMAKE_COMMAND} -E ${link_file_flag} "${CMAKE_CURRENT_SOURCE_DIR}/${in}" "${CMAKE_CURRENT_BINARY_DIR}/${out}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${in}"
    )
    set(link_outputs ${link_outputs} "${CMAKE_CURRENT_BINARY_DIR}/${out}" PARENT_SCOPE)
endfunction(link_file)

################################################################################

link_file(python/__init__.py example/__init__.py)
link_file(python/submodule.py example/submodule.py)
link_file(python/test.py example/test.py)

################################################################################

add_custom_target(python ALL DEPENDS ${link_outputs} example_cpp)
