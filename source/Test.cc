#include <cpy/Test.h>
#include <cpy/Macros.h>
#include <iostream>
#include <any>

struct goo {
    friend std::ostream & operator<<(std::ostream &os, goo) {return os << "goo";}
};

/******************************************************************************/

auto test1 = unit_test("test-1", COMMENT("This is a test"), [](cpy::Context ctx, bool b) {
    ctx("a message");
    int n = ctx.section("new-section", [](cpy::Context ctx) {
        ctx.equal(3, 4);
        return 5;
    });
    ctx("hmm");
    ctx(b);

    std::cerr << "Hey I am std::cerr 1" << std::endl;
    std::cout << "Hey I am std::cout 1" << std::endl;

    ctx.time(1, []{return 1;});

    ctx(LOCATION).near(5, 5.0);

    auto xxx = 5, yyy = 6;

    ctx.equal(xxx, yyy, goo(), COMMENT("x should equal y"));

    if (!ctx.equal(1, 2)) return;
}, {{false}});

UNIT_TEST("test-2", "This is a test 2") = [](cpy::Context ctx) {
    std::cerr << "Hey I am std::cerr 2" << std::endl;
    std::cout << "Hey I am std::cout 2" << std::endl;


    std::cout << sizeof(std::monostate) << " sizeof(std::monostate)" << std::endl;
    std::cout << sizeof(bool)  << " sizeof(bool) " << std::endl;
    std::cout << sizeof(std::any)  << " sizeof(std::any) " << std::endl;
    std::cout << sizeof(cpy::Integer)  << " sizeof(Integer) " << std::endl;
    std::cout << sizeof(double)  << " sizeof(double) " << std::endl;
    std::cout << sizeof(std::complex<double>)  << " sizeof(std::complex<double>) " << std::endl;
    std::cout << sizeof(std::string)  << " sizeof(std::string) " << std::endl;
    std::cout << sizeof(std::string_view) << " sizeof(std::string_view)" << std::endl;
    std::cout << sizeof(cpy::Value) << " sizeof(Value)" << std::endl;

        return 8.9;
    //return "hello";
    // if (!ctx.throws_as<std::runtime_error>([]{})) return;
};

UNIT_TEST("test-3") = [](auto ctx) {
    std::cout << cpy::get_value("max_time").as_double() << std::endl;
    throw std::runtime_error("runtime_error: uh oh");
};

UNIT_TEST("test-4") = [](auto ctx) {
    throw cpy::Skip();
    ctx.equal(5, 5);
};

UNIT_TEST("test-5") = [](auto ctx) {
    ctx.equal(5, 5);
};

/******************************************************************************/
