/**
 * @file Cast.h
 * @brief
 */
#pragma once
#include "Wrap.h"
#include "API.h"
#include <rebind-cpp/Core.h>
#include <rebind-cpp/Arrays.h>
#include <rebind/Function.h>

namespace rebind::py {

/******************************************************************************/

Object cast_to_object(Ref const &v, Object const &t, Object const &root);


template <class Self>
PyObject * c_cast(PyObject *self, PyObject *type) noexcept {
    return raw_object([=] {
        // Ref ptr(cast_object<Self>(self));
        Ref ptr;
        DUMP("c_cast ", typeid(Self).name(), " ", ptr.name());
        return cast_to_object(ptr, Object(type, true), Object(self, true));
    });
}

// template <class T>
// Object cast_to_object(T &&v, Object const &t, Object const &root) {
//     if constexpr(std::is_same_v<unqualified<T>, Value>) {

//     } else {
//         return cast_to_object(Ref(static_cast<T &&>(t)))
//     }
// }

/******************************************************************************/

// Unambiguous conversions from some basic C++ types to Objects

inline Object as_object(Object o) {return std::move(o);}

inline Object as_object(bool b) {return {b ? Py_True : Py_False, true};}

inline Object as_object(Integer i) {return {PyLong_FromLongLong(static_cast<long long>(i)), false};}

inline Object as_object(Real x) {return {PyFloat_FromDouble(x), false};}

inline Object as_object(std::string const &s) {return {PyUnicode_FromStringAndSize(s.data(), s.size()), false};}

inline Object as_object(std::string_view s) {return {PyUnicode_FromStringAndSize(s.data(), s.size()), false};}

inline Object as_object(BinaryView s) {return {PyByteArray_FromStringAndSize(reinterpret_cast<char const *>(s.data()), s.size()), false};}

inline Object as_object(Binary const &s) {return {PyByteArray_FromStringAndSize(reinterpret_cast<char const *>(s.data()), s.size()), false};}

/******************************************************************************/

// Initialize an object that has a direct Python wrapped equivalent
template <class T>
Object default_object(T t) {
    auto o = Object::from(PyObject_CallObject(type_object<T>(), nullptr));
    cast_object<T>(o) = std::move(t);
    return o;
}

inline Object as_object(Index t) {return default_object(std::move(t));}

/******************************************************************************/

/// Source driven conversion: guess the correct Python type from the source type
/// I guess this is where automatic class conversions should be done?
inline Object as_deduced_object(Ref &&ref) {
    DUMP("asking for object");
    Scope s;
    if (!ref) return {Py_None, true};
    if (auto v = ref.load<Object>(s))           return std::move(*v);
    if (auto v = ref.load<Real>(s))             return as_object(std::move(*v));
    if (auto v = ref.load<Integer>(s))          return as_object(std::move(*v));
    if (auto v = ref.load<bool>(s))             return as_object(std::move(*v));
    if (auto v = ref.load<std::string_view>(s)) return as_object(std::move(*v));
    if (auto v = ref.load<std::string>(s))      return as_object(std::move(*v));
    // if (auto v = ref.load<Function>(s))         return as_object(std::move(*v));
    if (auto v = ref.load<Index>(s))            return as_object(std::move(*v));
    if (auto v = ref.load<Binary>(s))           return as_object(std::move(*v));
    if (auto v = ref.load<BinaryView>(s))       return as_object(std::move(*v));
    // if (auto v = ref.load<ArgView>(s)) return map_as_tuple(*v, [](Ref const &x) {return as_deduced_object(x);});
    if (auto v = ref.load<Sequence>(s))  return map_as_tuple(*v, [](Value &&x) {return as_deduced_object(Ref(std::move(x)));});
    return {};
}

/******************************************************************************/

}
