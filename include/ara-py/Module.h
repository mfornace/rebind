// #include <ara-py/Variable.h>
#include "Call.h"
#include "Load.h"
#include "Dump.h"

#include <structmember.h>
#include <unordered_map>
#include <deque>


struct Example;

namespace ara {

template <>
ara_stat Switch<Example>::call(ara_input, void*, void*, void*) noexcept;

}

/******************************************************************************************/

namespace ara::py {

/******************************************************************************/

template <class T>
bool add_module_type(PyObject* mod, char const* name) {
    T::initialize(T::def());
    if (PyType_Ready(+T::def()) < 0) return false;
    // incref(t);
    if (PyModule_AddObject(mod, name, ~T::def()) < 0) return false;
    return true;
}

/******************************************************************************/

template<>
PyObject* init_module<Example>() noexcept {
    Py_Initialize();

    DUMP("initializing...");
    try {
        Module<Example>::init();
    } catch (std::exception const &e) {
        return type_error("Failed to initialize ara module: %s", e.what());
    } catch (...) {
        return type_error("Failed to initialize ara module: unknown error");
    }

    DUMP("initializing...done");
    static PyMethodDef methods[] = {
        {"call", c_function(c_module_call<Example>), METH_VARARGS | METH_KEYWORDS, "Call a function"},
        {nullptr, nullptr, 0, nullptr}
    };

    // Needs to be static (either in function or outside)
    static PyModuleDef module = {
        PyModuleDef_HEAD_INIT,
        "cpp",
        "A Python module",
        -1,
        methods,
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
    if (!add_module_type<Meta>(mod, "Meta")) return nullptr;
    if (!add_module_type<pyVariable>(mod, "Variable")) return nullptr;

    return mod;
}

}