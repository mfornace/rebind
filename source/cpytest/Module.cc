#include <cpytest/Test.h>
#include <cpytest/Macros.h>
#include <cpytest/CFunctions.h>
#include <chrono>
#include <iostream>
#include <vector>

#ifndef CPY_MODULE
#   define CPY_MODULE libcpy
#endif

namespace cpy
{
    bool define_any(PyObject *);
}
struct Data {
    int number=5;
};
static Data test_int{5};
static Data * test_int_address = &test_int;

extern "C" {

PyObject *cpy_test_data_function(PyObject *self, PyObject *args) {
    Py_ssize_t i;
    if (!PyArg_ParseTuple(args, "n", &i)) return nullptr;
    auto n = (std::size_t)(self);
    return Py_BuildValue("n", static_cast<Py_ssize_t>(n));
}

static PyMethodDef cpy_test_data_meth= {
    "function_name",
    cpy_test_data_function,
    METH_VARARGS,
    "the doc"
};

static auto cpy_test_data = PyCFunction_New(&cpy_test_data_meth, nullptr);
auto x = (getattrfunc) nullptr;
/******************************************************************************/

static PyMethodDef cpy_methods[] = {
    {"run_test",     (PyCFunction) cpy_run_test,     METH_VARARGS,
        "Run a unit test. Positional arguments:\n"
        "i (int):             test index\n"
        "handlers (tuple):    list of handlers for each event\n"
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
        auto m = PyModule_Create(&cpy_definition);
        if (!cpy::define_any(m)) return nullptr;
        PyModule_AddObject(m, "test_data", cpy_test_data);
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
