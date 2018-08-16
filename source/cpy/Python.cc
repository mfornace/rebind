/**
 * @brief Python-related C++ source code for cpy
 * @file Python.cc
 */
#include <cpy/PythonAPI.h>
#include <cpy/Function.h>
#include <any>
#include <iostream>
#include <map>

#ifndef CPY_MODULE
#   define CPY_MODULE libcpy
#endif


namespace cpy {

/******************************************************************************/

StreamSync cout_sync{std::cout, std::cout.rdbuf()};
StreamSync cerr_sync{std::cerr, std::cerr.rdbuf()};

/******************************************************************************/

PythonError python_error() noexcept {
    PyObject *type, *value, *traceback;
    PyErr_Fetch(&type, &value, &traceback);
    PyObject *str = PyObject_Str(value);
    char const *c = nullptr;
    if (str) {
#       if PY_MAJOR_VERSION > 2
            c = PyUnicode_AsUTF8(str); // PyErr_Clear
#       else
            c = PyString_AsString(str);
#       endif
        Py_DECREF(str);
    }
    PyErr_Restore(type, value, traceback);
    return PythonError(c ? c : "Python error with failed str()");
}

/******************************************************************************/

// None
// bool
// long
// float
// bytes
// unicode
// buffer
// memory view
// Any
// Function
bool from_python(Value &v, Object o) {
    if (+o == Py_None) {
        v = std::monostate();
    } else if (PyBool_Check(+o)) {
        v = (+o == Py_True) ? true : false;
    } else if (PyLong_Check(+o)) {
        v = static_cast<Integer>(PyLong_AsLongLong(+o));
    } else if (PyFloat_Check(+o)) {
        v = static_cast<Real>(PyFloat_AsDouble(+o));
    // } else if (PyComplex_Check(+o)) {
        // v = std::complex<double>{PyComplex_RealAsDouble(+o), PyComplex_ImagAsDouble(+o)};
    } else if (PyTuple_Check(+o) || PyList_Check(+o)) {
        Vector<Value> vals;
        vals.reserve(PyObject_Length(+o));
        map_iterable(o, [&](Object o) {
            vals.emplace_back();
            return from_python(vals.back(), std::move(o));
        });
        std::cout << vals.size() << " " << vals[0].var.index() << std::endl;
        v = std::move(vals);
    } else if (PyBytes_Check(+o)) {
        char *c;
        Py_ssize_t size;
        PyBytes_AsStringAndSize(+o, &c, &size);
        v = std::string(c, size);
    } else if (PyUnicode_Check(+o)) { // no use of wstring for now.
        Py_ssize_t size;
#if PY_MAJOR_VERSION > 2
        char const *c = PyUnicode_AsUTF8AndSize(+o, &size);
#else
        char *c;
        if (PyString_AsStringAndSize(+o, &c, &size)) return false;
#endif
        if (c) v = std::string(static_cast<char const *>(c), size);
        else return false;
    } else if (PyObject_TypeCheck(+o, &AnyType)) {
        v = Value(std::in_place_t(), std::move(as_any(+o)));
    } else if (PyObject_TypeCheck(+o, &TypeIndexType)) {
        v = Value(std::in_place_t(), as_index(+o));
    } else if (PyObject_TypeCheck(+o, &FunctionType)) {
        v = Value(std::move(as_function(+o)));
    // } else if (PyObject_CheckBuffer(+o)) {
// hmm
    // } else if (PyMemoryView_Check(+o)) {
// hmm
    } else {
        v = Value(std::in_place_t(), std::move(o));
        // PyErr_SetString(PyExc_TypeError, "Invalid type for conversion to C++");
    }
    return !PyErr_Occurred();
};

/******************************************************************************/

// Store the objects in pypack in pack
bool put_argpack(ArgPack &pack, Object pypack) {
    auto n = PyObject_Length(+pypack);
    if (n < 0) return false;
    pack.reserve(pack.size() + n);
    auto out = map_iterable(std::move(pypack), [&pack](Object o) {
        pack.emplace_back();
        return from_python(pack.back(), std::move(o));
    });
    return out;
}

// If necessary, restore the objects in pack into pypack
bool get_argpack(ArgPack &pack, Object pypack) {
    auto it = pack.begin();
    return map_iterable(pypack, [&it](Object o) {
        if (std::holds_alternative<Any>(it->var)) {
            if (!PyObject_TypeCheck(+o, &AnyType)) return false;
            as_any(+o) = std::move(std::get<Any>(it->var));
        }
        if (std::holds_alternative<Function>(it->var)) {
            if (!PyObject_TypeCheck(+o, &FunctionType)) return false;
            as_function(+o) = std::move(std::get<Function>(it->var));
        }
        ++it;
        return true;
    });
}

/******************************************************************************/

PyObject *one_argument(PyObject *args, PyTypeObject *type=nullptr) {
    if (PyTuple_Size(args) != 1) {
        PyErr_SetString(PyExc_TypeError, "Expected single argument");
        return nullptr;
    }
    PyObject *value = PyTuple_GET_ITEM(args, 0);
    if (type && !PyObject_TypeCheck(value, type)) {
        PyErr_SetString(PyExc_TypeError, "Invalid argument type");
        return nullptr;
    }
    return value;
}

Object raised(PyObject *exc, char const *message) {
    PyErr_SetString(exc, message);
    return {};
}

template <class T>
PyObject *tp_new(PyTypeObject *subtype, PyObject *, PyObject *) {
    PyObject* o = subtype->tp_alloc(subtype, 0); // 0 unused
    new (&cast_object<T>(o)) T; // noexcept
    return o;
}

template <class T>
void tp_delete(PyObject *o) {
    reinterpret_cast<Holder<T> *>(o)->~Holder<T>();
    Py_TYPE(o)->tp_free(o);
}

PyObject * any_move_from(PyObject *self, PyObject *args) {
    PyObject *value = one_argument(args, self->ob_type);
    if (!value) return nullptr;
    as_any(self) = std::move(as_any(value)); // noexcept
    Py_RETURN_NONE;
}

/******************************************************************************/

PyObject *type_index_new(PyTypeObject *subtype, PyObject *, PyObject *) {
    PyObject* o = subtype->tp_alloc(subtype, 0); // 0 unused
    new (&cast_object<std::type_index>(o)) std::type_index(typeid(void)); // noexcept
    return o;
}

long type_index_hash(PyObject *o) {
    return static_cast<long>(as_index(o).hash_code());
}

PyObject *type_index_name(PyObject *o) noexcept {
    return return_object([=] {return (to_python(as_index(o).name()));});
}

PyObject *type_index_compare(PyObject *self, PyObject *other, int op) {
    return return_object([=] {
        auto const &s = as_index(self);
        auto const &o = as_index(other);
        bool out;
        switch(op) {
            case(Py_LT): out = s < o;
            case(Py_GT): out = s > o;
            case(Py_LE): out = s <= o;
            case(Py_EQ): out = s == o;
            case(Py_NE): out = s != o;
            case(Py_GE): out = s >= o;
        }
        return to_python(out);
    });
}

PyTypeObject TypeIndexType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cpy.TypeIndex",
    .tp_hash = type_index_hash,
    .tp_str = type_index_name,
    .tp_richcompare = type_index_compare,
    .tp_basicsize = sizeof(Holder<std::type_index>),
    .tp_dealloc = static_cast<destructor>(tp_delete<std::type_index>),
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = "C++ type_index object",
    .tp_new = type_index_new,
};

