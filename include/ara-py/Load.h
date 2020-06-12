#pragma once
#include "Wrap.h"
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

/******************************************************************************/


/******************************************************************************/

template <class F>
Value<> map_output(Always<> t, F &&f) {
    // Type objects
    // return {};
    if (auto type = Maybe<pyType>(t)) {
        if (pyNone::matches(*type))  return f(pyNone());
        if (pyBool::matches(*type))  return f(pyBool());
        if (pyInt::matches(*type))   return f(pyInt());
        if (pyFloat::matches(*type)) return f(pyFloat());
        if (pyStr::matches(*type))   return f(pyStr());
        if (pyBytes::matches(*type)) return f(pyBytes());
        if (pyIndex::matches(*type)) return f(pyIndex());
// //         else if (*type == &PyBaseObject_Type)                  return as_deduced_object(std::move(r));        // object
//         // if (*type == static_type<VariableType>()) return f(VariableType());           // Value
    }
    // Non type objects
    DUMP("Not a type");
//     // if (auto p = instance<Index>(t)) return f(Index());  // Index

//     if (pyUnion::matches(t)) return f(pyUnion());
//     if (pyList::matches(t))  return f(pyList());       // pyList[T] for some T (compound def)
//     if (pyTuple::matches(t)) return f(pyTuple());      // pyTuple[Ts...] for some Ts... (compound def)
//     if (pyDict::matches(t))  return f(pyDict());       // pyDict[K, V] for some K, V (compound def)
//     DUMP("Not one of the structure types");
    return {};
//     DUMP("custom convert ", output_conversions.size());
//     if (auto p = output_conversions.find(Value<>{t, true}); p != output_conversions.end()) {
//         DUMP(" conversion ");
//         Value<> o;// = ref_to_object(std::move(r), {});
//         if (!o) return type_error("could not cast value to Python object");
//         DUMP("calling function");
// //         // auto &obj = cast_object<Variable>(o).ward;
// //         // if (!obj) obj = root;
//         return Value<>::from(PyObject_CallFunctionObjArgs(+p->second, +o, nullptr));
//     }
}

/******************************************************************************/

}