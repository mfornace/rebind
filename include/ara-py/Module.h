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
    Py_Initialize();

    // DUMP("initializing...done");
    // static PyMethodDef methods[] = {
        // {"call", reinterpret<module_call<Example>, Ignore, Always<pyTuple>, CallKeywords>, METH_VARARGS | METH_KEYWORDS, "Call a function"},
        // {nullptr, nullptr, 0, nullptr}
    // };

    // Needs to be static (either in function or outside)
    static PyModuleDef module = {
        PyModuleDef_HEAD_INIT,
        "sfb",
        "sfb Python module",
        -1,
        nullptr
        // methods,
    };
    // the Schema satisfies:
    // - it is an empty type
    // - you can call Schema()(name, tags, arguments)
    // - that is, the first argument is always a name.
    // - "self" is not used because there is no data held by the type.

    // one option is to define a single function:  lookup(str, *args) -> result
    // - for this all we would need is the Index

    // another option is to define a function for each str in the schema: lookup(str, *args) -> result
    // - for this, the tags would not be that good.
    // - the strings may not be valid, OK this is not good.

    Py_Initialize();
    PyObject* mod = PyModule_Create(&module);
    if (!mod) return nullptr;
    if (!add_module_type<pyIndex>(mod, "Index")) return nullptr;
    if (!add_module_type<pyVariable>(mod, "Variable")) return nullptr;
    if (!add_module_type<pyMethod>(mod, "Method")) return nullptr;
    if (!add_module_type<pyFunction>(mod, "Function")) return nullptr;
    if (!add_module_type<pyForward>(mod, "Forward")) return nullptr;
    if (!add_module_type<pyBoundMethod>(mod, "BoundMethod")) return nullptr;
    if (!add_module_type<pyMember>(mod, "Member")) return nullptr;

    DUMP("returning", bool(mod));
    return mod;
}

}

