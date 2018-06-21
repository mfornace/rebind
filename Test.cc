#include "Test.h"
#include "Macros.h"
#include <iostream>

struct goo {
    friend std::ostream & operator<<(std::ostream &os, goo) {return os << "goo";}
};

/******************************************************************************/

auto test1 = unit_test("first-test", COMMENT("This is a test"), [](cpy::Context ctx, bool b) {
    ctx.info("a message");
    int n = ctx("new-section", [](cpy::Context ctx) {
        ctx.require_eq(3, 4);
        return 5;
    });
    ctx.info("hmm");
    ctx.info(b);

    std::cerr << "Hey I am std::cerr 1" << std::endl;
    std::cout << "Hey I am std::cout 1" << std::endl;

    ctx.time(1, []{return 1;});

    ctx.require_near(5, 5.0);

    auto xxx = 5, yyy = 6;

    ctx.require_eq(xxx, yyy, goo(), COMMENT("x should equal y"));

    if (!ctx.require_eq(1, 2)) return;
}, {{false}});

UNIT_TEST("second-test", "This is a test 2") = [](cpy::Context ctx) {
    std::cerr << "Hey I am std::cerr 2" << std::endl;
    std::cout << "Hey I am std::cout 2" << std::endl;
    return 8.9;
    //return "hello";
    // if (!ctx.require_throws<std::runtime_error>([]{})) return;
};

UNIT_TEST("third-test") = [](auto ctx) {
    std::cout << cpy::get_value("max_time").as_double() << std::endl;
    throw std::runtime_error("runtime_error: uh oh");
};

UNIT_TEST("fourth-test") = [](auto ctx) {
    ctx.require_eq(5, 5);
};

/******************************************************************************/