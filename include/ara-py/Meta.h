#pragma once
#include "Variable.h"
#include <deque>
#include <structmember.h>

namespace ara::py {

/******************************************************************************/

struct method {
    Value<> tag;       // optional
    Value<pyStr> mode; // optional
    Value<> out;       // optional
    // Value<> signature;
    // Value<> docstring;
    // Value<> doc;
    auto traverse() {return std::tie(tag, mode, out);}
};

template <class T>
struct GC {
    static int clear(Always<T> self) noexcept {
        using V = typename T::type;
        self->~V();
        return 0;
    }

    template <class O>
    static bool traverse_each(int &out, O &o, visitproc visit, void* arg) noexcept {
        if (o) {
            int stat = visit(~o, arg);
            return stat != 0;
        }
        return false;
    }

    static int traverse(Always<T> self, visitproc visit, void* arg) noexcept {
        int out = 0;
        std::apply([&](auto &...t) {
            (traverse_each(out, t, visit, arg) || ...);
        }, self->traverse());
        return out;
    }

    static void set_flags(Always<pyType> o) noexcept {
        o->tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC;
        o->tp_clear = reinterpret<clear, -1, Always<T>>;
        o->tp_traverse = reinterpret<traverse, -1, Always<T>, visitproc, void*>;
    }
};

struct py_method : StaticType<py_method> {
    using type = Subtype<method>;

    // static Value<> repr(Always<py_method> self) {return Always<>(*self->doc);}

    // static Value<> str(Always<py_method> self) {return Always<>(*self->docstring);}

    static Value<> get(Always<py_method> self, Maybe<> instance, Ignore);

    static Value<> call(Always<py_method> self, Always<pyTuple> args, Maybe<pyDict> kws) {
        return {};
    }

    static void initialize_type(Always<pyType> o) noexcept {
        define_type<py_method>(o, "ara.method", "ara method type");
        // o->tp_repr = reinterpret<repr, Always<py_method>>;
        // o->tp_str = reinterpret<str, Always<py_method>>;
        o->tp_call = reinterpret_kws<call, Always<py_method>>;
        o->tp_descr_get = reinterpret<get, nullptr, Always<py_method>, Maybe<>, Ignore>;
        GC<py_method>::set_flags(o);
        // PyMemberDef, tp_members
    }

    static void placement_new(method &m, Maybe<> tag, Maybe<pyStr> mode, Maybe<> out) {
        new(&m) method{tag, mode, out};
    }

    static void placement_new(method &m, Always<pyTuple> args, Maybe<pyDict> kws) {
        auto [tag, mode, out] = parse<0, Object, pyStr, Object>(args, kws, {"tag", "mode", "out"});
        placement_new(m, tag, mode, out);
    }
};

/******************************************************************************/

struct bound_method {
    Bound<py_method> method;
    Bound<py_variable> instance;
    auto traverse() {return std::tie(method, instance);}
};

struct py_bound_method : StaticType<py_bound_method> {
    using type = Subtype<bound_method>;

    static void initialize_type(Always<pyType> t) {
        define_type<py_bound_method>(t, "ara.bound_method", "ara bound_method type");
        t->tp_call = reinterpret_kws<call, Always<py_bound_method>>;
        GC<py_bound_method>::set_flags(t);
    }

    static Value<> call(Always<py_bound_method>, Always<pyTuple>, Maybe<pyDict>);
    
    static void placement_new(bound_method &f, Always<py_method> m, Always<py_variable> s) noexcept {
        new(&f) bound_method{m, s};
    }

    static void placement_new(bound_method &f, Always<pyTuple> args, Maybe<pyDict> kws) {
        auto [m, s] = parse<2, py_method, py_variable>(args, kws, {"method", "instance"});
        placement_new(f, m, s);
    }
};

/******************************************************************************/

Value<> py_method::get(Always<py_method> self, Maybe<> instance, Ignore) {
    if (instance) {
        return Value<py_bound_method>::new_from(self, Always<py_variable>::from(*instance));
    } else return self;
}

/******************************************************************************/

struct bind {
    Value<py_variable> instance; // optional
    Value<> tag; // optional
    Value<pyStr> mode; // optional
    Value<> out; // optional
    auto traverse() {return std::tie(instance, tag, mode, out);}
};

struct py_bind : StaticType<bind> {
    using type = Subtype<bind>;

    static void initialize_type(Always<pyType> t) {
        define_type<py_bind>(t, "ara.bind", "ara binding decorator");
        t->tp_call = reinterpret_kws<call, Always<py_bind>>;
        GC<py_bind>::set_flags(t);
    }

    static void placement_new(bind &f, Always<pyTuple> args, Maybe<pyDict> kws) {
        auto [mode, tag, out] = parse<1, pyStr, Object, Object>(args, kws, {"mode", "tag", "out"});
        new(&f) bind{{}, tag, mode, out};
    }

    static void placement_new(bind &f, Always<py_variable> v, Maybe<> tag, Maybe<pyStr> mode, Maybe<> out) {
        new(&f) bind{v, tag, mode, out};
    }

    static Value<> call(Always<py_bind> f, Always<pyTuple> args, Maybe<pyDict> kws);
};

/******************************************************************************/

struct Member {
    Bound<pyStr> name;
    Value<> out; // optional
    Value<pyStr> doc; // optional
    auto traverse() {return std::tie(name, out, doc);}
};

struct py_member : StaticType<Member> {
    using type = Subtype<Member>;

    static void initialize_type(Always<pyType> t) {
        define_type<py_member>(t, "ara.Member", "ara Member type");
        t->tp_descr_get = reinterpret<get, nullptr, Always<py_member>, Maybe<>, Ignore>;
        GC<py_member>::set_flags(t);
    }

    static Value<> get(Always<py_member> self, Maybe<> instance, Ignore);

    static void placement_new(Member &f, Always<pyTuple> args, Maybe<pyDict> kws) {
        auto [name, out, doc] = parse<1, pyStr, Object, pyStr>(args, kws, {"name", "out", "doc"});
        new(&f) Member{name, out, doc};
    }
};

/******************************************************************************/

}