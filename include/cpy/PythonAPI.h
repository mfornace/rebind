/**
 * @brief Python-related C++ API for cpy
 * @file PythonAPI.h
 */

#pragma once
#include "Document.h"

#include <mutex>
#include <complex>
#include <iostream>
#include <variant>
#include <unordered_map>
#include <map>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wregister"
#include <Python.h>
#pragma GCC diagnostic pop

namespace cpy {

extern PyTypeObject FunctionType, WrapType, TypeIndexType;
extern std::unordered_map<std::type_index, std::string> type_names;

enum class Scalar {Bool, Char, SignedChar, UnsignedChar, Unsigned, Signed, Float, Pointer};
extern Zip<Scalar, std::type_index, unsigned> scalars;

/******************************************************************************/

template <class T>
struct Holder {
    PyObject_HEAD // 16 bytes for the ref count and the type object
    T value; // I think stack is OK because this object is only casted to anyway.
};

/******************************************************************************/

struct PythonError : ClientError {
    PythonError(char const *s) : ClientError(s) {}
};

PythonError python_error(std::nullptr_t=nullptr) noexcept;

template <class ...Ts>
std::nullptr_t type_error(char const *s, Ts ...ts) {PyErr_Format(PyExc_TypeError, s, ts...); return nullptr;}

/******************************************************************************/

struct WeakReference {
    Reference ref;
    std::weak_ptr<void> handle;
    bool has_value() const {return !handle.expired() && ref.has_value();}
    auto type() const {return ref.type();}
    Reference & lock() {
        if (auto p = handle.lock()) return ref;
        else throw python_error(type_error("expired reference"));
    }
};

using Wrap = std::variant<Value, WeakReference>;

struct Object {
    PyObject *ptr = nullptr;
    Object() = default;
    Object(std::nullptr_t) {}
    Object(PyObject *o, bool incref) : ptr(o) {if (incref) Py_INCREF(ptr);}
    Object(Object const &o) : ptr(o.ptr) {Py_XINCREF(ptr);}
    Object(Object &&o) noexcept : ptr(std::exchange(o.ptr, nullptr)) {}
    Object & operator=(Object o) noexcept {ptr = std::exchange(o.ptr, nullptr); return *this;}

    explicit operator bool() const {return ptr;}
    explicit operator PyObject *() const {return ptr;}
    PyObject *operator+() const {return ptr;}

    bool operator<(Object const &o) const {return ptr < o.ptr;}
    bool operator>(Object const &o) const {return ptr > o.ptr;}
    bool operator==(Object const &o) const {return ptr == o.ptr;}
    bool operator!=(Object const &o) const {return ptr != o.ptr;}
    bool operator<=(Object const &o) const {return ptr <= o.ptr;}
    bool operator>=(Object const &o) const {return ptr >= o.ptr;}
    friend void swap(Object &o, Object &p) {std::swap(o.ptr, p.ptr);}

    ~Object() {Py_XDECREF(ptr);}
};

extern std::map<Object, Object> type_conversions;

/******************************************************************************/

struct Buffer {
    static Zip<std::string_view, std::type_index> formats;
    Py_buffer view;
    bool ok;

    Buffer(PyObject *o, int flags) {ok = PyObject_GetBuffer(o, &view, flags) == 0;}

    static std::type_index format(std::string_view s);
    static Binary binary(Py_buffer *view, std::size_t len);
    static Value binary_view(Py_buffer *view, std::size_t len);

