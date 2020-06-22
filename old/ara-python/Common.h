/**
 * @brief Python-related C++ API for ara
 * @file PythonAPI.h
 */

#pragma once
#include "Object.h"

// #include <ara-cpp/Schema.h>
// #include <ara-cpp/Core.h>
// #include <ara/types/Standard.h>
#include <ara-cpp/Arrays.h>

#include <mutex>
#include <unordered_map>


namespace ara::py {

/******************************************************************************/

extern Object TypeError, UnionType;

extern std::unordered_map<Object, Object> output_conversions, input_conversions, type_translations;

extern std::unordered_map<Index, std::pair<Object, Object>> python_types;

/******************************************************************************/

template <class T>
using Vector = std::vector<T>;

enum class Scalar {Bool, Char, SignedChar, UnsignedChar, Unsigned, Signed, Float, Pointer};

extern Vector<std::tuple<Scalar, Index, unsigned>> scalars;

// template <class T, class ...Ts>
// Vector<T> vectorize(Ts &&...ts) {
//     Vector<T> out;
//     out.reserve(sizeof...(Ts));
//     (out.emplace_back(static_cast<Ts &&>(ts)), ...);
//     return out;
// }

// template <class T, class V, class F>
// Vector<T> mapped(V const &v, F &&f) {
//     Vector<T> out;
//     out.reserve(std::size(v));
//     for (auto &&x : v) out.emplace_back(f(x));
//     return out;
// }

/******************************************************************************/

template <class ...Ts>
std::nullptr_t type_error(char const *s, Ts ...ts) {PyErr_Format(TypeError, s, ts...); return nullptr;}

/******************************************************************************/

struct ArrayBuffer {
    std::vector<Py_ssize_t> shape_stride;
    ara_array data;
    // std::size_t n_elem;
    Object base;
    // Ref data;

    ArrayBuffer() noexcept = default;

    ArrayBuffer(ArrayView const &a, Object const &b) {}
    // : n_elem(a.layout.n_elem()),
    //     base(b), data(a.data) {
    //     for (std::size_t i = 0; i != a.layout.depth(); ++i)
    //         shape_stride.emplace_back(a.layout.shape(i));
    //     auto const item = Buffer::itemsize(data.index());
    //     for (std::size_t i = 0; i != a.layout.depth(); ++i)
    //         shape_stride.emplace_back(a.layout.stride(i) * item);
    // }
};

/******************************************************************************/

template <class V, class F>
Object map_as_tuple(V &&v, F &&f) noexcept {
    auto out = Object::from(PyTuple_New(std::size(v)));
    auto it = std::begin(v);
    using T = std::conditional_t<std::is_rvalue_reference_v<V>,
        decltype(*it), decltype(std::move(*it))>;
    for (Py_ssize_t i = 0u; i != std::size(v); ++i, ++it)
        if (!set_tuple_item(out, i, f(static_cast<T>(*it)))) return {};
    return out;
}

template <class ...Ts>
Object tuple_from(Ts &&...ts) {
    auto out = Object::from(PyTuple_New(sizeof...(Ts)));
    Py_ssize_t i = 0;
    auto go = [&](Object const &x) {return set_tuple_item(out, i++, x);};
    return (go(ts) && ...) ? out : Object();
}

/******************************************************************************/

template <class F>
void map_iterable(Object iterable, F &&f) {
    auto iter = Object::from(PyObject_GetIter(+iterable));
    while (true) {
        if (auto it = Object(PyIter_Next(+iter), false)) f(std::move(it));
        else return;
    }
}


/******************************************************************************/

/// RAII acquisition/release of Python GIL
struct PythonGuard {
    PythonFrame &lock;

    PythonGuard(PythonFrame &u) : lock(u) {lock.acquire();}
    ~PythonGuard() {lock.release();}
};

/******************************************************************************/

std::string wrong_type_message(WrongType const &e, std::string_view={});

/******************************************************************************/

template <class F>
PyObject *raw_object(F &&f) noexcept {
    try {
        Object o = static_cast<F &&>(f)();
        xincref(+o);
        return +o;
    } catch (PythonError const &) {
        return nullptr;
    } catch (std::bad_alloc const &e) {
        PyErr_SetString(PyExc_MemoryError, "C++: out of memory (std::bad_alloc)");
    } catch (WrongNumber const &e) {
        unsigned int n0 = e.expected, n = e.received;
        PyErr_Format(TypeError, "C++: wrong number of arguments (expected %u, got %u)", n0, n);
    } catch (WrongType const &e) {
        try {PyErr_SetString(TypeError, wrong_type_message(e, "C++: ").c_str());}
        catch(...) {PyErr_SetString(TypeError, e.what());}
    } catch (std::exception const &e) {
        if (!PyErr_Occurred())
            PyErr_Format(PyExc_RuntimeError, "C++: %s", e.what());
    } catch (...) {
        if (!PyErr_Occurred())
            PyErr_SetString(PyExc_RuntimeError, unknown_exception_description());
    }
    return nullptr;
}

}

/******************************************************************************/
