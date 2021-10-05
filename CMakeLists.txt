cmake_minimum_required(VERSION 3.8)
project(sfb VERSION 0.0.0 LANGUAGES CXX)

################################################################################

add_library(headers INTERFACE)
add_library(sfb::headers ALIAS headers)
target_compile_features(headers INTERFACE cxx_std_17)
target_include_directories(headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

################################################################################

# Maybe change in future to user provided interface library?
add_library(sfb STATIC ${CMAKE_CURRENT_SOURCE_DIR}/source/Source.cc)
add_library(sfb::sfb ALIAS sfb)
target_link_libraries(sfb PUBLIC headers)

################################################################################

# This function preferentially symlinks a file and records the output in ${link_outputs}
# If on Windows, a copy is done instead of symlinking
# This function is provided for convenience in downstream use of the sfb library
function(link_file out_list in out)
    get_filename_component(dir ${out} DIRECTORY)
    if(WIN32)
        set(link_file_flag "copy")
    else()
        set(link_file_flag "create_symlink")
    endif()
    add_custom_command(OUTPUT ${out}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${dir}
        COMMAND ${CMAKE_COMMAND} -E ${link_file_flag} ${in} ${out}
        DEPENDS ${in}
    )
    set(${out_list} ${${out_list}} ${out}) # Update list in the current scope only
    # Before return, propagate variable's value to the parent scope.
    set(${out_list} ${${out_list}} PARENT_SCOPE)
endfunction(link_file)

################################################################################

include(cmake/Rust.cmake)

################################################################################

add_subdirectory(example)

################################################################################
