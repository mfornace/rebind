#include "Test.h"
#include "Macros.h"

using namespace cpy;

/******************************************************************************/

auto test1 = unit_test("first-test", [](Context ctx) {
    ctx.info("a message");
    int n = ctx("new-section", [](Context ctx) {
        ctx.require_eq(3, 4);
        return 5;
    });
    ctx.info(true);

    ctx.time(1, []{return 1;});

    ctx.require_near(5, 5.0);

    auto xxx = 5, yyy = 6;

    ctx.require_eq(xxx, yyy, COMMENT("x should equal y"));

    throw std::runtime_error("runtime_error: uh oh");
    if (!ctx.require_eq(1, 2))
        return;
    if (!ctx.require_throws<std::runtime_error>([]{}))
        return;
});

UNIT_TEST("second-test") = [](Context const &) {};
UNIT_TEST("third-test") = [](Context const &) {};

/******************************************************************************/