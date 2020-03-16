
################################################################################

add_library(rust SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/source/Rust.cc
)

target_link_libraries(rust PUBLIC librebind)