/**
 * @brief Python-related C++ API for rebind
 * @file PythonAPI.h
 */

#pragma once
#include "Object.h"

// #include <rebind-cpp/Schema.h>
// #include <rebind-cpp/Core.h>
// #include <rebind/types/Standard.h>
#include <rebind-cpp/Arrays.h>

#include <mutex>
#include <unordered_map>


namespace rebind::py {

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
    rebind_array data;
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

std::string_view from_unicode(PyObject *o);

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

/// RAII release of Python GIL
struct PythonFrame final : Frame {
    std::mutex mutex;
    PyThreadState *state = nullptr;
    bool no_gil;

    explicit PythonFrame(bool no_gil) : no_gil(no_gil) {}

    void enter() override { // noexcept
        DUMP("running with nogil=", no_gil);
        // release GIL and save the thread
        if (no_gil && !state) state = PyEval_SaveThread();
    }

    // std::shared_ptr<Frame> new_frame(std::shared_ptr<Frame> t) noexcept override {
    //     DUMP("suspended Python ", bool(t));
    //     // if we already saved the python thread state, return this
    //     if (no_gil || state) return std::move(t);
    //     // return a new frame that can be entered
    //     else return std::make_shared<PythonFrame>(no_gil);
    // }

    // acquire GIL; lock mutex to prevent multiple threads trying to get the thread going
    void acquire() noexcept {if (state) {mutex.lock(); PyEval_RestoreThread(state);}}

    // release GIL; unlock mutex
    void release() noexcept {if (state) {state = PyEval_SaveThread(); mutex.unlock();}}

    // reacquire the GIL and restart the thread, if required
    ~PythonFrame() {if (state) PyEval_RestoreThread(state);}
};

/******************************************************************************/

/// RAII acquisition/release of Python GIL
struct PythonGuard {
    PythonFrame &lock;

    PythonGuard(PythonFrame &u) : lock(u) {lock.acquire();}
    ~PythonGuard() {lock.release();}
};

/******************************************************************************/

std::string_view get_type_name(Index idx) noexcept;

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
