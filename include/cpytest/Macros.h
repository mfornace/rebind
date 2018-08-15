
// #ifndef REQUIRE
// #   define REQUIRE(cond, ...) require(glue(#cond, cond), ::cpy::line_number(__LINE__), ::cpy::file_name(__FILE__),  __VA_ARGS__)
// #endif

#define CPY_HERE ::cpy::file_line(__FILE__, __LINE__)
#define CPY_COMMENT(...) ::cpy::comment(__VA_ARGS__ "", __FILE__, __LINE__)
#define CPY_UNIT_TEST(NAME, ...) static auto CPY_CAT(anonymous_test_, __COUNTER__) = ::cpy::AnonymousClosure{NAME, CPY_COMMENT(__VA_ARGS__)}
#define CPY_GLUE(X) ::cpy::glue(CPY_STRING(X), X)

#ifndef GLUE
    #define GLUE CPY_GLUE
#endif

#ifndef HERE
#   define HERE CPY_HERE
#endif

#ifndef COMMENT
#   define COMMENT CPY_COMMENT
#endif

#ifndef UNIT_TEST
#   define UNIT_TEST CPY_UNIT_TEST
#endif

