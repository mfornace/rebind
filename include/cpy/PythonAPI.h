/**
 * @brief Python-related C++ API for cpy
 * @file PythonAPI.h
 */

#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wregister"
#include <Python.h>
#pragma GCC diagnostic pop

#include "Value.h"
#include <mutex>
#include <iostream>
#include <typeindex>

namespace cpy {

template <class T>
struct Holder {
    PyObject_HEAD // 16 bytes for the ref count and the type object
    T value; // I think stack is OK because this object is only casted to anyway.
};

extern PyTypeObject FunctionType, AnyType, TypeIndexType;

struct Object {
    PyObject *ptr = nullptr;
    Object() = default;
    Object(PyObject *o, bool incref) : ptr(o) {if (incref) Py_INCREF(ptr);}
    Object(Object const &o) : ptr(o.ptr) {Py_XINCREF(ptr);}
    Object(Object &&o) noexcept : ptr(std::exchange(o.ptr, nullptr)) {}
    Object & operator=(Object o) noexcept {ptr = std::exchange(o.ptr, nullptr); return *this;}

    explicit operator bool() const {return ptr;}
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

struct Buffer {
    Py_buffer view;
    bool ok;

    Buffer(PyObject *o, int flags) {ok = PyObject_GetBuffer(o, &view, flags) == 0;}

    ~Buffer() {PyBuffer_Release(&view);}
};

/******************************************************************************/

inline PyObject * type_object(PyTypeObject &o) noexcept {return reinterpret_cast<PyObject *>(&o);}

/******************************************************************************/

template <class T>
T & cast_object(PyObject *o) {
    if constexpr(std::is_same_v<T, Any>) {
        if (!PyObject_TypeCheck(o, &AnyType)) throw std::invalid_argument("Expected instance of cpy.Any");
    }
    if constexpr(std::is_same_v<T, Function>) {
        if (!PyObject_TypeCheck(o, &FunctionType)) throw std::invalid_argument("Expected instance of cpy.Function");
    }
    if constexpr(std::is_same_v<T, std::type_index>) {
        if (!PyObject_TypeCheck(o, &TypeIndexType)) throw std::invalid_argument("Expected instance of cpy.TypeIndex");
    }
    return reinterpret_cast<Holder<T> *>(o)->value;
}

/******************************************************************************/

inline std::type_index type_index_of(Value const &v) {
    return std::visit([](auto const &x) -> std::type_index {
        return typeid(std::decay_t<decltype(x)>);
    }, v.var);
}

inline Object to_python(Object t) noexcept {
    return t;
}

inline Object to_python(std::monostate const &) noexcept {
    return {Py_None, true};
}

inline Object to_python(bool b) noexcept {
    return {b ? Py_True : Py_False, true};
}

inline Object to_python(char const *s) noexcept {
    return {PyUnicode_FromString(s ? s : ""), false};
}

template <class T, std::enable_if_t<(std::is_integral_v<T>), int> = 0>
Object to_python(T t) noexcept {
    return {PyLong_FromLongLong(static_cast<long long>(t)), false};
}

inline Object to_python(double t) noexcept {
    return {PyFloat_FromDouble(t), false};
}

inline Object to_python(std::string const &s) noexcept {
    return {PyUnicode_FromStringAndSize(s.data(), static_cast<Py_ssize_t>(s.size())), false};
}

inline Object to_python(std::string_view const &s) noexcept {
    return {PyUnicode_FromStringAndSize(s.data(), static_cast<Py_ssize_t>(s.size())), false};
}

inline Object to_python(std::wstring const &s) noexcept {
    return {PyUnicode_FromWideChar(s.data(), static_cast<Py_ssize_t>(s.size())), false};
}

inline Object to_python(std::wstring_view const &s) noexcept {
    return {PyUnicode_FromWideChar(s.data(), static_cast<Py_ssize_t>(s.size())), false};
}

inline Object to_python(std::complex<double> const &s) noexcept {
    return {PyComplex_FromDoubles(s.real(), s.imag()), false};
}

inline Object to_python(Binary const &s) noexcept {
    return {PyByteArray_FromStringAndSize(s.data(), s.size()), false};
}

inline Object to_python(Function f) noexcept {
    Object o{PyObject_CallObject(type_object(FunctionType), nullptr), false};
    cast_object<Function>(+o) = std::move(f);
    return o;
}

inline Object to_python(std::type_index f) noexcept {
    Object o{PyObject_CallObject(type_object(TypeIndexType), nullptr), false};
    cast_object<std::type_index>(+o) = std::move(f);
    return o;
}

template <class T, std::enable_if_t<std::is_same_v<Any, T>, int> = 0>
inline Object to_python(T a) noexcept {
    if (a.type() == typeid(Object)) return std::move(a).template cast<Object>();
    Object o{PyObject_CallObject(type_object(AnyType), nullptr), false};
    cast_object<Any>(+o) = std::move(a);
    return o;
}

ArgPack to_argpack(Object pypack);

/******************************************************************************/

template <class V, class F=Identity>
Object to_tuple(V &&v, F const &f={}) noexcept {
    Object out = {PyTuple_New(v.size()), false};
    if (!out) return {};
    for (Py_ssize_t i = 0u; i != v.size(); ++i) {
        using T = std::conditional_t<std::is_rvalue_reference_v<V>,
            decltype(v[i]), decltype(std::move(v[i]))>;
        Object item = to_python(f(static_cast<T>(v[i])));
        if (!item) return {};
        Py_INCREF(+item);
        if (PyTuple_SetItem(+out, i, +item)) {
            Py_DECREF(+item);
            return {};
        }
    }
    return out;
}

template <class T, std::enable_if_t<std::is_same_v<T, Value> || std::is_same_v<T, Value>, int> = 0>
Object to_python(T const &s) noexcept;

Object to_python(Sequence const &s) {
    auto const n = s.size();
    Object out = {PyTuple_New(n), false};
    if (!out) return {};
    Py_ssize_t i = 0u;
    bool ok = true;
    s.scan([&](Value o) {
        if (!ok) return;
        Object item = to_python(std::move(o));
        if (!item) {ok = false; return;}
        Py_INCREF(+item);
        if (PyTuple_SetItem(+out, i, +item)) {
            Py_DECREF(+item);
            ok = false;
        }
        ++i;
    });
    return out;
}
template <class T>
Object to_python(Vector<T> &&v) {return to_tuple(std::move(v));}

template <class T>
Object to_python(Vector<T> const &v) {return to_tuple(v);}

template <class T, std::enable_if_t<std::is_same_v<T, Value> || std::is_same_v<T, Value>, int> >
Object to_python(T const &s) noexcept {
    return std::visit([](auto const &x) {return to_python(x);}, s.var);
}

template <class T, std::enable_if_t<std::is_same_v<T, Value> || std::is_same_v<T, Value>, int> = 0>
Object to_python(T &&s) noexcept {
    return std::visit([](auto &x) {return to_python(std::move(x));}, s.var);
}

inline Object to_python(KeyPair const &p) noexcept {
    Object key = to_python(p.key);
    if (!key) return {};
    Object value = to_python(p.value);
    if (!value) return {};
    return {PyTuple_Pack(2u, +key, +value), false};
}

/******************************************************************************/

struct PythonError : ClientError {
    PythonError(char const *s) : ClientError(s) {}
};

PythonError python_error() noexcept;

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
struct ReleaseGIL {
    PyThreadState * state;
    std::mutex mutex;
    ReleaseGIL(bool no_gil) : state(no_gil ? PyEval_SaveThread() : nullptr) {}
    void acquire() noexcept {if (state) {mutex.lock(); PyEval_RestoreThread(state);}}
    void release() noexcept {if (state) {state = PyEval_SaveThread(); mutex.unlock();}}
    ~ReleaseGIL() {if (state) PyEval_RestoreThread(state);}
};

/// RAII reacquisition of Python GIL
struct AcquireGIL {
    ReleaseGIL * const lock; //< ReleaseGIL object; can be nullptr
    AcquireGIL(ReleaseGIL *u) : lock(u) {if (lock) lock->acquire();}
    ~AcquireGIL() {if (lock) lock->release();}
};

/******************************************************************************/

Value from_python(Object o);

struct PythonFunction {
    Object function;

