/**
 * @brief Python-related C++ API for cpy
 * @file PythonAPI.h
 */

#pragma once
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wregister"
#include <Python.h>
#pragma GCC diagnostic pop

#include <functional>

namespace cpy {

using Version = std::tuple<unsigned, unsigned, unsigned>;
static constexpr Version PythonVersion{PY_MAJOR_VERSION, PY_MINOR_VERSION, PY_MICRO_VERSION};

void initialize_global_objects();
void clear_global_objects();

enum class Scalar {Bool, Char, SignedChar, UnsignedChar, Unsigned, Signed, Float, Pointer};
extern Zip<Scalar, TypeIndex, unsigned> scalars;

inline std::size_t reference_count(PyObject *o) {return o ? Py_REFCNT(o) : 0u;}

inline void incref(PyObject *o) noexcept {Py_INCREF(o);}
inline void decref(PyObject *o) noexcept {Py_DECREF(o);}
inline void xincref(PyObject *o) noexcept {Py_XINCREF(o);}
inline void xdecref(PyObject *o) noexcept {Py_XDECREF(o);}

void print(PyObject *o);

/******************************************************************************/

struct TypeObject {
    PyTypeObject *ptr;
    operator PyObject *() const {return reinterpret_cast<PyObject *>(ptr);}
    operator PyTypeObject *() const {return ptr;}
};

template <class T>
struct Holder {
    static PyTypeObject type;
    PyObject_HEAD // 16 bytes for the ref count and the type object
    T value; // I think stack is OK because this object is only casted to anyway.
};

template <class T>
PyTypeObject Holder<T>::type;

template <class T>
TypeObject type_object(Type<T> t={}) {return {&Holder<T>::type};}

/******************************************************************************/

struct PythonError : ClientError {
    PythonError(char const *s) : ClientError(s) {}
};

PythonError python_error(std::nullptr_t=nullptr) noexcept;

/******************************************************************************/

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

    ~Object() {xdecref(ptr);}
};

}

namespace std {
    template <>
    struct hash<cpy::Object> {
        size_t operator()(cpy::Object const &o) const {return std::hash<PyObject *>()(o.ptr);}
    };
}
