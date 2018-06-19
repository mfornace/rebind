
// #ifndef REQUIRE
// #   define REQUIRE(cond, ...) require(glue(#cond, cond), ::cpy::line_number(__LINE__), ::cpy::file_name(__FILE__),  __VA_ARGS__)
// #endif

#ifndef LOCATION
#   define LOCATION ::cpy::file_and_line(__FILE__, __LINE__)
#endif

#ifndef COMMENT
#   define COMMENT(...) ::cpy::comment(__VA_ARGS__ "", __FILE__, __LINE__)
#endif


#ifndef UNIT_TEST
    #define UNIT_TEST_CAT_IMPL(s1, s2) s1##s2

    #define UNIT_TEST_CAT(s1, s2) UNIT_TEST_CAT_IMPL(s1, s2)

    #define UNIT_TEST(NAME, ...) static auto UNIT_TEST_CAT(anonymous_test_, __COUNTER__) = ::cpy::AnonymousClosure{NAME, COMMENT(__VA_ARGS__)}
#endif
