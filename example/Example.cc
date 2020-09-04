
#include <sfb/Call.h>
#include <sfb/Core.h>
#include <sfb-cpp/Schema.h>
#include <sfb-cpp/Standard.h>
#include <sfb-cpp/Array.h>
#include <sfb-cpp/Tuple.h>
#include <iostream>


#include <string_view>
#include <vector>
#include <string>
#include <any>
#include <mutex>

static_assert(16 == sizeof(std::shared_ptr<int>));
static_assert(16 == sizeof(std::string_view));
static_assert(24 == sizeof(std::string));
static_assert(32 == sizeof(std::any));
static_assert(24 == sizeof(std::vector<int>));
static_assert(8 == sizeof(std::unique_ptr<int>));
static_assert(16 == sizeof(std::shared_ptr<int>));
static_assert(8 == sizeof(std::optional<int>));

static_assert(std::is_trivially_copyable_v<std::optional<int>>);


using sfb::Type;
using sfb::Value;
using sfb::Schema;

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
// bool to_value(Object &o, Index i, Thing t) {
//     if (i.equals<std::string>()) {
//         if (auto r = t.rvalue()) return o.emplace<std::string>((*r).name);
//         return o.emplace<std::string>((*r).name);
//     }

//     if (i.equals<GooBase>()) {
//         if (auto r = t.rvalue()) o.emplace(*r);
//         else o.emplace(*t);
//     }

//     if (t.in_scope()) {
//         return t.member(&M::name)
//             || t.derive<GooBase>()
//             || (i.equals<std::string_view>() && o.emplace(r->name));
//             || (i.equals<mutable_string_view>()) && o.visit<mutable_string_view>() {
//             if (auto r = t.lvalue()) return (*r).name;
//         }

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
//     }
// }


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
// std::optional<Goo> from_ref(Object &r, Type<Goo>) {
//     std::optional<Goo> out;
//     // move_as does the request and destroys itself in event of success
//     // some of these would not need to destroy themselves though
//     // (i.e.) if request is for a reference or a trivially_copyable type
//     // maybe it is better to just say double *...?
//     if (auto t = r.move_as<double>()) out.emplace(std::move(*t));
//     if (auto t = r.move_as<double &>()) out.emplace(*t);
//     if (auto t = r.move_as<double const &>()) out.emplace(*t);
//     // not entirely sure what point of this one is...
//     // well, I guess it is useful for C++ code.
//     if (auto t = r.move_as<double &&>()) out.emplace(std::move(*t));
//     return out;
// }

/******************************************************************************/

template <>
struct sfb::Impl<GooBase> {
    static bool call(Frame) {return false;}
};

template <>
struct sfb::Impl<Goo> : sfb::Default<Goo> {
    // static auto attribute(AttributeFrame<Goo> f) {
    //     return f(&Goo::x, "x")
    //         || f(&Goo::x, "x2");
    // }

    static bool method(Frame f) {
        DUMP("calling Goo f", sfb::Lifetime({0}).value);
        return f([](double x){return Goo(x);},          "new")
            || f([](Goo g, Goo g2, Goo g3) {return g;}, "add")
            || f(&Goo::test_throw,                      "test_throw")
            || f([](Goo const &g) {return g.x;},        "get_x")
            || f([](Goo &g, double x) {g.x += x;},      "add")
            || f.derive<GooBase>();
    }
};

/******************************************************************************/

// could make this return a schema
Schema make_schema() {
    Schema s;
    s.object("global_value", 123);
    s.function("easy", [] {
        DUMP("invoking easy");
        return 1.2;
    });
    s.function("fun", [](int i, double d) {
        DUMP("fun", i, d);
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
    s.function("lref2", [](double &i) -> double & {return i = 2;});
    s.function("lref3", [](double const &i) -> double const & {return i;});
    s.function("clref", [](double const &i) {});
    s.function("noref", [](double i) {});
    s.function("rref", [](double &&i) {});
    s.function("str_argument", [](std::string_view s) {DUMP("string =", s); return std::string(s);});
    s.function("string_argument", [](std::string s) {DUMP("string =", s); return s;});
    s.function("stringref_argument", [](std::string const &s) {DUMP("string =", s); return s;});

    s.object("Goo", sfb::Index::of<Goo>());

    s.function("vec1", [](std::vector<int> const &) {});
    s.function("vec2", [](std::vector<int> &) {});
    s.function("vec3", [](std::vector<int>) {});
    s.function("mutex", [] {return std::mutex();});
    s.function("bool", [](bool b) {return b;});
    s.function("dict", [](std::map<std::string, std::string> b) {
        DUMP("map size", b.size());
        for (auto const &p : b) DUMP("map item", p.first, p.second);
        return b;
    });

    DUMP("made schema");
    return s;
}

struct Example {};

template <>
struct sfb::Impl<Example> : sfb::Default<Example> {
    static bool call(sfb::Frame frame) {
        return frame([](Example const &self) {return make_schema();});
    }
};

struct Test {};


template <>
struct sfb::Impl<Test> {
    static sfb::Name::stat name(sfb::Str&)                                     noexcept {return {};}
    static sfb::Info::stat info(sfb::Index&, void const*& s)                   noexcept {return {};}
    static sfb::Call::stat call_nothrow(Target&, ArgView&)                     noexcept {return {};} // combine with method?
    static sfb::Load::stat load_nothrow(Target&, Pointer, Mode)                noexcept {return {};}
    static sfb::Dump::stat dump_nothrow(Target&, Pointer, Mode)                noexcept {return {};}
    static sfb::Equal::stat equal_nothrow(Test const&, Test const&)            noexcept {return {};} // no cross type comparison... (at least Str to String would be good...)
    static sfb::Compare::stat compare_nothrow(Test const&, Test const&)        noexcept {return {};} // no cross type comparison... (at least Str to String would be good...)
    static sfb::Attribute::stat attribute_nothrow(Target&, Pointer, Str, Mode) noexcept {return {};} // too specific? make static possible?
    static sfb::Element::stat element_nothrow(Target&, Pointer, Integer, Mode) noexcept {return {};} // too specific?
    static sfb::Hash::stat hash_nothrow(std::size_t&, Test const&)             noexcept {return {};}
    static sfb::Swap::stat swap(Test&, Test&)                                  noexcept {return {};}
    static sfb::Relocate::stat relocate(void*, Test&&)                         noexcept {return {};}
    static sfb::Copy::stat copy(Target&, Test const&)                          noexcept {return {};}
    static sfb::Deallocate::stat deallocate(Test&)                             noexcept {return {};}
    static sfb::Destruct::stat destruct(Test&)                                 noexcept {return {};}
};


SFB_DEFINE(example_test, Test);
SFB_DECLARE(example_boot, Example);
SFB_DEFINE(example_boot, Example);


