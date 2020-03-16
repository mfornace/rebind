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

/// Main wrapper type for Pointer: adds a ward object for lifetime management
struct PyPointer : Pointer {
    using Pointer::Pointer;
    Object ward = {};
};

template <>
struct Wrap<Value> : Wrap<PyValue> {};

template <>
struct Wrap<Pointer> : Wrap<PyPointer> {};

/******************************************************************************/

template <class T>
T * cast_if(PyObject *o) {
    if (!PyObject_TypeCheck(o, type_object<T>())) return nullptr;
    return std::addressof(reinterpret_cast<Wrap<T> *>(o)->value);
}

template <class T>
T & cast_object(PyObject *o) {
    if (!PyObject_TypeCheck(o, type_object<T>()))
        throw std::invalid_argument("Expected instance of rebind.Index");
    return reinterpret_cast<Wrap<T> *>(o)->value;
}

/******************************************************************************/

template <class T>
PyObject *tp_new(PyTypeObject *subtype, PyObject *, PyObject *) noexcept {
    static_assert(noexcept(T{}), "Default constructor should be noexcept");
    PyObject *o = subtype->tp_alloc(subtype, 0); // 0 unused
    if (o) new (&cast_object<T>(o)) T; // Default construct the C++ type
    return o;
}

/******************************************************************************/

template <class T>
void tp_delete(PyObject *o) noexcept {
    reinterpret_cast<Wrap<T> *>(o)->~Wrap<T>();
    Py_TYPE(o)->tp_free(o);
}

/******************************************************************************/

template <class T>
PyObject * copy_from(PyObject *self, PyObject *other) noexcept {
    return raw_object([=] {
        cast_object<T>(self) = cast_object<T>(other); // not notexcept
        return Object(Py_None, true);
    });
}

/******************************************************************************/

template <class T>
PyTypeObject type_definition(char const *name, char const *doc) {
    PyTypeObject o{PyVarObject_HEAD_INIT(NULL, 0)};
    o.tp_name = name;
    o.tp_basicsize = sizeof(Wrap<T>);
    o.tp_dealloc = tp_delete<T>;
    o.tp_new = tp_new<T>;
    o.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    o.tp_doc = doc;
    return o;
}

/******************************************************************************/

}

namespace rebind {

template <>
struct ToValue<py::Object> {
    bool operator()(Value &v, py::Object const &o) const {
        DUMP("trying to get Value from Object", v.name());
#warning "need to do this Object"
        return false;
        // if (auto p = cast_if<Pointer>(o)) {
        //     DUMP("requested qualified variable", t, p->index());
        //     v = p->request_value(t);
        //     DUMP(p->index(), t, v.index());
        // }
    }
};

}


// namespace rebind {

// template <>
// struct ToValue<py::Object, Value> {
//     void operator()(Value &v, py::Object const &o) const {
//         // Value v;
//         // DUMP("trying to get reference from unqualified Object", t);
//         // if (!o) return v;

//         // DUMP("ref1", reference_count(o));
//         // Object type = Object(reinterpret_cast<PyObject *>((+o)->ob_type), true);

//         // if (auto p = input_conversions.find(type); p != input_conversions.end()) {
//         //     Object guard(+o, false); // PyObject_CallFunctionObjArgs increments reference
//         //     o = Object::from(PyObject_CallFunctionObjArgs(+p->second, +o, nullptr));
//         //     type = Object(reinterpret_cast<PyObject *>((+o)->ob_type), true);
//         // }

//         // DUMP("ref2", reference_count(o));

//         // bool ok = object_response(v, t, std::move(o));

//         // DUMP("got response from object", ok);
//         // if (!ok) { // put diagnostic for the source type
//         //     auto o = Object::from(PyObject_Repr(+type));
//         //     DUMP("setting object error description", from_unicode(o));
//         //     v = {Type<std::string>(), from_unicode(o)};
//         // }
//         // return ok;
//     }
// };

// }