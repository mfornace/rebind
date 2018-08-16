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

bool attach_type(PyTypeObject *, PyObject *, char const *) noexcept;

extern PyTypeObject FunctionType, AnyType, TypeIndexType;

struct Identity {
    template <class T>
    T const & operator()(T const &t) const {return t;}
};

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

struct NullObject {
    operator Object() const {return {};}
};

struct NoLookup {
    NoLookup() = default;

    template <class T>
    constexpr NoLookup(T const &) {}

    constexpr NullObject operator()(std::type_info const &) const {return {};}
};

struct SortedLookup {
    std::vector<std::pair<std::type_index, Object>> contents;

    Object operator()(std::type_index const &idx) const {
        auto it = std::lower_bound(contents.begin(), contents.end(), std::make_pair(idx, Object()));
        return (it != contents.end() && it->first == idx) ? it->second : Object();
    }
};

extern SortedLookup GlobalLookup;

/******************************************************************************/

template <class T>
T & cast_object(PyObject *o) {
    return reinterpret_cast<Holder<T> *>(o)->value;
}

auto & as_any(PyObject *o) {
    if (!PyObject_TypeCheck(o, &AnyType)) throw std::invalid_argument("Expected instance of cpy.Any");
    return cast_object<Any>(o);
}

auto & as_index(PyObject *o) {
    if (!PyObject_TypeCheck(o, &TypeIndexType)) throw std::invalid_argument("Expected instance of cpy.TypeIndex");
    return cast_object<std::type_index>(o);
}

auto & as_function(PyObject *o) {
    if (!PyObject_TypeCheck(o, &FunctionType)) throw std::invalid_argument("Expected instance of cpy.Function");
    return cast_object<Function>(o);
}

/******************************************************************************/

inline Object to_python(std::monostate const &, NoLookup={}) noexcept {
    return {Py_None, true};
}

inline Object to_python(bool b, NoLookup={}) noexcept {
    return {b ? Py_True : Py_False, true};
}

inline Object to_python(char const *s, NoLookup={}) noexcept {
    return {PyUnicode_FromString(s ? s : ""), false};
}

template <class T, std::enable_if_t<(std::is_integral_v<T>), int> = 0>
Object to_python(T t, NoLookup={}) noexcept {
    return {PyLong_FromLongLong(static_cast<long long>(t)), false};
}

inline Object to_python(double t, NoLookup={}) noexcept {
    return {PyFloat_FromDouble(t), false};
}

inline Object to_python(std::string const &s, NoLookup={}) noexcept {
    return {PyUnicode_FromStringAndSize(s.data(), static_cast<Py_ssize_t>(s.size())), false};
}

inline Object to_python(std::string_view const &s, NoLookup={}) noexcept {
    return {PyUnicode_FromStringAndSize(s.data(), static_cast<Py_ssize_t>(s.size())), false};
}

inline Object to_python(std::wstring const &s, NoLookup={}) noexcept {
    return {PyUnicode_FromWideChar(s.data(), static_cast<Py_ssize_t>(s.size())), false};
}

inline Object to_python(std::wstring_view const &s, NoLookup={}) noexcept {
    return {PyUnicode_FromWideChar(s.data(), static_cast<Py_ssize_t>(s.size())), false};
}

inline Object to_python(std::complex<double> const &s, NoLookup={}) noexcept {
    return {PyComplex_FromDoubles(s.real(), s.imag()), false};
}

inline Object to_python(Binary const &s, NoLookup={}) noexcept {
    return {Py_None, false};
}

inline Object to_python(Function f, NoLookup={}) noexcept {
    Object o{PyObject_CallObject((PyObject *) &FunctionType, nullptr), false};
    cast_object<Function>(+o) = std::move(f);
    return o;
}

inline Object to_python(std::type_index f, NoLookup={}) noexcept {
    Object o{PyObject_CallObject((PyObject *) &TypeIndexType, nullptr), false};
    cast_object<std::type_index>(+o) = std::move(f);
    return o;
}

template <class T, class F, std::enable_if_t<std::is_same_v<Any, T>, int> = 0>
inline Object to_python(T a, F &&lookup) noexcept {
    Object f = lookup(a.type());
    Object o{PyObject_CallObject(f ? +f : (PyObject *) &AnyType, nullptr), false};
    cast_object<Any>(+o) = std::move(a);
    return o;
}

bool put_argpack(ArgPack &pack, Object pypack);

bool get_argpack(ArgPack &pack, Object pypack);

/******************************************************************************/

