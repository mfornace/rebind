cmake_minimum_required(VERSION 3.8)
project(rebind VERSION 0.0.0 LANGUAGES CXX)

################################################################################

add_library(headers INTERFACE)
add_library(rebind::headers ALIAS headers)
target_compile_features(headers INTERFACE cxx_std_17)
target_include_directories(headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

################################################################################

# Maybe change in future to user provided interface library?
add_library(librebind STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/source/Source.cc
)
set_target_properties(librebind PROPERTIES OUTPUT_NAME rebind ${REBIND_STATIC_PROPERTIES})
target_link_libraries(librebind PUBLIC headers)

################################################################################

# This function preferentially symlinks a file and records the output in ${link_outputs}
# If on Windows, a copy is done instead of symlinking
# This function is provided for convenience in downstream use of the rebind library
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

include(cmake/Python.cmake)
include(cmake/Rust.cmake)

################################################################################

add_subdirectory(example)