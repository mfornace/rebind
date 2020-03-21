
################################################################################

add_library(rebind_rust SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/source/Rust.cc
)

target_link_libraries(rebind_rust PUBLIC librebind)