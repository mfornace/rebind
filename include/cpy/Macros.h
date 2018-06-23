
// #ifndef REQUIRE
// #   define REQUIRE(cond, ...) require(glue(#cond, cond), ::cpy::line_number(__LINE__), ::cpy::file_name(__FILE__),  __VA_ARGS__)
// #endif

#define CPY_CAT_IMPL(s1, s2) s1##s2
#define CPY_CAT(s1, s2) CPY_CAT_IMPL(s1, s2)

#define CPY_LOCATION ::cpy::file_and_line(__FILE__, __LINE__)
#define CPY_COMMENT(...) ::cpy::comment(__VA_ARGS__ "", __FILE__, __LINE__)
#define CPY_UNIT_TEST(NAME, ...) static auto CPY_CAT(anonymous_test_, __COUNTER__) = ::cpy::AnonymousClosure{NAME, CPY_COMMENT(__VA_ARGS__)}

#ifndef LOCATION
#   define LOCATION CPY_LOCATION
#endif

#ifndef COMMENT
#   define COMMENT CPY_COMMENT
#endif

#ifndef UNIT_TEST
#   define UNIT_TEST CPY_UNIT_TEST
#endif

