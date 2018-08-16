
#include <cpy/Function.h>
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
};

Document & document() noexcept {
    static Document static_document;
    return static_document;
}

void goo::show() const {std::cout << x << ", " << &x << std::endl;}

// could make this return a document
bool make_document() {
    auto &doc = document();
    doc.def("fun", [](int i, double d) {
        return i + d;
    });
    doc.def("vec", [](double i, double d) {
        return std::vector<double>{i, i, d};
    });
    doc.def("goo.new", [](double x) -> goo {
        return x;
    });
    doc.def("goo.add", [](goo x) {
        x.x += 4;
        x.show();
        return x;
    });
    doc.def("goo.show", [](goo const &x) {
        x.show();
    });
    doc.method("goo", "show", make_function([](goo const &x) {
        x.show();
    }));
    doc.type<goo>("goo");
    return bool();
}

// then this is just add_document()
static bool blah = make_document();

}