template <class V, class L=NoLookup, class F=Identity>
Object to_tuple(V &&v, L &&l={}, F const &f={}) noexcept {
    Object out = {PyTuple_New(v.size()), false};
    if (!out) return {};
    for (Py_ssize_t i = 0u; i != v.size(); ++i) {
        using T = std::conditional_t<std::is_rvalue_reference_v<V>,
            decltype(v[i]), decltype(std::move(v[i]))>;
        Object item = to_python(f(static_cast<T>(v[i])), l);
        if (!item) return {};
        Py_INCREF(+item);
        if (PyTuple_SetItem(+out, i, +item)) {
            Py_DECREF(+item);
            return {};
        }
    }
    return out;
}

template <class T, class F=NoLookup>
Object to_python(Vector<T> &&v, F &&f={}) {return to_tuple(std::move(v), f);}

template <class T, class F=NoLookup>
Object to_python(Vector<T> const &v, F &&f={}) {return to_tuple(v, f);}

template <class T, class F=NoLookup, std::enable_if_t<std::is_same_v<T, Value>, int> = 0>
Object to_python(T const &s, F &&f={}) noexcept {
    return std::visit([&](auto const &x) {return to_python(x, f);}, s.var);
}

template <class T, class F=NoLookup, std::enable_if_t<std::is_same_v<T, Value>, int> = 0>
Object to_python(T &&s, F &&f={}) noexcept {
    return std::visit([&](auto &x) {return to_python(std::move(x), f);}, s.var);
}

template <class F=NoLookup>
inline Object to_python(KeyPair const &p, F &&f={}) noexcept {
    Object key = to_python(p.key, f);
    if (!key) return {};
    Object value = to_python(p.value, f);
    if (!value) return {};
    return {PyTuple_Pack(2u, +key, +value), false};
}

/******************************************************************************/

template <class F>
bool map_iterable(Object iterable, F &&f) {
    Object iter = {PyObject_GetIter(+iterable), false};
    if (!iter) return false;

    bool ok = true;
    while (ok) {
        auto it = Object(PyIter_Next(+iter), false);
        if (!+it) break;
        ok = f(std::move(it));
    }
    return ok;
}

/******************************************************************************/

struct PythonError : ClientError {
    PythonError(char const *s) : ClientError(s) {}
};

PythonError python_error() noexcept;

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

/// std::ostream synchronizer for redirection from multiple threads
struct StreamSync {
    std::ostream &stream;
    std::streambuf *original; // never changed (unless by user)
    std::mutex mutex;
    Vector<std::streambuf *> queue;
};

extern StreamSync cout_sync;
extern StreamSync cerr_sync;

/// RAII acquisition of cout or cerr
struct RedirectStream {
    StreamSync &sync;
    std::streambuf * const buf;

    RedirectStream(StreamSync &s, std::streambuf *b) : sync(s), buf(b) {
        if (!buf) return;
        std::lock_guard<std::mutex> lk(sync.mutex);
        if (sync.queue.empty()) sync.stream.rdbuf(buf); // take over the stream
        else sync.queue.push_back(buf); // or add to queue
    }

    ~RedirectStream() {
        if (!buf) return;
        std::lock_guard<std::mutex> lk(sync.mutex);
        auto it = std::find(sync.queue.begin(), sync.queue.end(), buf);
        if (it != sync.queue.end()) sync.queue.erase(it); // remove from queue
        else if (sync.queue.empty()) sync.stream.rdbuf(sync.original); // set to original
        else { // let next waiting stream take over
            sync.stream.rdbuf(sync.queue[0]);
            sync.queue.erase(sync.queue.begin());
        }
    }
};

/******************************************************************************/

bool from_python(Value &v, Object o);

/******************************************************************************/

template <class F>
PyObject * return_object(F &&f) noexcept {
    try {
        Object o = static_cast<F &&>(f)();
        Py_XINCREF(+o);
        return +o;
    } catch (PythonError const &) {
        return nullptr;
    } catch (std::bad_alloc const &e) {
        PyErr_Format(PyExc_MemoryError, "C++ out of memory with message %s", e.what());
    } catch (std::exception const &e) {
        if (!PyErr_Occurred())
            PyErr_Format(PyExc_RuntimeError, "C++ exception with message %s", e.what());
    } catch (...) {
        if (!PyErr_Occurred())
            PyErr_SetString(PyExc_RuntimeError, "Unknown C++ exception");
    }
    return nullptr;
}

/******************************************************************************/

}
