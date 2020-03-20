#pragma once
#include "Object.h"
#include "API.h"
#include <rebind/Value.h>
#include <rebind/Conversions.h>

namespace rebind::py {

/******************************************************************************/

template <class T>
struct Wrap {
    static PyTypeObject type;
    PyObject_HEAD // 16 bytes for the ref count and the type object
    T value; // stack is OK because this object is only casted to anyway.
};

template <class T>
PyTypeObject Wrap<T>::type;

template <class T>
SubClass<PyTypeObject> type_object(Type<T> t={}) {return {&Wrap<T>::type};}

/******************************************************************************/

/// Main wrapper type for Value: adds a ward object for lifetime management
struct PyValue : Value {
    using Value::Value;
    Object ward = {};
};

/// Main wrapper type for Ref: adds a ward object for lifetime management
struct PyRef : Ref {
    using Ref::Ref;
    Object ward = {};
};

template <>
struct Wrap<Value> : Wrap<PyValue> {};

template <>
struct Wrap<Ref> : Wrap<PyRef> {};

/******************************************************************************/

template <class T>
T * cast_if(PyObject *o) {
    if (!PyObject_TypeCheck(o, type_object<T>())) return nullptr;
    return std::addressof(reinterpret_cast<Wrap<T> *>(o)->value);
}

template <class T>
T & cast_object(PyObject *o) {
    if (!PyObject_TypeCheck(o, type_object<T>()))
        throw std::invalid_argument("Expected instance of " + std::string(typeid(T).name()));
    return reinterpret_cast<Wrap<T> *>(o)->value;
}

/******************************************************************************/

template <class T>
PyObject *c_new(PyTypeObject *subtype, PyObject *, PyObject *) noexcept {
    static_assert(noexcept(T{}), "Default constructor should be noexcept");
    PyObject *o = subtype->tp_alloc(subtype, 0); // 0 unused
    if (o) new (&cast_object<T>(o)) T; // Default construct the C++ type
    return o;
}

/******************************************************************************/

template <class T>
void c_delete(PyObject *o) noexcept {
    reinterpret_cast<Wrap<T> *>(o)->~Wrap<T>();
    Py_TYPE(o)->tp_free(o);
}

/******************************************************************************/

// #pragma clang diagnostic push
// #pragma clang diagnostic ignored "-Wdeprecated-declarations"

template <class T>
PyTypeObject type_definition(char const *name, char const *doc) {
    PyTypeObject o{PyVarObject_HEAD_INIT(NULL, 0)};
    o.tp_name = name;
    o.tp_basicsize = sizeof(Wrap<T>);
    o.tp_dealloc = c_delete<T>;
    o.tp_new = c_new<T>;
    o.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    o.tp_doc = doc;
    return o;
}

// #pragma clang diagnostic pop

/******************************************************************************/

bool object_to_value(Value &v, Object o);

/******************************************************************************/

}

namespace rebind {

template <>
struct ToValue<py::Object> {
    bool operator()(Value &v, py::Object o) const {return py::object_to_value(v, std::move(o));}
};

}