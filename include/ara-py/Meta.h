#pragma once
#include "Variable.h"
#include <deque>
#include <structmember.h>

namespace ara::py {

/******************************************************************************/

struct Method {
    Value<> tag;              // optional
    Value<pyStr> mode;        // optional
    Value<> out;              // optional

    Value<pyTuple> signature; // tuple of arguments
    Value<pyStr> docstring;   // output of repr()
    Value<pyStr> doc;         // output of str()

    bool gil = true;
    auto traverse() {return std::tie(tag, mode, out, signature, docstring, doc);}
    static constexpr std::string_view names[] = {"tag", "mode", "out", "signature", "docstring", "doc"};
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

    static Value<> getattr(Always<T> self, Always<> key) {
        if (auto m = Maybe<pyStr>(key)) {
            std::string_view const s = view_underlying(*m);
            auto const &names = T::type::names;
            auto const it = std::find(std::begin(names), std::end(names), s);
            if (it != std::end(names)) {
                Value<> out;
                auto const n = it - std::begin(names);
                std::apply([&](auto const &...ts) {
                    uint i = 0;
                    ((n == i++ ? (out = ts, 0) : 0), ...);
                }, self->traverse());
                if (!out) out = Always<>(*Py_None);
                return out;
            }
        }

        return Value<>::take(PyObject_GenericGetAttr(~self, ~key));
    }

    static Value<> get_names(Always<pyType> t, Ignore) {
        auto out = Value<pyList>::take(PyObject_Dir(~t));
        for (auto const &s : T::type::names)
            if (PyList_Append(~out, ~pyStr::from(s))) throw PythonError();
        return out;
    }

    static void set_flags(Always<pyType> o, bool dir) noexcept {
        static PyMethodDef methods[] = {
            {"__dir__", reinterpret<get_names, nullptr, Always<pyType>, Ignore>, METH_CLASS | METH_NOARGS,
                "__dir__(self)\n--\n\nreturn tuple of fields in self"},
            {nullptr, nullptr, 0, nullptr}
        };

        o->tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC;
        o->tp_clear = reinterpret<clear, -1, Always<T>>;
        o->tp_traverse = reinterpret<traverse, -1, Always<T>, visitproc, void*>;
        o->tp_getattro = reinterpret<getattr, nullptr, Always<T>, Always<>>;
        if (dir) o->tp_methods = methods;
    }
};

struct pyMethod : StaticType<pyMethod> {
    using type = Subtype<Method>;

    static Value<> repr(Always<pyMethod> self) {
        if (!self->doc) self->doc = pyStr::from("Method()");
        return self->doc;
    }

    static Value<> str(Always<pyMethod> self) {
        if (!self->docstring) self->docstring = pyStr::from("Method()");
        return self->docstring;
    }

    static Value<> get(Always<pyMethod> self, Maybe<> instance, Ignore);

    static Value<> call(Always<pyMethod> self, Always<pyTuple> args, Maybe<pyDict> kws) {
        return {};
    }

    static void initialize_type(Always<pyType> o) noexcept {
        define_type<pyMethod>(o, "ara.Method", "ara Method type");
        o->tp_repr = reinterpret<repr, nullptr, Always<pyMethod>>;
        o->tp_str = reinterpret<str, nullptr, Always<pyMethod>>;
        o->tp_call = reinterpret_kws<call, Always<pyMethod>>;
        o->tp_descr_get = reinterpret<get, nullptr, Always<pyMethod>, Maybe<>, Ignore>;
        GC<pyMethod>::set_flags(o, true);
        // PyMemberDef, tp_members
    }

    static void placement_new(Method &m, Maybe<> tag, Maybe<pyStr> mode, Maybe<pyTuple> signature, Maybe<> out, Maybe<pyStr> doc, Maybe<pyStr> docstring) {
        new(&m) Method{tag, mode, out, signature, doc, docstring};
    }

