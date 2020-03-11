#pragma once
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wregister"
#include <Python.h>
#pragma GCC diagnostic pop

#include <functional>
#include <rebind/Type.h>
#include <rebind/Common.h>
#include <rebind/Error.h>

namespace rebind::py {

/******************************************************************************/

inline std::size_t reference_count(PyObject *o) {return o ? Py_REFCNT(o) : 0u;}
inline void incref(PyObject *o) noexcept {Py_INCREF(o);}
inline void decref(PyObject *o) noexcept {Py_DECREF(o);}
inline void xincref(PyObject *o) noexcept {Py_XINCREF(o);}
inline void xdecref(PyObject *o) noexcept {Py_XDECREF(o);}
inline PyObject * not_none(PyObject *o) {return o == Py_None ? nullptr : o;}

/******************************************************************************/

void print(PyObject *o);

using Version = std::tuple<unsigned, unsigned, unsigned>;
static constexpr Version PythonVersion{PY_MAJOR_VERSION, PY_MINOR_VERSION, PY_MICRO_VERSION};

/******************************************************************************/

template <class T>
struct SubClass {
    T *ptr;
    operator PyObject *() const {return reinterpret_cast<PyObject *>(ptr);}
    operator T *() const {return ptr;}
};

/******************************************************************************/

template <class T, class U>
bool compare(decltype(Py_EQ) op, T const &t, U const &u) noexcept {
    switch(op) {
        case(Py_EQ): return t == u;
        case(Py_NE): return t != u;
        case(Py_LT): return t <  u;
        case(Py_GT): return t >  u;
        case(Py_LE): return t <= u;
        case(Py_GE): return t >= u;
    }
    return false;
}

/******************************************************************************/

inline bool set_tuple_item(PyObject *t, Py_ssize_t i, PyObject *x) {
    if (!x) return false;
    incref(x);
    PyTuple_SET_ITEM(t, i, x);
    return true;
}

/******************************************************************************/

/// Helper class for dealing with memoryview, Py_buffer
class Buffer {
    static Vector<std::pair<std::string_view, std::type_info const *>> formats;
    bool valid;

public:
    Py_buffer view;

    Buffer(Buffer const &) = delete;
    Buffer(Buffer &&b) noexcept : view(b.view), valid(std::exchange(b.valid, false)) {}

    Buffer & operator=(Buffer const &) = delete;
    Buffer & operator=(Buffer &&b) noexcept {view = b.view; valid = std::exchange(b.valid, false); return *this;}

    explicit operator bool() const {return valid;}

    Buffer(PyObject *o, int flags) {
        DUMP("before buffer", reference_count(o));
        valid = PyObject_GetBuffer(o, &view, flags) == 0;
        if (valid) DUMP("after buffer", reference_count(o), view.obj == o);
    }

    static std::type_info const & format(std::string_view s);
    static std::string_view format(std::type_info const &t);
    static std::size_t itemsize(std::type_info const &t);

    // static Binary binary(Py_buffer *view, std::size_t len);
    // static Variable binary_view(Py_buffer *view, std::size_t len);

    ~Buffer() {
        if (valid) {
            PyObject *o = view.obj;
            DUMP("before release", reference_count(view.obj));
            PyBuffer_Release(&view);
            DUMP("after release", reference_count(o));
        }
    }
};

/******************************************************************************/

}