/******************************************************************************/



/******************************************************************************/

// GOOD


// I think this is not needed - just provide default
// int cpy_any_init(PyObject *self, PyObject *args, PyObject *kws) {
//     PyObject *value;
//     if (!PyArg_ParseTupleAndKeywords(args, kws, "|O", const_cast<char **>(keywords), &value)) return -1;
//     as_any(self).reset();
//     return 0;
// }

PyObject * any_copy_from(PyObject *self, PyObject *args) noexcept {
    PyObject *value = one_argument(args, self->ob_type);
    if (!value) return nullptr;
    std::cout << "fix copy" << std::endl;
    as_any(self) = as_any(value); // not notexcept
    Py_RETURN_NONE;
}

int any_bool(PyObject *self) {
    return as_any(self).has_value(); // noexcept
}

PyObject * any_cast_to(PyObject *self, PyObject *args) noexcept {
    return return_object([=]() -> Object {
        PyObject *type = one_argument(args);
        // if (!PyType_Check(type))
            // return raised(PyExc_TypeError, "Expected type object");
        // if (!PyObject_IsSubclass(type, (PyObject *) &AnyType))
            // return raised(PyExc_TypeError, "Expected subclass of Any");
        Object o{PyObject_CallObject(type, nullptr), false};
        if (!o) return o;
        as_any(+o) = std::move(as_any(self));
        // (+o)->ob_type = (PyTypeObject *) type;
        return o;
    });
}

PyObject * any_has_value(PyObject *self, PyObject *) noexcept {
    return return_object([=] {
        return to_python(as_any(self).has_value());
    });
}

PyObject * any_index(PyObject *self, PyObject *) noexcept {
    return return_object([=] {
        Object o{PyObject_CallObject((PyObject *) &TypeIndexType, nullptr), false};
        as_index(+o) = as_any(self).type();
        return o;
    });
}

PyNumberMethods cpy_any_number = {
    .nb_bool = static_cast<inquiry>(any_bool)
};

PyMethodDef AnyTypeMethods[] = {
    {"index",     static_cast<PyCFunction>(any_index),     METH_NOARGS, "index it"},
    {"cast_to",   static_cast<PyCFunction>(any_cast_to),   METH_VARARGS, "cast it"},
    {"move_from", static_cast<PyCFunction>(any_move_from), METH_VARARGS, "move it"},
    {"copy_from", static_cast<PyCFunction>(any_copy_from), METH_VARARGS, "copy it"},
    {"has_value", static_cast<PyCFunction>(any_has_value), METH_VARARGS, "has value"},
    {nullptr, nullptr, 0, nullptr}
};

