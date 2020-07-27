#pragma once
#include <ara/Index.h>
#include "Wrap.h"
#include "Builtins.h"

namespace ara::py {

/******************************************************************************/

struct IndexObject : ObjectBase, Index {};

template <class T, class U>
bool compare_bool(T const& t, U const& u, int op) {
    switch (op) {
        case Py_EQ: return t == u;
        case Py_NE: return t != u;
        case Py_LT: return t < u;
        case Py_GT: return t > u;
        case Py_LE: return t <= u;
        case Py_GE: return t >= u;
        default: return false;
    }
}

template <class T>
Value<> compare(Always<T> self, Always<> other, int op) {
    if (auto o = Maybe<T>(other))
        return pyBool::from(compare_bool(*self, *o, op));
    else return Always<>(*Py_NotImplemented);
}

/******************************************************************************/

struct pyIndex : StaticType<pyIndex> {
    using type = IndexObject;

    static long hash(Always<pyIndex> o) noexcept {
        return static_cast<long>(std::hash<Index>()(*o));
    }

    static Value<> repr(Always<pyIndex> o) noexcept {
        return {PyUnicode_FromFormat("Index('%s')", o->name().data()), false};
    }

    static Value<> str(Always<pyIndex> o) {
        return Value<>::take(PyUnicode_FromString(o->name().data()));//, false};
    }

    template <class ...Args>
    static void placement_new(Index &t, Args &&...args) noexcept {t = Index(std::forward<Args>(args)...);}

    static int as_bool(Always<pyIndex> i) noexcept {return bool(*i);}

    static Value<pyInt> as_int(Always<pyIndex> i) {return pyInt::from(i->integer());}

    static PyNumberMethods number_methods;

    static void initialize_type(Always<pyType> o) noexcept;

    static Value<pyIndex> load(Ref &ref, Ignore, Ignore) {return {};}

    static Value<> call(Index, Always<pyTuple>, CallKeywords&&);

    static Value<pyIndex> from_address(Always<>) {
        return {};
    }

    static Value<pyIndex> from_file(Always<pyTuple>) {
        auto ctypes = Object::take(Py_import_module("ctypes"));
        auto cdll = getattr(ctypes, "CDLL");
        auto lib = call_object(cdll, args[0]);
        auto fun = getattr(lib, args[1]);
        setattr(fun, "restype", getattr(ctypes, "void_p"));
        return from_address(call_object(fun));
    }
};

/******************************************************************************/

}