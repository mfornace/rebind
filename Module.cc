#include <cpy/PythonBindings.h>

#include <cpy/Test.h>
#include <cpy/Macros.h>
#include <cpy/PythonBindings.h>
#include <chrono>
#include <iostream>
#include <vector>

#ifndef CPY_MODULE
#   define CPY_MODULE libcpy
#endif

extern "C" {

/******************************************************************************/

typedef struct {
    PyObject_HEAD
    std::any value; // I think stack is OK because this object is only casted to anyway.
} cpy_AnyObject;

int cpy_any_init(PyObject *self, PyObject *args, PyObject *kws) {
    PyObject *value;
    char const *keywords[] = {"value", nullptr};
    if (!PyArg_ParseTupleAndKeywords(args, kws, "|O", const_cast<char **>(keywords), &value)) return -1;
    reinterpret_cast<cpy_AnyObject *>(self)->value.reset();
    return 0;
}

PyObject *cpy_any_new(PyTypeObject *subtype, PyObject *, PyObject *) {
    PyObject* o = subtype->tp_alloc(subtype, 0); // 0 unused
    new (&(reinterpret_cast<cpy_AnyObject *>(o)->value)) std::any; // noexcept
    return o;
}

void cpy_any_delete(PyObject *o) {
    reinterpret_cast<cpy_AnyObject *>(o)->~cpy_AnyObject();
    Py_TYPE(o)->tp_free(o);
}

static PyTypeObject cpy_AnyType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cpy.Any",
    .tp_basicsize = sizeof(cpy_AnyObject),
    .tp_dealloc = static_cast<destructor>(cpy_any_delete),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "std::any object",
    .tp_init = cpy_any_init,
    .tp_new = cpy_any_new
};

/******************************************************************************/

static PyMethodDef cpy_methods[] = {
    {"run_test",     (PyCFunction) cpy_run_test,     METH_VARARGS,
        "Run a unit test. Positional arguments:\n"
        "i (int):             test index\n"
        "callbacks (tuple):   list of callbacks for each event\n"
        "args (tuple or int): arguments to apply, or index of the already-registered argument pack\n"
        "gil (bool):          whether to keep the Python global interpreter lock on\n"
        "cout (bool):         whether to redirect std::cout\n"
        "cerr (bool):         whether to redirect std::cerr\n"},
    {"find_test",    (PyCFunction) cpy_find_test,    METH_VARARGS,
        "Find the index of a unit test from its registered name (str)"},
    {"n_tests",      (PyCFunction) cpy_n_tests,      METH_NOARGS,  "Number of registered tests (no arguments)"},
    {"compile_info", (PyCFunction) cpy_compile_info, METH_NOARGS,  "Compilation information (no arguments)"},
    {"test_names",   (PyCFunction) cpy_test_names,   METH_NOARGS,  "Names of registered tests (no arguments)"},
    {"test_info",    (PyCFunction) cpy_test_info,    METH_VARARGS, "Info of a registered test from its index (int)"},
    {"n_parameters", (PyCFunction) cpy_n_parameters, METH_VARARGS, "Number of parameter packs of a registered test from its index (int)"},
    {"add_test",     (PyCFunction) cpy_add_test,     METH_VARARGS, "Add a unit test from a python function"},
    {"add_value",    (PyCFunction) cpy_add_value,    METH_VARARGS, "Add a unit test from a python value"},
    {nullptr, nullptr, 0, nullptr}};

/******************************************************************************/

#define CPY_STRING0(x) #x
#define CPY_STRING(x) CPY_STRING0(x)

#if PY_MAJOR_VERSION > 2
    static struct PyModuleDef cpy_definition = {
        PyModuleDef_HEAD_INIT,
        CPY_STRING(CPY_MODULE),
        "A Python module to run C++ unit tests",
        -1,
        cpy_methods
    };

    PyObject* CPY_CAT(PyInit_, CPY_MODULE)(void) {
        Py_Initialize();
        if (PyType_Ready(&cpy_AnyType) < 0) return nullptr;
        auto m = PyModule_Create(&cpy_definition);
        Py_INCREF(&cpy_AnyType);
        PyModule_AddObject(m, "Any", reinterpret_cast<PyObject *>(&cpy_AnyType));
        return m;
    }
#else
    void CPY_CAT(init, CPY_MODULE)(void) {
        if (PyType_Ready(&cpy_AnyType) < 0) return;
        auto m = Py_InitModule(CPY_STRING(CPY_MODULE), cpy_methods);
        Py_INCREF(&cpy_AnyType);
        PyModule_AddObject(m, "Any", reinterpret_cast<PyObject *>(&cpy_AnyType));
    }
#endif

/******************************************************************************/

}
