#pragma once
#include "Wrap.h"
#include <ara/Ref.h>

namespace ara::py {

/******************************************************************************/

// Unambiguous conversions from some basic C++ types to Objects

inline Object as_object(Object o) {return std::move(o);}

inline Object as_object(bool b) {return {b ? Py_True : Py_False, true};}

inline Object as_object(Integer i) {return Object::from(PyLong_FromLongLong(static_cast<long long>(i)));}

inline Object as_object(Float x) {return Object::from(PyFloat_FromDouble(x));}

inline Object as_object(std::string const &s) {return Object::from(PyUnicode_FromStringAndSize(s.data(), s.size()));}

inline Object as_object(std::string_view s) {return Object::from(PyUnicode_FromStringAndSize(s.data(), s.size()));}

// Initialize an object that has a direct Python wrapped equivalent
template <class T>
Object default_object(T t) {
    DUMP("make Index");
    auto o = Object::from(PyObject_CallObject(TypePtr::from<T>(), nullptr));
    DUMP("huh");
    cast_object<T>(+o) = std::move(t);
    DUMP("huh2", t.name());
    return o;
}

inline Object as_object(Index t) {return default_object(std::move(t));}

// inline Object as_object(BinaryView s) {return {PyByteArray_FromStringAndSize(reinterpret_cast<char const *>(s.data()), s.size()), false};}

// inline Object as_object(Binary const &s) {return {PyByteArray_FromStringAndSize(reinterpret_cast<char const *>(s.data()), s.size()), false};}

/******************************************************************************/

// Convert C++ Value to a requested Python type
// First explicit types are checked:
// None, object, bool, int, float, str, bytes, Index, list, tuple, dict, Value, Overload, memoryview
// Then, the output_conversions map is queried for Python function callable with the Value
Object try_load(Ref &r, Ptr t, Object root);

/******************************************************************************/


template <class Self>
Ptr c_load(Ptr self, Ptr type) noexcept {
    return raw_object([=]() -> Object {
        DUMP("c_load ", typeid(Self).name());
        Ref ref = cast_object<Self>(self).as_ref();
        if (auto out = try_load(ref, type, Object())) return out;
        return type_error("cannot convert value to type %R from %R", type, +as_object(ref.index()));
    });
}


}