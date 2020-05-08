#pragma once
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wregister"
#ifndef PY_SSIZE_T_CLEAN
#   define PY_SSIZE_T_CLEAN
#endif
#include <Python.h>
#pragma GCC diagnostic pop

#include <ara/API.h>
#include <ara/Type.h>
#include <utility>

// #define ARA_PY_BEGIN ara::py { inline namespace v37
// #define ARA_PY_END }

/******************************************************************************/

namespace ara::py {


static constexpr auto Version = std::make_tuple(PY_MAJOR_VERSION, PY_MINOR_VERSION, PY_MICRO_VERSION);

/******************************************************************************/

struct PythonError {
    PythonError(std::nullptr_t=nullptr) {}
};

/******************************************************************************/

enum class LockType {Read, Write};

/******************************************************************************/

template <class Module>
PyObject* init_module() noexcept;

/******************************************************************************/

// Non null wrapper for object pointer
template <class T=PyObject>
struct Instance {
    T *ptr;
    explicit constexpr Instance(T *b) noexcept __attribute__((nonnull (2))) : ptr(b) {}

    constexpr T* operator+() const noexcept __attribute__((returns_nonnull)) {return ptr;}
    PyObject* object() const noexcept __attribute__((returns_nonnull)) {return reinterpret_cast<PyObject*>(ptr);}

    template <class To>
    Instance<To> as() const {return Instance<To>(reinterpret_cast<To *>(ptr));}

    constexpr bool operator==(Instance const &other) noexcept {return ptr == other.ptr;}
};

// inline constexpr Instance<> instance(PyObject* t) {return Instance<>(t);}

template <class T>
constexpr Instance<T> instance(T* t) noexcept {
    // if (!t) throw std::runtime_error("bad!");
    return Instance<T>(t);
}

// template <class T>
// Instance<T> instance(PyObject* t) {return Instance<T>(reinterpret_cast<T*>(t));}

/******************************************************************************/

// RAII shared pointer interface to a possibly null object
struct Shared {
    PyObject* base;

    Shared() : base(nullptr) {}
    Shared(std::nullptr_t) : base(nullptr) {}

    Shared(PyObject* o, bool increment) : base(o) {if (increment) Py_XINCREF(base);}
    Shared(Instance<> o, bool increment) : base(+o) {if (increment) Py_INCREF(base);}

    Shared(Shared const &o) noexcept : base(o.base) {Py_XINCREF(base);}
    Shared & operator=(Shared const &o) noexcept {base = o.base; Py_XINCREF(base); return *this;}

    Shared(Shared &&o) noexcept : base(std::exchange(o.base, nullptr)) {}
    Shared & operator=(Shared &&o) noexcept {base = std::exchange(o.base, nullptr); return *this;}

    static Shared from(PyObject* o) {return o ? Shared(o, false) : throw PythonError();}

    explicit operator bool() const {return base;}
    PyObject* operator+() const {return base;}

    bool operator<(Shared const &o) const {return base < o.base;}
    bool operator>(Shared const &o) const {return base > o.base;}
    bool operator==(Shared const &o) const {return base == o.base;}
    bool operator!=(Shared const &o) const {return base != o.base;}
    bool operator<=(Shared const &o) const {return base <= o.base;}
    bool operator>=(Shared const &o) const {return base >= o.base;}
    friend void swap(Shared &o, Shared &p) noexcept {std::swap(o.base, p.base);}

    ~Shared() {Py_XDECREF(base);}
};

/******************************************************************************/

template <class ...Ts>
std::nullptr_t type_error(char const *s, Ts ...ts) {
    PyErr_Format(PyExc_TypeError, s, ts...);
    return nullptr;
}

/******************************************************************************/

template <int M, int N>
union Object {PyObject* base;};

using Export = Object<PY_MAJOR_VERSION, PY_MINOR_VERSION>;

// Dump
//  {
//     PyErr_Format(PyExc_ImportError, "Python version %d.%d was not compiled by the ara library", Major, Minor);
//     return nullptr;
// }

// template <>
// PyObject* init_module<PY_MAJOR_VERSION, PY_MINOR_VERSION>(Shared<PY_MAJOR_VERSION, PY_MINOR_VERSION> const &);

}


namespace std {
    template <>
    struct hash<ara::py::Shared> {
        size_t operator()(ara::py::Shared const &o) const {return std::hash<PyObject*>()(o.base);}
    };
}