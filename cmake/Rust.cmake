
################################################################################

add_library(rebind_rust STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/source/Rust.cc
)

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    set_target_properties(rebind_rust PROPERTIES LINK_FLAGS "-Wl,-flat_namespace,-undefined,dynamic_lookup")
else()
    set_target_properties(rebind_rust PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
endif()

target_link_libraries(rebind_rust PUBLIC librebind)