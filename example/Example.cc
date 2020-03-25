
#include <rebind/Schema.h>
#include <rebind/types/Arrays.h>
#include <iostream>

namespace example {

using rebind::Type;
using rebind::Scope;
using rebind::Value;
using rebind::Schema;

/******************************************************************************/

struct Blah {
    std::string name;
    Blah(std::string s) : name(s) {}
    void dump() const {DUMP(name);}
};

Value response(std::type_index t, Blah b) {
    if (t == typeid(std::string)) return std::move(b.name);
    return {};
}

template <class T>
std::optional<Blah> from_ref(Type<Blah>, T &&, Scope &s) {
    if constexpr(std::is_same_v<rebind::unqualified<T>, std::string>)
        return Blah("haha");
    return s.error("bad blah", rebind::fetch<Blah>());
}

//remove iostream
struct Goo {
    double x=1000;

    Goo(double xx) : x(xx) {}
    Goo(Goo const &g) : x(g.x) {DUMP("copy");}
    Goo(Goo &&g) noexcept : x(g.x) {DUMP("move"); g.x = -1;}
    Goo & operator=(Goo g) {
        x = g.x;
        DUMP("assign");
        return *this;
    }
    void test_throw(double xx) {
        if (xx < 0) throw std::runtime_error("cannot be negative");
        else x += xx;
    }
    friend std::ostream &operator<<(std::ostream &os, Goo const &g) {
        return os << "Goo(" << g.x << ", " << &g.x << ")";
    }
};

// template <Qualifier Q>
// auto response(Index t, qualified<Goo, Q> b) {
//     DUMP("casting Blah to double const &");
//     return (t == typeid(double)) ? &b.x : nullptr;
// }

/******************************************************************************/

void render(Schema &s, Type<Blah> t) {
    s.type(t, "submodule.Blah");
    s.function("submodule.Blah.new", rebind::construct<std::string>(t));
    s.method(t, "dump", &Blah::dump);
}

void render(Schema &s, Type<Goo> t) {
    s.type(t, "Goo");
    s.render(Type<Blah>());
    s.function("Goo.new", [](double x) -> Goo {return x;});
    s.method(t, "add", [](Goo x) {
        x.x += 4;
        DUMP(x.x);
        return x;
    });
    s.method(t, ".x", &Goo::x);
    s.method(t, "{}", streamable(t));
}

// could make this return a schema
void write_schema(rebind::Schema &s) {
    s.object("global_value", 123);
    s.function("easy", [] {return 1.2;});
    s.function("fun", [](int i, double d) {
        return i + d;
    });
    s.function("refthing", [](double const &d) {
        return d;
    });
    s.function("submodule.fun", [](int i, double d) {
        return i + d;
    });
    s.function("test_pair", [](std::pair<int, double> p) {
        p.first += 3;
        p.second += 0.5;
        return p;
    });
    s.function("test_tuple", [](std::tuple<int, float> p) {
        return std::get<1>(p);
    });
    s.function("vec", [](double i, double d) {
        return std::vector<double>{i, i, d};
    });
    s.function("moo", [](Goo &i) {
        i.x += 5;
    });
    s.function("lref", [](double &i) {i = 2;});
    s.function("clref", [](double const &i) {});
    s.function("noref", [](double i) {});
    s.function("rref", [](double &&i) {});
    s.render(Type<Goo>());

    s.function("buffer", [](std::tuple<rebind::BinaryData, std::type_index, std::vector<std::size_t>> i) {
        DUMP(std::get<0>(i).size());
        DUMP(std::get<1>(i).name());
        DUMP(std::get<2>(i).size());
        for (auto &c : std::get<0>(i)) c += 4;
    });
    s.function("vec1", [](std::vector<int> const &) {});
    s.function("vec2", [](std::vector<int> &) {});
    s.function("vec3", [](std::vector<int>) {});
    s.function<1>("vec4", [](int, int i=2) {});

    DUMP("made schema");
}

}

namespace rebind {

void init(Schema &s) {example::write_schema(s);}

}