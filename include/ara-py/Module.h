// #include <ara-py/Variable.h>
#include "Call.h"
#include "Dump.h"
#include "Meta.h"

#include <structmember.h>
#include <unordered_map>
#include <deque>


/******************************************************************************************/

namespace ara::py {

/******************************************************************************/

template <class T>
bool add_module_type(PyObject* mod, char const* name) {
    T::initialize_type(T::def());
    if (PyType_Ready(+T::def()) < 0) return false;
    Py_INCREF(~T::def());
    if (PyModule_AddObject(mod, name, ~T::def()) < 0) return false;
    return true;
}

/******************************************************************************/

PyObject* init_module() noexcept {
    static PyMethodDef methods[] = {
        {"load_library", reinterpret_kws<load_library, Ignore>, METH_VARARGS | METH_KEYWORDS,
            "load_library(file_name, function_name)\n--\n\nload index from a DLL"},
        {"load_address", reinterpret<load_address, nullptr, Ignore, Always<>>, METH_O,
            "load_address(integer_address)\n--\n\nload index from an address"},
        {nullptr, nullptr, 0, nullptr}
    };

    // Needs to be static (either in function or outside)
    static PyModuleDef module = {
        PyModuleDef_HEAD_INIT,
        "sfb",
        "sfb Python module",
        -1,
        methods,
    };

    Py_Initialize();
    PyObject* mod = PyModule_Create(&module);
    if (!mod) return nullptr;
    if (!add_module_type<pyIndex>(mod, "Index")) return nullptr;
    if (!add_module_type<pyVariable>(mod, "Variable")) return nullptr;
    if (!add_module_type<pyMethod>(mod, "Method")) return nullptr;
    if (!add_module_type<pyForward>(mod, "Forward")) return nullptr;
    if (!add_module_type<pyBoundMethod>(mod, "BoundMethod")) return nullptr;
    if (!add_module_type<pyMember>(mod, "Member")) return nullptr;

    DUMP("returning", bool(mod));
    return mod;
}

}

