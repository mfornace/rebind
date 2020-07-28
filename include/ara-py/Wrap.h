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
    // static bool matches(Always<PyTypeObject> p) {return +p == Py_None->ob_type;}



    // static Value<> load(Ignore, Ignore, Ignore) {return {Py_None, true};}
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

template <class T> template <class ...Args>
Value<T> Value<T>::new_from(Args &&...args) {
    DUMP("allocating new object", type_name<T>());
    auto out = Value<T>::take(T::def()->tp_alloc(+T::def(), 0)); // 0 unused
    T::placement_new(*out, std::forward<Args>(args)...);
    DUMP(bool(out), out, reference_count(out));
    return out;
}

template <class T>
PyObject* default_construct(PyTypeObject* subtype, PyObject*, PyObject*) noexcept {
    DUMP("allocating new object");
    PyObject *o = subtype->tp_alloc(subtype, 0); // 0 unused
    DUMP("initializing object");
    if (o) T::placement_new(*reinterpret_cast<typename T::type*>(o)); // Default construct the C++ type
    DUMP("returning object", reference_count(Ptr<>{o}))
    return o;
}

/******************************************************************************/

template <class T>
void call_destructor(PyObject *o) noexcept {
    DUMP("destroying", type_name<T>(), reference_count(Ptr<>{o}));
    reinterpret_cast<T *>(o)->~T();
    Py_TYPE(o)->tp_free(o);
}

/******************************************************************************/

template <class T>
void define_type(Always<pyType> o, char const *name, char const *doc) noexcept {
    DUMP("define_type", name, type_name<T>());
    o->tp_name = name;
    o->tp_basicsize = sizeof(typename T::type);
    o->tp_itemsize = 0;
    o->tp_dealloc = call_destructor<typename T::type>;
    o->tp_new = default_construct<T>;
    o->tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    o->tp_doc = doc;
}

/******************************************************************************/

// template <class T>
// Always<Type> static_type(ara::Type<T> = {}) noexcept {return T::type();}

/******************************************************************************/

// template <class T>
// T* cast_if(PyObject* o) {
//     if (!o || !PyObject_TypeCheck(+o, +static_type<T>())) return nullptr;
//     return std::addressof(reinterpret_cast<Wrap<T> *>(+o)->value);
// }

// template <class T>
// T& cast_object_unsafe(PyObject *o) noexcept {return reinterpret_cast<Wrap<T> *>(o)->value;}

// template <class T>
// T& cast_object(PyObject *o) {
//     if (!PyObject_TypeCheck(o, +static_type<T>()))
//         throw std::invalid_argument("Expected instance of " + std::string(typeid(T).name()));
//     return cast_object_unsafe<T>(o);
// }

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