PyTypeObject AnyType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cpy.Any",
    .tp_as_number = &cpy_any_number,
    .tp_basicsize = sizeof(Holder<Any>),
    .tp_dealloc = static_cast<destructor>(tp_delete<Any>),
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = "C++ class object",
    .tp_new = tp_new<Any>,
    .tp_methods = AnyTypeMethods
};

/******************************************************************************/

PyObject * function_call(PyObject *self, PyObject *args, PyObject *) noexcept {
    std::cout << "add keywords" << std::endl;
    return cpy::return_object([=]() -> Object {
        auto &s = as_function(self);
        if (!s) {
            PyErr_SetString(PyExc_ValueError, "Invalid C++ function");
            return {};
        }
        cpy::ArgPack pack;
        // this is some collection of arbitrary things that may include Any
        if (!put_argpack(pack, {args, true})) return {};
        // now the Anys have been moved inside the pack, no matter what.
        BaseContext ct;
        Object out = cpy::to_python(s(ct, pack));

        if (!get_argpack(pack, {args, true})) return {};
        return out;
        // but, now we should put back the Any in case it wasn't moved.
        // the alternative would be to redo Any into some sort of reference type
        // such that we don't have to move it back and forth
        // in that case... Any would just have to be maybe shared_ptr<Any> I suppose.
        // because we'd keep a handle to it here
        // and we'd need a handle in C++ too
        // we also couldn't move the value safely in C++ because
        // the reference count would be 2 probably
        // so yeah it seems best to use put_argpack and re-put afterwards.
    });
}

PyTypeObject FunctionType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cpy.Function",
    .tp_basicsize = sizeof(Holder<Function>),
    .tp_dealloc = static_cast<destructor>(tp_delete<Function>),
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = "C++ function object",
    .tp_new = tp_new<Function>,
    .tp_call = function_call,  // overload
    .tp_methods = nullptr
};

/******************************************************************************/

bool attach_type(PyObject *m, char const *name, PyTypeObject *t) noexcept {
    if (PyType_Ready(t) < 0) return false;
    Py_INCREF(t);
    PyModule_AddObject(m, name, reinterpret_cast<PyObject *>(t));
    return true;
}

template <class F>
bool attach(PyObject *m, char const *name, F &&f) noexcept {
    PyObject *o = return_object(static_cast<F &&>(f));
    if (!o) return false;
    Py_INCREF(o);
    PyModule_AddObject(m, name, o);
    return true;
}

/******************************************************************************/

}

extern "C" {

#if PY_MAJOR_VERSION > 2
    static struct PyModuleDef cpy_definition = {
        PyModuleDef_HEAD_INIT,
        CPY_STRING(CPY_MODULE),
        "A Python module to run C++ unit tests",
        -1,
        // cpy_methods
    };

    PyObject* CPY_CAT(PyInit_, CPY_MODULE)(void) {
        auto const &doc = cpy::document();

        Py_Initialize();
        auto m = PyModule_Create(&cpy_definition);
        return m
            && cpy::attach_type(m, "Any", &cpy::AnyType)
            && cpy::attach_type(m, "Function", &cpy::FunctionType)
            && cpy::attach_type(m, "TypeIndex", &cpy::TypeIndexType)
            // && cpy::attach(m, "methods_fun", [m]() -> cpy::Object {
            //     Py_INCREF(Py_None);
            //     return {PyCFunction_NewEx(&cpy::register_method, Py_None, m), false};
            // })
            && cpy::attach(m, "methods", [&] {
                return cpy::to_tuple(doc.methods, [](auto const &i) {return std::get<2>(i);});
            })
            && cpy::attach(m, "objects", [&] {
                return cpy::to_tuple(doc.values, [&](auto const &i) {return i.second;});
            })
            && cpy::attach(m, "type_names", [&] {
                return cpy::to_tuple(doc.types, [](auto const &i) {return i.second;});
            })
            && cpy::attach(m, "type_indices", [&] {
                return cpy::to_tuple(doc.types, [](auto const &i) {return i.first;});
            })
            && cpy::attach(m, "object_names", [&] {
                return cpy::to_tuple(doc.values, [](auto const &i) {return i.first;});
            })
            && cpy::attach(m, "method_scopes", [&] {
                return cpy::to_tuple(doc.methods, [](auto const &i) {return std::get<0>(i);});
            })
            && cpy::attach(m, "method_names", [&] {
                return cpy::to_tuple(doc.methods, [](auto const &i) {return std::get<1>(i);});
            })
        ? m : nullptr;
    }
#else
    void CPY_CAT(init, CPY_MODULE)(void) {
    }
#endif
}
