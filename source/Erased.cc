
#include <cpy/Document.h>
#include <iostream>

namespace cpy {

/******************************************************************************/

struct Blah {
    std::string name;
    Blah(std::string s) : name(s) {}
    void dump() const {std::cout << name << std::endl;}
};

auto to_value(Type<Blah>, Blah b) {return std::move(b.name);}

template <class T>
Blah from_value(Type<Blah>, T &&, DispatchMessage &msg) {
    if constexpr(std::is_same_v<no_qualifier<T>, std::string>)
        return Blah("haha");
    throw msg.error();
}

//remove iostream
struct Goo {
    double x=1000;

    Goo(double xx) : x(xx) {}
    Goo(Goo const &g) : x(g.x) {std::cout << "copy" << std::endl;}
    Goo(Goo &&g) noexcept : x(g.x) {std::cout << "move" << std::endl; g.x = -1;}
    Goo & operator=(Goo g) {
        x = g.x;
        std::cout << "assign" << std::endl;
        return *this;
    }
    void test_throw(double xx) {
        if (xx < 0) throw std::runtime_error("cannot be negative");
        else x += xx;
    }
    void show() const {std::cout << x << ", " << &x << std::endl;}
};

/******************************************************************************/

void render(Document &doc, Type<Blah> t) {
    doc.type(t, "submodule.Blah");
    doc.method(t, "new", Construct<Blah, std::string>());
    doc.method(t, "dump", &Blah::dump);
}

void render(Document &doc, Type<Goo> t) {
    doc.type(t, "Goo");
    doc.render(Type<Blah>());
    doc.method(t, "new", [](double x) -> Goo {return x;});
    doc.method(t, "add", [](Goo x) {
        x.x += 4;
        x.show();
        return x;
    });
    doc.method(t, "show", &Goo::show);
    doc.method(t, "test_throw", mutate([](Goo &g, double x) {
        std::cout << "before throw " << g.x << std::endl;
        g.test_throw(x);
    }));
}

// could make this return a document
bool make_document() {
    auto &doc = document();
    doc.recurse("fun", [](int i, double d) {
        return i + d;
    });
    doc.recurse("submodule.fun", [](int i, double d) {
        return i + d;
    });
    doc.recurse("vec", [](double i, double d) {
        return std::vector<double>{i, i, d};
    });
    doc.render(Type<Goo>());
    return bool();
}

// then this is just add_document()
static bool blah = make_document();

}