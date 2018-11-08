
#include <cpy/Document.h>
#include <cpy/Standard.h>
#include <iostream>

namespace cpy {

/******************************************************************************/

struct Blah {
    std::string name;
    Blah(std::string s) : name(s) {}
    void dump() const {std::cout << name << std::endl;}
};

Variable response(std::type_index t, Blah b) {
    if (t == typeid(std::string)) return std::move(b.name);
    return {};
}

template <class T>
Blah request(Type<Blah>, T &&, Dispatch &msg) {
    if constexpr(std::is_same_v<no_qualifier<T>, std::string>)
        return Blah("haha");
    throw msg.error("bad blah", typeid(Blah), typeid(T));
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
    friend std::ostream &operator<<(std::ostream &os, Goo const &g) {
        return os << "Goo(" << g.x << ", " << &g.x << ")" << std::endl;
    }
};

template <class Q>
auto response(Q, qualified<Goo, Q> b, std::type_index t) {
    std::cout << "casting Blah to double const &" << std::endl;
    return (t == typeid(double)) ? &b.x : nullptr;
}

/******************************************************************************/

void render(Document &doc, Type<Blah> t) {
    doc.type(t, "submodule.Blah");
    doc.method(t, "new", construct<std::string>(t));
    doc.method(t, "dump", &Blah::dump);
}

void render(Document &doc, Type<Goo> t) {
    doc.type(t, "Goo");
    doc.render(Type<Blah>());
    doc.method(t, "new", [](double x) -> Goo {return x;});
    doc.method(t, "add", [](Goo x) {
        x.x += 4;
        std::cout << x.x << std::endl;
        return x;
    });
    doc.method(t, "{}", streamable(t));
}

// could make this return a document
bool make_document() {
    auto &doc = document();
    doc.function("fun", [](int i, double d) {
        return i + d;
    });
    doc.function("refthing", [](double const &d) {
        return d;
    });
    doc.function("submodule.fun", [](int i, double d) {
        return i + d;
    });
    doc.function("test_pair", [](std::pair<int, double> p) {
        p.first += 3;
        p.second += 0.5;
        return p;
    });
    doc.function("test_tuple", [](std::tuple<int, float> p) {
        return std::get<1>(p);
    });
    doc.function("vec", [](double i, double d) {
        return std::vector<double>{i, i, d};
    });
    doc.function("moo", [](Goo &i) {
        i.x += 5;
    });
    doc.function("lref", [](double &i) {i = 2;});
    doc.function("clref", [](double const &i) {});
    doc.function("noref", [](double i) {});
    doc.function("rref", [](double &&i) {});
    doc.render(Type<Goo>());

    doc.function("buffer", [](std::tuple<BinaryData, std::type_index, Vector<std::size_t>> i) {
        std::cout << std::get<0>(i).size() << std::endl;
        std::cout << std::get<1>(i).name() << std::endl;
        std::cout << std::get<2>(i).size() << std::endl;
        for (auto &c : std::get<0>(i)) c += 4;
    });
    doc.function("vec1", [](std::vector<int> const &) {});
    doc.function("vec2", [](std::vector<int> &) {});
    doc.function("vec3", [](std::vector<int>) {});

    return bool();
}

// then this is just add_document()
static bool static_document_trigger = make_document();

}