#pragma once
#include "Wrap.h"
#include <ara/Ref.h>

namespace ara::py {


/******************************************************************************/

// Unambiguous conversions from some basic C++ types to Objects

inline Shared as_object(Shared o) {return std::move(o);}

inline Shared as_object(bool b) {return {b ? Py_True : Py_False, true};}

inline Shared as_object(Integer i) {return Shared::from(PyLong_FromLongLong(static_cast<long long>(i)));}

inline Shared as_object(ara_float x) {return Shared::from(PyFloat_FromDouble(x));}

inline Shared as_object(std::string const &s) {return Shared::from(PyUnicode_FromStringAndSize(s.data(), s.size()));}

inline Shared as_object(std::string_view s) {return Shared::from(PyUnicode_FromStringAndSize(s.data(), s.size()));}

// Initialize an object that has a direct Python wrapped equivalent
template <class T>
Shared as_exported_object(T t) {
    auto o = Shared::from(PyObject_CallObject(static_type<T>().object(), nullptr));
    cast_object_unsafe<T>(+o) = std::move(t);
    return o;
}

inline Shared as_object(Index t) {
    return as_exported_object(std::move(t));
}

// inline Shared as_object(BinaryView s) {return {PyByteArray_FromStringAndSize(reinterpret_cast<char const *>(s.data()), s.size()), false};}

// inline Shared as_object(Binary const &s) {return {PyByteArray_FromStringAndSize(reinterpret_cast<char const *>(s.data()), s.size()), false};}

/******************************************************************************/

// Convert C++ Value to a requested Python type
// First explicit types are checked:
// None, object, bool, int, float, str, bytes, Index, list, tuple, dict, Value, Overload, memoryview
// Then, the output_conversions map is queried for Python function callable with the Value
Shared try_load(Ref &r, Instance<> t, Shared root);

/******************************************************************************/


template <class Self>
PyObject* c_load(PyObject* self, PyObject* type) noexcept {
    return raw_object([=]() -> Shared {
        DUMP("c_load ", typeid(Self).name(), bool(type));
        auto acquired = acquire_ref(cast_object<Self>(self), true, true);
        if (auto out = try_load(acquired.ref, instance(type), Shared())) return out;
        return type_error("cannot convert value to type %R from %R", type, +as_object(acquired.ref.index()));
    });
}


}