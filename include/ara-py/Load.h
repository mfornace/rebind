#include "Wrap.h"

namespace ara::py {

/******************************************************************************/

// Unambiguous conversions from some basic C++ types to Objects

inline Object as_object(Object o) {return std::move(o);}

inline Object as_object(bool b) {return {b ? Py_True : Py_False, true};}

// inline Object as_object(Integer i) {return {PyLong_FromLongLong(static_cast<long long>(i)), false};}

// inline Object as_object(Real x) {return {PyFloat_FromDouble(x), false};}

inline Object as_object(std::string const &s) {return {PyUnicode_FromStringAndSize(s.data(), s.size()), false};}

inline Object as_object(std::string_view s) {return {PyUnicode_FromStringAndSize(s.data(), s.size()), false};}

// inline Object as_object(BinaryView s) {return {PyByteArray_FromStringAndSize(reinterpret_cast<char const *>(s.data()), s.size()), false};}

// inline Object as_object(Binary const &s) {return {PyByteArray_FromStringAndSize(reinterpret_cast<char const *>(s.data()), s.size()), false};}

/******************************************************************************/

// Convert C++ Value to a requested Python type
// First explicit types are checked:
// None, object, bool, int, float, str, bytes, Index, list, tuple, dict, Value, Overload, memoryview
// Then, the output_conversions map is queried for Python function callable with the Value
Object try_load(Ref &r, Ptr t, Ptr root);

/******************************************************************************/

}