#pragma once
#include "Raw.h"
// #include "Common.h"
#include <ara/Common.h>

namespace ara::py {

/******************************************************************************/

struct pyType : Wrap<pyType> {
    using type = PyTypeObject;
    static Always<pyType> def() {return PyType_Type;}

    static bool check(Always<> o) {return PyType_Check(+o);}
};

bool is_subclass(Always<pyType> s, Always<pyType> t) {
    if (s == t) return true;
    int x = PyObject_IsSubclass(~s, ~t);
    return (x < 0) ? throw PythonError() : x;
}

/******************************************************************************/

struct ObjectBase {
    PyObject_HEAD
};

template <class T>
struct Subtype : ObjectBase, T {};

template <class T>
struct StaticType : Wrap<T> {    
    static PyTypeObject definition;

    static Always<pyType> def() {return definition;}
    static bool check(Always<> o) {return PyObject_TypeCheck(+o, &definition);}

    static bool matches(Always<pyType> o) {return is_subclass(o, def());}
};

template <class T>
PyTypeObject StaticType<T>::definition{PyVarObject_HEAD_INIT(NULL, 0)};

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