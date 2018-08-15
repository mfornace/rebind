/**
 * @brief Python-related C++ source code for cpy
 * @file Python.cc
 */
#include <cpy/PythonAPI.h>
#include <cpy/Function.h>
#include <any>
#include <iostream>

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
        v = Value(std::in_place_t(), std::move(cast_value<Any>(+o)));
    } else if (PyObject_TypeCheck(+o, &FunctionType)) {
        v = Value(std::move(cast_value<Function>(+o)));
    // } else if (PyObject_CheckBuffer(+o)) {
// hmm
    // } else if (PyMemoryView_Check(+o)) {
// hmm
    } else {
        PyErr_SetString(PyExc_TypeError, "Invalid type for conversion to C++");
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
            cast_value<Any>(+o) = std::move(std::get<Any>(it->var));
        }
        if (std::holds_alternative<Function>(it->var)) {
            if (!PyObject_TypeCheck(+o, &FunctionType)) return false;
            cast_value<Function>(+o) = std::move(std::get<Function>(it->var));
        }
        ++it;
        return true;
    });
}

/******************************************************************************/

PyObject *one_argument(PyObject *args, PyTypeObject *type) {
    if (PyTuple_Size(args) != 1) {
        PyErr_SetString(PyExc_TypeError, "Expected single argument");
        return nullptr;
    }
    PyObject *value = PyTuple_GET_ITEM(args, 0);
    if (!PyObject_TypeCheck(value, type)) {
        PyErr_SetString(PyExc_TypeError, "Invalid argument type");
        return nullptr;
    }
    return value;
}

template <class T>
PyObject *tp_new(PyTypeObject *subtype, PyObject *, PyObject *) {
    PyObject* o = subtype->tp_alloc(subtype, 0); // 0 unused
    new (&cast_value<T>(o)) T; // noexcept
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
    cast_value<Any>(self) = std::move(cast_value<Any>(value)); // noexcept
    Py_RETURN_NONE;
}

/******************************************************************************/

// GOOD


// I think this is not needed - just provide default
// int cpy_any_init(PyObject *self, PyObject *args, PyObject *kws) {
//     PyObject *value;
//     if (!PyArg_ParseTupleAndKeywords(args, kws, "|O", const_cast<char **>(keywords), &value)) return -1;
//     cast_value<Any>(self).reset();
//     return 0;
// }

PyObject * any_copy_from(PyObject *self, PyObject *args) {
    PyObject *value = one_argument(args, self->ob_type);
    if (!value) return nullptr;
    std::cout << "fix copy" << std::endl;
    cast_value<Any>(self) = cast_value<Any>(value); // not notexcept
    Py_RETURN_NONE;
}

int cpy_any_bool(PyObject *self) {
    return cast_value<Any>(self).has_value(); // noexcept
}

PyObject * cpy_any_has_value(PyObject *self, PyObject *) {
    if (cast_value<Any>(self).has_value()) Py_RETURN_TRUE;
    Py_RETURN_FALSE;
}

static PyMethodDef cpy_any_methods[] = {
    {"move_from", static_cast<PyCFunction>(any_move_from), METH_VARARGS, "move it"},
    {"copy_from", static_cast<PyCFunction>(any_copy_from), METH_VARARGS, "copy it"},
    {"has_value", static_cast<PyCFunction>(cpy_any_has_value), METH_VARARGS, "has value"},
    {nullptr, nullptr, 0, nullptr}
};

PyNumberMethods cpy_any_number = {
    .nb_bool = static_cast<inquiry>(cpy_any_bool)
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
    // .tp_init = cpy_any_init,  // overload
    .tp_methods = cpy_any_methods
};

/******************************************************************************/

PyObject * function_call(PyObject *self, PyObject *args, PyObject *) {
    std::cout << "add keywords" << std::endl;
    if (!cast_value<Function>(self)) {
        PyErr_SetString(PyExc_ValueError, "Invalid C++ function");
        return nullptr;
    }
    return cpy::return_object([=] {
        cpy::ArgPack pack;
        // this is some collection of arbitrary things that may include Any
        if (!put_argpack(pack, {args, true})) return cpy::Object();
        // now the Anys have been moved inside the pack, no matter what.
        BaseContext ct;
        Object out = cpy::to_python(cast_value<Function>(self)(ct, pack));

        if (!get_argpack(pack, {args, true})) return cpy::Object();
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

static PyMethodDef function_method = {
    .ml_name = "__call__",
    .ml_meth = (PyCFunction) function_call,
    .ml_flags = METH_VARARGS | METH_KEYWORDS,
    .ml_doc = "a test method"
};

PyTypeObject FunctionType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cpy.Function",
    .tp_basicsize = sizeof(Holder<Function>),
    .tp_dealloc = static_cast<destructor>(tp_delete<Function>),
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = "C++ function object",
    .tp_new = tp_new<Function>,
    .tp_call = function_call,  // overload
    .tp_methods = nullptr // overload
};

/******************************************************************************/

bool define_types(PyObject *m) {
    if (PyType_Ready(&AnyType) < 0) return false;
    Py_INCREF(&AnyType);
    PyModule_AddObject(m, "Any", reinterpret_cast<PyObject *>(&AnyType));

    if (PyType_Ready(&FunctionType) < 0) return false;
    Py_INCREF(&FunctionType);
    PyModule_AddObject(m, "Function", reinterpret_cast<PyObject *>(&FunctionType));
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
        Py_Initialize();
        auto m = PyModule_Create(&cpy_definition);
        if (!cpy::define_types(m)) return nullptr;
        auto const &d = cpy::document();

        auto keys = cpy::return_object([&] {
            return cpy::to_tuple(d.values, [](auto const &i) {return i.first;});
        });
        auto values = cpy::return_object([&] {
            return cpy::to_tuple(d.values, [](auto const &i) {return i.second;});
        });
        if (keys && values) {
            Py_INCREF(+keys);
            PyModule_AddObject(m, "keys", +keys);
            Py_INCREF(+values);
            PyModule_AddObject(m, "values", +values);
        } else return nullptr;
        return m;
    }
#else
    void CPY_CAT(init, CPY_MODULE)(void) {
        cpy::define_types(m);
    }
#endif
}