    ~Buffer() {PyBuffer_Release(&view);}
};

/******************************************************************************/

inline PyObject * type_object(PyTypeObject &o) noexcept {return reinterpret_cast<PyObject *>(&o);}

inline PyTypeObject & type_ref(Type<Function>) {return FunctionType;}
inline PyTypeObject & type_ref(Type<Wrap>) {return WrapType;}
inline PyTypeObject & type_ref(Type<std::type_index>) {return TypeIndexType;}

/******************************************************************************/

template <class T>
T * cast_if(PyObject *o) {
    if constexpr(std::is_same_v<T, Wrap>)
        if (!PyObject_TypeCheck(o, &WrapType)) return nullptr;
    if constexpr(std::is_same_v<T, Function>)
        if (!PyObject_TypeCheck(o, &FunctionType)) return nullptr;
    if constexpr(std::is_same_v<T, std::type_index>)
        if (!PyObject_TypeCheck(o, &TypeIndexType)) return nullptr;
    return std::addressof(reinterpret_cast<Holder<T> *>(o)->value);
}

template <class T>
T & cast_object(PyObject *o) {
    static_assert(std::is_same_v<T, Wrap> || std::is_same_v<T, std::type_index> || std::is_same_v<T, Function>);
    if (std::is_same_v<T, Wrap>) {
        if (!PyObject_TypeCheck(o, &WrapType)) throw std::invalid_argument("Expected instance of cpy.Value");
    }
    if (std::is_same_v<T, Function>) {
        if (!PyObject_TypeCheck(o, &FunctionType)) throw std::invalid_argument("Expected instance of cpy.Function");
    }
    if (std::is_same_v<T, std::type_index>) {
        if (!PyObject_TypeCheck(o, &TypeIndexType)) throw std::invalid_argument("Expected instance of cpy.TypeIndex");
    }
    return reinterpret_cast<Holder<T> *>(o)->value;
}

/******************************************************************************/

inline Object as_weak_reference(WeakReference a) {
    Object o{PyObject_CallObject(type_object(WrapType), nullptr), false};
    cast_object<Wrap>(+o) = std::move(a);
    return o;
}

ArgPack positional_args(Object const &pypack);

template <>
struct Simplify<PyObject> {
    void operator()(Value &, PyObject const &, std::type_index) const;
    void *operator()(Qualifier, PyObject const &, std::type_index) const;
};

/******************************************************************************/

inline bool set_tuple_item(Object const &t, Py_ssize_t i, Object const &x) {
    if (!x) return false;
    Py_INCREF(+x);
    PyTuple_SET_ITEM(+t, i, +x);
    return true;
}

template <class V, class F>
Object map_as_tuple(V &&v, F &&f) noexcept {
    Object out = {PyTuple_New(std::size(v)), false};
    if (!out) return {};
    auto it = std::begin(v);
    for (Py_ssize_t i = 0u; i != std::size(v); ++i, ++it) {
        using T = std::conditional_t<std::is_rvalue_reference_v<V>,
            decltype(*it), decltype(std::move(*it))>;
        if (!set_tuple_item(out, i, f(static_cast<T>(*it)))) return {};
    }
    return out;
}

template <class ...Ts>
Object args_as_tuple(Ts &&...ts) {
    Object out = {PyTuple_New(sizeof...(Ts)), false};
    if (!out) return {};
    Py_ssize_t i = 0;
    auto go = [&](Object const &x) {return set_tuple_item(out, i++, x);};
    return (go(ts) && ...) ? out : Object();
}

struct NoDeleter {void operator()(void *) {}};

inline Object to_python_args(ArgPack const &s, Vector<std::shared_ptr<void>> &storage, Object const &sig={}) {
    if (sig && !PyTuple_Check(+sig))
        throw python_error(type_error("expected tuple but got %R", (+sig)->ob_type));
    std::size_t len = sig ? PyObject_Length(+sig) : 0;
    auto const n = s.size();
    Object out = {PyTuple_New(n), false};
    if (!out) return {};
    Py_ssize_t i = 0u;
    for (auto const &ref : s) {
        if (i < len) {
            PyObject *t = PyTuple_GET_ITEM(+sig, i);
        } else {
            storage.emplace_back(ref.ptr, NoDeleter());
            if (!set_tuple_item(out, i, as_weak_reference(WeakReference{ref, storage.back()}))) return {};
        }
        ++i;
    }
    return out;
}

/******************************************************************************/

template <class F>
void map_iterable(Object iterable, F &&f) {
    Object iter = {PyObject_GetIter(+iterable), false};
    if (!iter) throw python_error();
    while (true) {
        auto it = Object(PyIter_Next(+iter), false);
        if (!+it) return;
        f(std::move(it));
    }
}

/******************************************************************************/

/// RAII release of Python GIL
struct SuspendedPython final : Frame {
    PyThreadState *state;
    std::mutex mutex;

