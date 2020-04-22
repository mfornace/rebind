/**
 * @brief Python-related C++ API for ara
 * @file PythonAPI.h
 */

#pragma once
#include "Raw.h"
// #include <ara-cpp/Schema.h>

namespace ara::py {

/******************************************************************************/

template <int I, int J>
void initialize_global_objects();

template <int I, int J>
void clear_global_objects();

/******************************************************************************/

struct PythonError : std::runtime_error {
    PythonError(char const *s) : std::runtime_error(s) {}
};

PythonError python_error(std::nullptr_t=nullptr) noexcept;

/******************************************************************************/

template <int I, int J>
struct Obj {

};

template <int I, int J>
struct Object {
    PyObject *ptr = nullptr;
    Object() = default;
    Object(std::nullptr_t) {}
    static Object from(PyObject *o) {return o ? Object(o, false) : throw python_error();}
    Object(PyObject *o, bool increment) : ptr(o) {if (increment) xincref(ptr);}

    Object(Object const &o) noexcept : ptr(o.ptr) {xincref(ptr);}
    Object & operator=(Object const &o) noexcept {ptr = o.ptr; xincref(ptr); return *this;}

    Object(Object &&o) noexcept : ptr(std::exchange(o.ptr, nullptr)) {}
    Object & operator=(Object &&o) noexcept {ptr = std::exchange(o.ptr, nullptr); return *this;}

    explicit operator bool() const {return ptr;}
    operator PyObject *() const {return ptr;}
    PyObject *operator+() const {return ptr;}

    bool operator<(Object const &o) const {return ptr < o.ptr;}
    bool operator>(Object const &o) const {return ptr > o.ptr;}
    bool operator==(Object const &o) const {return ptr == o.ptr;}
    bool operator!=(Object const &o) const {return ptr != o.ptr;}
    bool operator<=(Object const &o) const {return ptr <= o.ptr;}
    bool operator>=(Object const &o) const {return ptr >= o.ptr;}
    friend void swap(Object &o, Object &p) {std::swap(o.ptr, p.ptr);}

    PyTypeObject * as_type() const {
        return PyType_CheckExact(ptr) ? reinterpret_cast<PyTypeObject *>(ptr) : nullptr;
    }

    ~Object() {xdecref(ptr);}
};

}

namespace std {
    template <>
    struct hash<ara::py::Object> {
        size_t operator()(ara::py::Object const &o) const {return std::hash<PyObject *>()(o.ptr);}
    };
}
