#pragma once
#include "Object.h"
#include "Common.h"
// #include "Variable.h"
// #include <rebind/Convert.h>

namespace rebind::py {

/******************************************************************************/

template <class T>
struct Wrap {
    static storage_like<PyTypeObject> type;
    PyObject_HEAD // 16 bytes for the ref count and the type object
    T value; // stack is OK because this object is only casted to anyway.
};

template <class T>
storage_like<PyTypeObject> Wrap<T>::type;

template <class T>
SubClass<PyTypeObject> type_object(Type<T> t={}) {return {&storage_cast<PyTypeObject>(Wrap<T>::type)};}

/******************************************************************************/

/// Main wrapper type for Value: adds a ward object for lifetime management
// struct PyValue : Value {
//     using Value::Value;
//     Object ward = {};
// };

// /// Main wrapper type for Ref: adds a ward object for lifetime management
// struct PyRef : Ref {
//     using Ref::Ref;
//     Object ward = {};
// };

// template <>
// struct Wrap<Value> : Wrap<PyValue> {};

// template <>
// struct Wrap<Ref> : Wrap<PyRef> {};

/******************************************************************************/

template <class T>
T * cast_if(PyObject *o) {
    if (!PyObject_TypeCheck(o, type_object<T>())) return nullptr;
    return std::addressof(reinterpret_cast<Wrap<T> *>(o)->value);
}

template <class T>
T & cast_object_unsafe(PyObject *o) {return reinterpret_cast<Wrap<T> *>(o)->value;}

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

template <class T>
storage_like<PyTypeObject> type_definition(char const *name, char const *doc) {
    static_assert(std::is_trivial_v<PyTypeObject>);
    DUMP("define type ", name);
    PyTypeObject out{PyVarObject_HEAD_INIT(NULL, 0)};
    out.tp_name = name;
    out.tp_basicsize = sizeof(Wrap<T>);
    out.tp_dealloc = c_delete<T>;
    out.tp_new = c_new<T>;
    out.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    out.tp_doc = doc;
    return *std::launder(reinterpret_cast<storage_like<PyTypeObject> *>(&out));
}

/******************************************************************************/

bool dump_object(Target &v, Object o);

// bool object_to_ref(Ref &v, Object o);

/******************************************************************************/

// Python function object for use in C++
// It is only invokable by passing in a Caller containing a PythonFrame
// That lets the function acquire the GIL safely from a multithreaded scenario
struct PythonFunction {
    Object function, signature;

    PythonFunction(Object f, Object s={}) : function(std::move(f)), signature(std::move(s)) {
        if (+signature == Py_None) signature = Object();
        if (!function)
            throw python_error(type_error("cannot convert null object to Overload"));
        if (!PyCallable_Check(+function))
            throw python_error(type_error("expected callable type but got %R", (+function)->ob_type));
        if (+signature && !PyTuple_Check(+signature))
            throw python_error(type_error("expected tuple or None but got %R", (+signature)->ob_type));
    }

    /// Run C++ functor; logs non-ClientError and rethrows all exceptions
//     bool operator()(Caller *c, Value *v, Ref *r, ArgView &args) const {
//         DUMP("calling python function");
//         auto p = c->target<PythonFrame>();
//         if (!p) throw DispatchError("Python context is expired or invalid");
//         PythonGuard lk(*p);
//         Object o = args_to_python(args, signature);
//         if (!o) throw python_error();
//         auto output = Object::from(PyObject_CallObject(function, o));
//         return false;
// #warning "cehck here"
//         // if (v) return object_to_value(*v, std::move(output));
//         // else return object_to_ref(*r, std::move(output));
//     }
};

template <int, int>
struct Obj {
    Obj() = delete;
    ~Obj() = delete;
};

using VersionedObject = Obj<PY_MAJOR_VERSION, PY_MINOR_VERSION>;

}

/******************************************************************************/

namespace rebind {

template <>
struct Dumpable< py::VersionedObject > {
    bool operator()(Target &v, py::VersionedObject &o) const {
        DUMP("hmm");
        return py::dump_object(v, py::Object(reinterpret_cast<PyObject *>(&o), true));
    }
    bool operator()(Target &v, py::VersionedObject const &o) const {DUMP("hmm"); return false;}
    bool operator()(Target &v, py::VersionedObject &&o) const {DUMP("hmm"); return false;}
};

// template <>
// struct Loadable<py::Object> {
//     bool operator()(Ref &v, py::Object o) const {return py::object_to_ref(v, std::move(o));}
// };

}

/******************************************************************************/