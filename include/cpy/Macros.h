
// #ifndef COMPARE
// #   define COMPARE(lhs, op, rhs, ...) require(lhs op rhs, ::cpy::comparison_glue(lhs, rhs, #op), ::cpy::comparison_glue(#lhs, #rhs, #op), ::cpy::line_number(__LINE__), ::cpy::file_name(__FILE__),  __VA_ARGS__)
// #endif

// #ifndef REQUIRE
// #   define REQUIRE(cond, ...) require(glue(#cond, cond), ::cpy::line_number(__LINE__), ::cpy::file_name(__FILE__),  __VA_ARGS__)
// #endif

// can change these to only give one operand too.
#ifndef LINE
#   define LINE() ::cpy::file_and_line(__FILE__, __LINE__)
#endif

#ifndef COMMENT
#   define COMMENT(STRING) ::cpy::comment(STRING, __FILE__, __LINE__)
#endif


#ifndef UNIT_TEST
    #define UNIT_TEST_CAT_IMPL(s1, s2) s1##s2

    #define UNIT_TEST_CAT(s1, s2) UNIT_TEST_CAT_IMPL(s1, s2)

    #define UNIT_TEST(NAME) static auto UNIT_TEST_CAT(anonymous_test_, __COUNTER__) = ::cpy::AnonymousClosure{NAME, __FILE__, __LINE__}
#endif
