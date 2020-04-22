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

/******************************************************************************************/


/******************************************************************************************/

namespace ara::py {

template <class Module>
PyObject* init_module() noexcept;

/******************************************************************************/

static constexpr int Major = PY_MAJOR_VERSION, Minor = PY_MINOR_VERSION;
using Ptr = PyObject*;

/******************************************************************************/

struct PythonError {
    PythonError(std::nullptr_t=nullptr) {}
};

struct Object {
    PyObject *base;

    Object() : base(nullptr) {}
    Object(std::nullptr_t) : base(nullptr) {}

    Object(PyObject *o, bool increment) : base(o) {if (increment) Py_XINCREF(base);}

    Object(Object const &o) noexcept : base(o.base) {Py_XINCREF(base);}
    Object & operator=(Object const &o) noexcept {base = o.base; Py_XINCREF(base); return *this;}

    Object(Object &&o) noexcept : base(std::exchange(o.base, nullptr)) {}
    Object & operator=(Object &&o) noexcept {base = std::exchange(o.base, nullptr); return *this;}

    explicit operator bool() const {return base;}
    PyObject *operator+() const {return base;}

    bool operator<(Object const &o) const {return base < o.base;}
    bool operator>(Object const &o) const {return base > o.base;}
    bool operator==(Object const &o) const {return base == o.base;}
    bool operator!=(Object const &o) const {return base != o.base;}
    bool operator<=(Object const &o) const {return base <= o.base;}
    bool operator>=(Object const &o) const {return base >= o.base;}
    friend void swap(Object &o, Object &p) noexcept {std::swap(o.base, p.base);}

    static Object from(PyObject *o) {return o ? Object(o, false) : throw PythonError();}

    ~Object() {Py_XDECREF(base);}
};

/******************************************************************************/

template <class T>
struct SubClass {
    T *ptr;
    SubClass(T* p=nullptr) noexcept : ptr(p) {}

    operator PyObject *() const {return reinterpret_cast<PyObject *>(ptr);}
    operator T *() const {return ptr;}
    explicit operator bool() const {return ptr;}

    bool operator==(SubClass const &other) {return ptr == other.ptr;}
};

struct TypePtr : SubClass<PyTypeObject> {
    using SubClass<PyTypeObject>::SubClass;

    static TypePtr from(Ptr p) noexcept {
        return TypePtr(PyType_CheckExact(p) ? reinterpret_cast<PyTypeObject *>(p) : nullptr);
    }

    template <class T>
    static TypePtr from(Type<T> t={}) noexcept;
};

/******************************************************************************/

template <class ...Ts>
std::nullptr_t type_error(char const *s, Ts ...ts) {
    PyErr_Format(PyExc_TypeError, s, ts...);
    return nullptr;
}

/******************************************************************************/

// Dump
//  {
//     PyErr_Format(PyExc_ImportError, "Python version %d.%d was not compiled by the ara library", Major, Minor);
//     return nullptr;
// }

// template <>
// PyObject* init_module<PY_MAJOR_VERSION, PY_MINOR_VERSION>(Object<PY_MAJOR_VERSION, PY_MINOR_VERSION> const &);

}