
#include <rebind/Schema.h>
#include <rebind/types/Arrays.h>
#include <iostream>

namespace example {

using rebind::Type;
using rebind::Scope;
using rebind::Value;
using rebind::Schema;

/******************************************************************************/

struct GooBase {

};

//remove iostream
struct Goo : GooBase {
    double x=1000;
    std::string name = "goo";

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

// bool members(Self<Goo> &s) {
//     return s("x", &Goo::x);
// }

bool method(Input<Goo> &self) {
    return self("x", &Goo::x)
        || self("test_throw", &Goo::test_throw)
        || self("get_x", [](Goo const &g) {return g.x;})
        || self("add", [](double x) {g.x += x;})
        || self.derive<GooBase>();
}

// to reference pretty much the same...
bool to_value(Output &o, Goo const &g) {
    if (o.matches<double>()) return o.emplace_if(g.x);
    if (o.matches<std::string_view>()) return o.emplace_if("hmmmmmm");
    // needs to be annotated somehow whether the return type is valid after "g" is destructed.
    if (o.matches<std::string_view>() && !o.leaks()) return o.emplace_if(g.name);
    return false;
}


std::optional<Goo> from_ref(Ref r, Type<Goo>) {
    std::optional<Goo> out;
    if (auto t = r.request<double>()) out.emplace(*t);
    return out;
}

/******************************************************************************/

// bool members(Self<Example> &s) {
//     return s("global_value", 123);
// }


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

    s.function("Goo.new", [](double x) -> Goo {return x;});

    // s.function("buffer", [](std::tuple<rebind::BinaryData, std::type_index, std::vector<std::size_t>> i) {
    //     DUMP(std::get<0>(i).size());
    //     DUMP(std::get<1>(i).name());
    //     DUMP(std::get<2>(i).size());
    //     for (auto &c : std::get<0>(i)) c += 4;
    // });
    s.function("vec1", [](std::vector<int> const &) {});
    s.function("vec2", [](std::vector<int> &) {});
    s.function("vec3", [](std::vector<int>) {});
    // s.function<1>("vec4", [](int, int i=2) {});

    DUMP("made schema");
}

}

namespace rebind {

void init(Schema &s) {example::write_schema(s);}

}