    static void placement_new(Method &m, Always<pyTuple> args, Maybe<pyDict> kws) {
        auto [tag, mode, sig, out, doc, docstring] = parse<0, Object, pyStr, pyTuple, Object, pyStr, pyStr>(
            args, kws, {"tag", "mode", "signature", "out", "doc", "docstring"});
        placement_new(m, tag, mode, sig, out, doc, docstring);
    }
};

/******************************************************************************/

struct BoundMethod {
    Bound<pyMethod> method;
    Bound<pyVariable> instance;
    auto traverse() {return std::tie(method, instance);}
    static constexpr std::string_view names[] = {"method", "instance"};
};

struct pyBoundMethod : StaticType<pyBoundMethod> {
    using type = Subtype<BoundMethod>;

    static void initialize_type(Always<pyType> t) {
        define_type<pyBoundMethod>(t, "ara.BoundMethod", "ara BoundMethod type");
        t->tp_call = reinterpret_kws<call, Always<pyBoundMethod>>;
        GC<pyBoundMethod>::set_flags(t, true);
    }

    static Value<> call(Always<pyBoundMethod>, Always<pyTuple>, Maybe<pyDict>);
    
    static void placement_new(BoundMethod &f, Always<pyMethod> m, Always<pyVariable> s) noexcept {
        new(&f) BoundMethod{m, s};
    }

    static void placement_new(BoundMethod &f, Always<pyTuple> args, Maybe<pyDict> kws) {
        auto [m, s] = parse<2, pyMethod, pyVariable>(args, kws, {"method", "instance"});
        placement_new(f, m, s);
    }
};

/******************************************************************************/

Value<> pyMethod::get(Always<pyMethod> self, Maybe<> instance, Ignore) {
    if (instance) {
        return Value<pyBoundMethod>::new_from(self, Always<pyVariable>::from(*instance));
    } else return self;
}

/******************************************************************************/

struct bind {
    Value<pyVariable> instance; // optional
    Value<> tag; // optional
    Value<pyStr> mode; // optional
    Value<> out; // optional
    auto traverse() {return std::tie(instance, tag, mode, out);}
    static constexpr std::string_view names[] = {"instance", "tag", "mode", "out"};
};

struct PyBind : StaticType<bind> {
    using type = Subtype<bind>;

    static void initialize_type(Always<pyType> t) {
        define_type<PyBind>(t, "ara.bind", "ara binding decorator");
        t->tp_call = reinterpret_kws<call, Always<PyBind>>;
        GC<PyBind>::set_flags(t, true);
    }

    static void placement_new(bind &f, Always<pyTuple> args, Maybe<pyDict> kws) {
        auto [mode, tag, out] = parse<1, pyStr, Object, Object>(args, kws, {"mode", "tag", "out"});
        new(&f) bind{{}, tag, mode, out};
    }

    static void placement_new(bind &f, Always<pyVariable> v, Maybe<> tag, Maybe<pyStr> mode, Maybe<> out) {
        new(&f) bind{v, tag, mode, out};
    }

    static Value<> call(Always<PyBind> f, Always<pyTuple> args, Maybe<pyDict> kws);
};

/******************************************************************************/

struct Member {
    Bound<pyStr> name;
    Value<> out; // optional
    Value<pyStr> doc; // optional
    auto traverse() {return std::tie(name, out, doc);}
    static constexpr std::string_view names[] = {"name", "out", "doc"};
};

struct PyMember : StaticType<Member> {
    using type = Subtype<Member>;

    static void initialize_type(Always<pyType> t) {
        define_type<PyMember>(t, "ara.Member", "ara Member type");
        t->tp_descr_get = reinterpret<get, nullptr, Always<PyMember>, Maybe<>, Ignore>;
        GC<PyMember>::set_flags(t, true);
    }

    static Value<> get(Always<PyMember> self, Maybe<> instance, Ignore);

    static void placement_new(Member &f, Always<pyTuple> args, Maybe<pyDict> kws) {
        auto [name, out, doc] = parse<1, pyStr, Object, pyStr>(args, kws, {"name", "out", "doc"});
        new(&f) Member{name, out, doc};
    }
};

/******************************************************************************/

}