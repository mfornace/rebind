#pragma once
#include "Wrap.h"
#include <ara/Ref.h>
#include <ara/Structs.h>
#include <ara-py/Builtins.h>
#include <ara-py/Variable.h>

#include <ara/Ref.h>
#include <ara/Core.h>

#include <unordered_map>

namespace ara::py {


/******************************************************************************/

// Unambiguous conversions from some basic C++ types to Objects



// Initialize an object that has a direct Python wrapped equivalent
// template <class T>
// Value<> as_exported_object(T t) {
//     auto o = Value<>::from(PyObject_CallObject(static_type<T>().object(), nullptr));
//     cast_object_unsafe<T>(+o) = std::move(t);
//     return o;
// }

// inline Value<> as_object(Index t) {
//     return as_exported_object(std::move(t));
// }

// inline Value as_object(BinaryView s) {return {PyByteArray_FromStringAndSize(reinterpret_cast<char const *>(s.data()), s.size()), false};}

// inline Value as_object(Binary const &s) {return {PyByteArray_FromStringAndSize(reinterpret_cast<char const *>(s.data()), s.size()), false};}

/******************************************************************************/

// Convert C++ Value to a requested Python type
// First explicit types are checked:
// None, object, bool, int, float, str, bytes, Index, list, tuple, dict, Value, Overload, memoryview
// Then, the output_conversions map is queried for Python function callable with the Value
Value<> try_load(Ref &r, Always<> t, Value<> root);

/******************************************************************************/


// template <class Self>
// PyObject* c_load(PyObject* self, PyObject* type) noexcept {
//     return raw_object([=]() -> Value {
//         DUMP("c_load ", typeid(Self).name(), bool(type));
//         auto acquired = acquire_ref(cast_object<Self>(self), LockType::Read);
//         if (auto out = try_load(acquired.ref, *type, Value())) return out;
//         return type_error("cannot convert value to type %R from %R", type, +as_object(acquired.ref.index()));
//     });
// }

/******************************************************************************/

std::string repr(Maybe<Object> o) {
    if (!o) return "null";
    auto r = Value<>::from(PyObject_Repr(+o));
#   if PY_MAJOR_VERSION > 2
        return PyUnicode_AsUTF8(~r); // PyErr_Clear
#   else
        return PyString_AsString(~r);
#   endif
}

/******************************************************************************/

// std::unordered_map<Value, Value> output_conversions;

/******************************************************************************/

// Value try_load(Ref &r, Instance<> t, Value root) {
//     DUMP("try_load", r.name(), "to type", repr(+t), "with root", repr(+root));
//     return map_output(t, [&](auto T) {
//         return decltype(T)::load(r, t, root);
//     });
// }

/******************************************************************************/

}