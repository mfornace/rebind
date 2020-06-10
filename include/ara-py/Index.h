#pragma once
#include <ara/Index.h>
#include "Wrap.h"

namespace ara::py {

/******************************************************************************/

struct pyIndex : StaticType<pyIndex>, Index {
    using builtin = pyIndex;
    // PyObject* new_(PyTypeObject *subtype, Ignore, Ignore) noexcept {
    //     PyObject* o = subtype->tp_alloc(subtype, 0); // 0 unused
    //     if (o) reinterpret_cast<Index*>(o)->reset(); // noexcept
    //     return o;
    // }
    void init() {new (static_cast<Index*>(this)) Index{};}

    static long hash(Always<pyIndex> o) noexcept {
        return static_cast<long>(std::hash<Index>()(*o));
    }

    static PyObject* repr(Always<pyIndex> o) noexcept {
        return PyUnicode_FromFormat("Index('%s')", o->name().data());
    }

    static PyObject* str(Always<pyIndex> o) noexcept {
        return PyUnicode_FromString(o->name().data());
    }
};

/******************************************************************************/


// Ptr index_compare(Ptr self, Ptr other, int op) {
//     return raw_object([=]() -> Object {
//         return {compare(op, cast_object<Index>(self), cast_object<Index>(other)) ? Py_True : Py_False, true};
//     });
// }

template <>
void StaticType<pyIndex>::initialize(Always<pyType> o) noexcept {
    define_type<pyIndex>(o, "ara.Index", "Index type");
    (+o)->tp_repr = api<pyIndex::repr, Always<pyIndex>>;
    (+o)->tp_hash = api<pyIndex::hash, Always<pyIndex>>;
    (+o)->tp_str = api<pyIndex::str, Always<pyIndex>>;
    // o->tp_richcompare = index_compare;
    // no init (just use default constructor)
    // tp_traverse, tp_clear
    // PyMemberDef, tp_members
};

/******************************************************************************/

}