    SuspendedPython(bool no_gil) : state(no_gil ? PyEval_SaveThread() : nullptr) {
        if (Debug) std::cout << "running with nogil=" << no_gil << std::endl;
    }

    std::shared_ptr<Frame> operator()(std::shared_ptr<Frame> &&t) override {
        if (Debug) std::cout << "suspended Python " << bool(t) << std::endl;
        return std::move(t);
    }

    // lock to prevent multiple threads trying to get the thread going
    void acquire() noexcept {if (state) {mutex.lock(); PyEval_RestoreThread(state);}}

    void release() noexcept {if (state) {state = PyEval_SaveThread(); mutex.unlock();}}

    ~SuspendedPython() {if (state) PyEval_RestoreThread(state);}
};

struct PythonEntry final : Frame {
    bool no_gil;
    PythonEntry(bool no_gil) : no_gil(no_gil) {}
    std::shared_ptr<Frame> operator()(std::shared_ptr<Frame> &&) override {
        if (Debug) std::cout << "entered C++ from Python nogil=" << no_gil << std::endl;
        return std::make_shared<SuspendedPython>(no_gil);
    }
    ~PythonEntry() = default;
};

/// RAII reacquisition of Python GIL
struct ActivePython {
    SuspendedPython &lock; //< SuspendedPython object; can be nullptr

    ActivePython(SuspendedPython &u) : lock(u) {lock.acquire();}
    ~ActivePython() {lock.release();}
};

/******************************************************************************/

struct PythonFunction {
    Object function, signature;

    PythonFunction(Object f, Object s={}) : function(std::move(f)), signature(std::move(s)) {
        if (+signature == Py_None) signature = Object();
        if (!function)
            throw python_error(type_error("cannot convert null object to Function"));
        if (!PyCallable_Check(+function))
            throw python_error(type_error("expected callable type but got %R", (+function)->ob_type));
        if (+signature && !PyTuple_Check(+signature))
            throw python_error(type_error("expected tuple or None but got %R", (+signature)->ob_type));
    }

    /// Run C++ functor; logs non-ClientError and rethrows all exceptions
    Value operator()(Caller c, ArgPack args) const {
        if (Debug) std::cout << "calling python function" << std::endl;
        auto p = c.target<SuspendedPython>();
        if (!p) throw DispatchError("Python context is expired or invalid");
        ActivePython lk(*p);
        Vector<std::shared_ptr<void>> storage;
        Object o = to_python_args(std::move(args), storage, signature);
        if (!o) throw python_error();
        o = {PyObject_CallObject(+function, +o), false};
        if (!o) throw python_error();
        return Value(Reference(*(+o)));
    }
};

/******************************************************************************/

std::string_view get_type_name(std::type_index idx) noexcept;

std::string wrong_type_message(WrongType const &e);

template <class F>
PyObject *raw_object(F &&f) noexcept {
    try {
        Object o = static_cast<F &&>(f)();
        Py_XINCREF(+o);
        return +o;
    } catch (PythonError const &) {
        return nullptr;
    } catch (std::bad_alloc const &e) {
        PyErr_SetString(PyExc_MemoryError, "C++: out of memory (std::bad_alloc)");
    } catch (WrongNumber const &e) {
        unsigned int n0 = e.expected, n = e.received;
        PyErr_Format(PyExc_TypeError, "C++: wrong number of arguments (expected %u, got %u)", n0, n);
    } catch (WrongType const &e) {
        try {PyErr_SetString(PyExc_TypeError, wrong_type_message(e).c_str());}
        catch(...) {PyErr_SetString(PyExc_TypeError, e.what());}
    } catch (std::exception const &e) {
        if (!PyErr_Occurred())
            PyErr_Format(PyExc_RuntimeError, "C++: %s", e.what());
    } catch (...) {
        if (!PyErr_Occurred())
            PyErr_SetString(PyExc_RuntimeError, "C++: unknown exception");
    }
    return nullptr;
}

/******************************************************************************/

}
