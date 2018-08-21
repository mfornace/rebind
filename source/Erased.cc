
#include <cpy/Document.h>
#include <iostream>

namespace cpy {

//remove iostream
struct goo {
    double x=1000;

    void show() const;
    goo(double xx) : x(xx) {}
    goo(goo const &g) : x(g.x) {std::cout << "copy" << std::endl;}
    goo(goo &&g) noexcept : x(g.x) {std::cout << "move" << std::endl; g.x = -1;}
    goo & operator=(goo g) {
        x = g.x;
        std::cout << "assign" << std::endl;
        return *this;
    }
    void test_throw(double xx) {
        if (xx < 0) throw std::runtime_error("cannot be negative");
        else x += xx;
    }
};

void goo::show() const {std::cout << x << ", " << &x << std::endl;}


template <class F, class C, class ...Ts>
auto mutate(F &&f, Pack<void, C, Ts...>) {
    return [f] (no_qualifier<C> &&self, Ts ...ts) {
        f(self, static_cast<decltype(ts) &&>(ts)...);
        return Value(std::move(self));
    };
}

template <class F>
auto mutate(F &&f) {return mutate(static_cast<F &&>(f), Signature<no_qualifier<F>>());}


void define(Document &doc, Type<goo>) {
    doc.type<goo>("goo");
    doc.recurse("goo.new", [](double x) -> goo {
        return x;
    });
    doc.recurse("goo.add", [](goo x) {
        x.x += 4;
        x.show();
        return x;
    });
    doc.recurse("goo.show", [](goo const &x) {
        x.show();
    });
    doc.method("goo", "show", [](goo const &x) {
        x.show();
    });
    doc.recurse("goo.test_throw", mutate([](goo &g, double x) {
        std::cout << "before throw " << g.x << std::endl;
        g.test_throw(x);
    }));
}

// could make this return a document
bool make_document() {
    auto &doc = document();
    doc.define("fun", [](int i, double d) {
        return i + d;
    });
    doc.define("vec", [](double i, double d) {
        return std::vector<double>{i, i, d};
    });
    define(doc, Type<goo>());
    return bool();
}

// then this is just add_document()
static bool blah = make_document();

}