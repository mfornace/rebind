
################################################################################

add_library(ara_rust STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/source/Rust.cc
)

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    set_target_properties(ara_rust PROPERTIES LINK_FLAGS "-Wl,-flat_namespace,-undefined,dynamic_lookup")
else()
    set_target_properties(ara_rust PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
endif()

target_link_libraries(ara_rust PUBLIC libara)