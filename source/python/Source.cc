// #include <ara-py/Variable.h>
#include <ara-py/Call.h>
#include <ara-py/Load.h>
#include <ara-py/Dump.h>
#include <ara-py/Methods.h>


struct Example;

namespace ara {

template <>
ara_stat impl<Example>::call(ara_input, void*, void*, ara_args*) noexcept;

}

/******************************************************************************************/

namespace ara::py {

/******************************************************************************/

Ptr index_new(PyTypeObject *subtype, Ptr, Ptr) noexcept {
    Ptr o = subtype->tp_alloc(subtype, 0); // 0 unused
    if (o) new (&cast_object<Index>(o)) Index(); // noexcept
    return o;
}

long index_hash(Ptr o) noexcept {
    return static_cast<long>(std::hash<Index>()(cast_object<Index>(o)));
}

Ptr index_repr(Ptr o) noexcept {
    Index const *p = cast_if<Index>(o);
    if (p) return PyUnicode_FromFormat("Index('%s')", p->name().data());
    return type_error("Expected instance of ara.Index");
}

Ptr index_str(Ptr o) noexcept {
    Index const *p = cast_if<Index>(o);
    if (p) return PyUnicode_FromString(p->name().data());
    return type_error("Expected instance of ara.Index");
}

// Ptr index_compare(Ptr self, Ptr other, int op) {
//     return raw_object([=]() -> Object {
//         return {compare(op, cast_object<Index>(self), cast_object<Index>(other)) ? Py_True : Py_False, true};
//     });
// }

void initialize_index(PyTypeObject *o) {
    define_type<Index>(o, "ara.Index", "Index");
    o->tp_repr = index_repr;
    o->tp_hash = index_hash;
    o->tp_str = index_str;
    // o->tp_richcompare = index_compare;
    // no init (just use default constructor)
    // tp_traverse, tp_clear
    // PyMemberDef, tp_members
};

/******************************************************************************/

PyNumberMethods VariableNumberMethods = {
    .nb_bool = static_cast<inquiry>(c_operator_has_value<Variable>),
};

PyMethodDef VariableMethods[] = {
    // {"copy_from", c_function(c_copy_from<Value>),
    //     METH_O, "assign from other using C++ copy assignment"},

    // {"move_from", c_function(c_move_from<Value>),
    //     METH_O, "assign from other using C++ move assignment"},

    // {"method", c_function(c_method<Value>),
        // METH_VARARGS | METH_KEYWORDS, "call a method given a name and arguments"},

    // {"address", c_function(c_address<Value>),
    //     METH_NOARGS, "get C++ pointer address"},

    // {"_ward", c_function(c_get_ward<Variable>),
        // METH_NOARGS, "get ward object"},

    // {"_set_ward", c_function(c_set_ward<Variable>),
        // METH_O, "set ward object and return self"},

    // {"is_stack_type", c_function(var_is_stack_type),
    // METH_NOARGS, "return if object is held in stack storage"},

    {"index", c_function(c_get_index<Variable>),
        METH_NOARGS, "return Index of the held C++ object"},

    {"has_value", c_function(c_has_value<Variable>),
        METH_NOARGS, "return if a C++ object is being held"},

    {"load", c_function(c_load<Variable>),
        METH_O, "cast to a given Python type"},

    // {"from_object", c_function(c_value_from),
        // METH_CLASS | METH_O, "cast an object to a given Python type"},
    {nullptr, nullptr, 0, nullptr}
};

/******************************************************************************/

void initialize_variable(PyTypeObject *o) {
    DUMP("defining Variable");
    define_type<Variable>(o, "ara.Variable", "Object class");
    o->tp_as_number = &VariableNumberMethods;
    DUMP("defining Variable");
    o->tp_methods = VariableMethods;
    DUMP("defining Variable");
    // o.tp_call = function_call;
    DUMP("defined Variable");
    // no init (just use default constructor)
    // tp_traverse, tp_clear
    // PyMemberDef, tp_members
};


template<> Ptr init_module<Example>() noexcept {
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
        {"call", c_function(c_call<Example>), METH_VARARGS, "Execute a shell command."},
        {NULL, NULL, 0, NULL}        /* Sentinel */
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
    Ptr mod = PyModule_Create(&module);
{
    auto t = TypePtr::from<Variable>();
    initialize_variable(t);
    if (PyType_Ready(t) < 0) return nullptr;
    // incref(t);
    if (PyModule_AddObject(mod, "Variable", t) < 0) return nullptr;
}

{
    auto t = TypePtr::from<Index>();
    initialize_index(t);
    if (PyType_Ready(t) < 0) return nullptr;
    // incref(t);
    if (PyModule_AddObject(mod, "Index", t) < 0) return nullptr;
}


    return mod;
}

}