    /// Run C++ functor; logs non-ClientError and rethrows all exceptions
    Value operator()(CallingContext &ct, ArgPack &args) const {
        AcquireGIL lk(&ct.get<ReleaseGIL>());
        Object o = to_python(args);
        if (!o) throw python_error();
        o = {PyObject_CallObject(+function, +o), false};
        if (!o) throw python_error();
        return from_python(std::move(o));
    }
};

/******************************************************************************/

std::string_view names[] = {
    "None",
    "bool",
    "int",
    "float",
    "str",
    "str",
    "TypeIndex",
    "Binary",
    "Function",
    "class",
    "tuple",
};

template <class F>
PyObject *raw_object(F &&f) noexcept {
    try {
        Object o = static_cast<F &&>(f)();
        Py_XINCREF(+o);
        return +o;
    } catch (PythonError const &) {
        return nullptr;
    } catch (std::bad_alloc const &e) {
        PyErr_Format(PyExc_MemoryError, "C++: out of memory: %s", e.what());
    } catch (WrongNumber const &e) {
        unsigned int n0 = e.expected, n = e.received;
        PyErr_Format(PyExc_TypeError, "C++: wrong number of arguments (expected %u, got %u)", n0, n);
    } catch (WrongTypes const &e) {
        std::ostringstream os;
        os << "C++: wrong argument types (";
        for (auto i : e.indices) os << names[i] << ", ";
        auto s = std::move(os).str();
        s.back() = ')';
        PyErr_SetString(PyExc_TypeError, s.c_str());
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
