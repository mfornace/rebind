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

struct pyIndex : StaticType<pyIndex> {
    using type = IndexObject;
    using value_type = Index;
    // PyObject* new_(PyTypeObject *subtype, Ignore, Ignore) noexcept {
    //     PyObject* o = subtype->tp_alloc(subtype, 0); // 0 unused
    //     if (o) reinterpret_cast<Index*>(o)->reset(); // noexcept
    //     return o;
    // }

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
    static void placement_new(Index &t, Args &&...args) noexcept {
        DUMP("working right?");
        t = Index(std::forward<Args>(args)...);
        DUMP("OK...", t.integer());
    }


    static int as_bool(Always<pyIndex> i) noexcept {return bool(*i);}

    static Value<pyInt> as_int(Always<pyIndex> i) {
        DUMP("type", Ptr<pyType>{(~i)->ob_type}, reference_count(i));
        DUMP("index address2", &*i, &static_cast<Index&>(*i), i->integer());
        return pyInt::from(i->integer());}

    static void initialize_type(Always<pyType> o) noexcept {
        static PyNumberMethods NumberMethods = {
            .nb_bool = api<as_bool, Always<pyIndex>>,
            .nb_int = api<as_int, Always<pyIndex>>
        };
        define_type<pyIndex>(o, "ara.Index", "Index type");
        o->tp_repr = api<repr, Always<pyIndex>>;
        o->tp_hash = api<hash, Always<pyIndex>>;
        o->tp_str = api<str, Always<pyIndex>>;
        o->tp_as_number = &NumberMethods;
        o->tp_richcompare = api<compare<pyIndex>, Always<pyIndex>, Always<>, int>;
    };

    static Value<pyIndex> load(Ref &ref, Ignore, Ignore) {return {};}
};

/******************************************************************************/


// Ptr index_compare(Ptr self, Ptr other, int op) {
//     return raw_object([=]() -> Object {
//         return {compare(op, cast_object<Index>(self), cast_object<Index>(other)) ? Py_True : Py_False, true};
//     });
// }

/******************************************************************************/

}