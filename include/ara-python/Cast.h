/**
 * @file Cast.h
 * @brief
 */
#pragma once
#include "Wrap.h"
#include "Common.h"
#include <ara-cpp/Core.h>
#include <ara-cpp/Arrays.h>
#include <ara/Call.h>

namespace ara::py {

/******************************************************************************/

Object cast_to_object(Ref &&v, Object const &t, Object const &root);


template <class Self>
PyObject * c_cast(PyObject *self, PyObject *type) noexcept {
    return raw_object([=] {
        DUMP("c_cast ", typeid(Self).name());
        return cast_to_object(cast_object<Self>(self).as_ref(), Object(type, true), Object(self, true));
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
