#pragma once
#include "Raw.h"
// #include "Common.h"
// #include "Variable.h"
#include <ara/Common.h>

namespace ara::py {

/******************************************************************************/

template <class T>
struct Wrap {
    PyObject_HEAD // seems to be 16 bytes for the ref count and the type object
    T value; // stack is OK because this object is only casted to anyway.
    static PyTypeObject type;
    static void initialize(Instance<PyTypeObject>) noexcept;
};

template <class T>
PyTypeObject Wrap<T>::type{PyVarObject_HEAD_INIT(NULL, 0)};

/******************************************************************************/

template <class T>
Instance<PyTypeObject> static_type(Type<T> t={}) noexcept {
    return instance(&Wrap<T>::type);
}

/******************************************************************************/

template <class T>
T* cast_if(PyObject* o) {
    if (!o || !PyObject_TypeCheck(+o, +static_type<T>())) return nullptr;
    return std::addressof(reinterpret_cast<Wrap<T> *>(+o)->value);
}

template <class T>
T& cast_object_unsafe(PyObject *o) noexcept {return reinterpret_cast<Wrap<T> *>(o)->value;}

template <class T>
T& cast_object(PyObject *o) {
    if (!PyObject_TypeCheck(o, +static_type<T>()))
        throw std::invalid_argument("Expected instance of " + std::string(typeid(T).name()));
    return cast_object_unsafe<T>(o);
}

/******************************************************************************/

// Python function object for use in C++
// It is only invokable by passing in a Caller containing a PythonFrame
// That lets the function acquire the GIL safely from a multithreaded scenario
// struct PythonFunction {
//     Object function, signature;

//     PythonFunction(Object f, Object s={}) : function(std::move(f)), signature(std::move(s)) {
//         if (+signature == Py_None) signature = Object();
//         if (!function)
//             throw python_error(type_error("cannot convert null object to Overload"));
//         if (!PyCallable_Check(+function))
//             throw python_error(type_error("expected callable type but got %R", (+function)->ob_type));
//         if (+signature && !PyTuple_Check(+signature))
//             throw python_error(type_error("expected tuple or None but got %R", (+signature)->ob_type));
//     }

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
// };

}

/******************************************************************************/