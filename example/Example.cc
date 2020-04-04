
#include <rebind/Schema.h>
#include <rebind/types/Arrays.h>
#include <iostream>


#include <string_view>
#include <vector>
#include <string>
#include <any>

static_assert(16 == sizeof(std::shared_ptr));
static_assert(16 == sizeof(std::string_view));
static_assert(24 == sizeof(std::string));
static_assert(32 == sizeof(std::any));
static_assert(24 == sizeof(std::vector<int>));
static_assert(16 == sizeof(std::unique_ptr<int>));
static_assert(16 == sizeof(std::shared_ptr<int>));
static_assert(16 == sizeof(std::optional<int>));

static_assert(std::is_trivially_copyable<std::optional<int>>);

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

// template <class T>
// struct Temporary {
//     T &&t;
//     T *operator->() const {return &t;}
//     T &&operator*() const {return t;}
// };

// String
// --> Ref holding <String &&>
// now we want String back
// ...mm not really possible
// in this case we need Value to be possible as an argument
// what about excluding && possibility?
// i.e. in c++ if we want to move in we instead allocate a moved version of the input
// obvious problem is the allocation, first of all. not really necessary.
// ok so then we have all 4 possibilties again I guess.
// request<T> is still fairly easy I think
// &T, &mut T, T
// T&, T const &, T &&
// response is fairly wordy
bool to_value(Object &o, Index i, Thing t) {
    if (i.equals<std::string>()) {
        if (auto r = t.rvalue()) return o.emplace<std::string>((*r).name);
        return o.emplace<std::string>((*r).name);
    }

    if (i.equals<GooBase>()) {
        if (auto r = t.rvalue()) o.emplace(*r);
        else o.emplace(*t);
    }

    if (t.in_scope()) {
        return t.member(&M::name)
            || t.derive<GooBase>()
            || (i.equals<std::string_view>() && o.emplace(r->name));
            || (i.equals<mutable_string_view>()) && o.visit<mutable_string_view>() {
            if (auto r = t.lvalue()) return (*r).name;
            if (auto r = t.rvalue()) return (*r).name; // very rarely useful
        }

        // if (i.equals<std::string const &>())
        //     return o.emplace<std::string>(r->name);

        // if (i.equals<std::string &&>())
        //     if (auto r = t.rvalue()) return o.emplace<std::string>((*r).name);

        // if (i.equals<std::string &>())
        //     if (auto r = t.lvalue()) return o.emplace<std::string>((*r).name);


        // if (i.equals<GooBase &>())
        //     if (auto r = t.lvalue()) return o.emplace(*r);
        // if (i.equals<GooBase const &>())
        //     if (auto r = t.lvalue()) return o.emplace(*r);
        // if (i.equals<GooBase &&>())
        //     if (auto r = t.lvalue()) return o.emplace(*r);
    }
}


// // maybe better to just use this in the non-reference case:
// // these are safe conversions even if g is destructed later
// bool to_value(Object &o, Index i, Temporary<Goo const> g) {
//     // inserts copy
//     if (i.equals<std::string>()) return o.emplace<std::string>(t->x), true; // safe
// }

// // this safe too, yep
// bool to_value(Object &o, Index i, Temporary<Goo> g) {
//     if (i.equals<std::string>()) return o.emplace<std::string>(std::move(g->name)); // safe
//     return to_value<Temporary<Goo const>>();
// }

// // almost same as Temporary<Goo>...but can bind to a const & too I guess.
// bool to_value(Object &o, Index i, Goo &&) {
//     // if (i.equals<std::string>()) return o.emplace<std::string>(std::move(g->name)); // safe
//     return to_value<Temporary<Goo>>() || to_value<Goo const &>();
// }

// bool to_value(Object &o, Index i, Goo &) {
//     // not particularly safe...but allowable in the function call context
//     if (i.equals<mutable_view>()) return o.emplace<mutable_view>(g->name); // bad
//     return to_value<Goo const &>(); // what about a temporary &? not sure any use
// }


// bool to_value(Object &o, Index i, Goo const &g) {
//     if (i.equals<std::string_view>()) return o.emplace<std::string_view>(g.name), true; // safe
//     return to_value<Temporary<Goo const>>();
// }


// we'll probably take out "guaranteed" copy constructibility
std::optional<Goo> from_ref(Object &r, Type<Goo>) {
    std::optional<Goo> out;
    // move_as does the request and destroys itself in event of success
    // some of these would not need to destroy themselves though
    // (i.e.) if request is for a reference or a trivially_copyable type
    // maybe it is better to just say double *...?
    if (auto t = r.move_as<double>()) out.emplace(std::move(*t));
    if (auto t = r.move_as<double &>()) out.emplace(*t);
    if (auto t = r.move_as<double const &>()) out.emplace(*t);
    // not entirely sure what point of this one is...
    // well, I guess it is useful for C++ code.
    if (auto t = r.move_as<double &&>()) out.emplace(std::move(*t));
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