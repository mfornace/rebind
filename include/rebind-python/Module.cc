/**
 * @brief Python-related C++ source code for rebind
 * @file Python.cc
 */
#include <rebind-python/Cast.h>
#include <rebind-python/API.h>
#include <rebind/Schema.h>
#include <any>
#include <iostream>
#include <numeric>

#ifndef REBIND_MODULE
#   define REBIND_MODULE librebind
#endif

#define REBIND_CAT_IMPL(s1, s2) s1##s2
#define REBIND_CAT(s1, s2) REBIND_CAT_IMPL(s1, s2)

#define REBIND_STRING_IMPL(x) #x
#define REBIND_STRING(x) REBIND_STRING_IMPL(x)

#include "Function.h"
#include "Definitions.cc"

namespace rebind::py {

// template <class F>
// constexpr PyMethodDef method(char const *name, F fun, int type, char const *doc) noexcept {
//     if constexpr (std::is_convertible_v<F, PyCFunction>)
//         return {name, static_cast<PyCFunction>(fun), type, doc};
//     else return {name, reinterpret_cast<PyCFunction>(fun), type, doc};
// }


/******************************************************************************/

bool attach_type(Object const &m, char const *name, PyTypeObject *t) noexcept {
    if (PyType_Ready(t) < 0) return false;
    incref(reinterpret_cast<PyObject *>(t));
    return PyDict_SetItemString(+m, name, reinterpret_cast<PyObject *>(t)) >= 0;
}

bool attach(Object const &m, char const *name, Object o) noexcept {
    return o && PyDict_SetItemString(m, name, o) >= 0;
}

/******************************************************************************/

// List[Table]
// where Table = Tuple[List[Property], bool]
// where Property = Tuple[str, Value]
Object tables_to_object(Vector<Index> const &v) {
    return map_as_tuple(v, [](auto const &t) {
        return tuple_from(
            as_object(t),
            map_as_tuple(t->properties, [](auto const &x) {
                return tuple_from(as_object(x.first), value_to_object(Value(x.second)));
            })
        );
    });
}

/******************************************************************************/

Object initialize(Schema const &schema) {
    initialize_global_objects();

    auto m = Object::from(PyDict_New());

    if (PyType_Ready(type_object<ArrayBuffer>()) < 0) return {};
    incref(type_object<ArrayBuffer>());

    DUMP("making python");

    bool s = true;

    // Builtin types
    s = s && attach_type(m, "Value", type_object<Value>());
    s = s && attach_type(m, "Ref", type_object<Ref>());
    s = s && attach_type(m, "Index", type_object<Index>());

    // scalars: exposed as Tuple[Tuple[int, Index, int], ...]
    s = s && attach(m, "scalars", map_as_tuple(scalars, [](auto const &x) {
        return tuple_from(as_object(static_cast<Integer>(std::get<0>(x))),
                          as_object(static_cast<Index>(std::get<1>(x))),
                          as_object(static_cast<Integer>(std::get<2>(x))));
    }));

    // Tuple[Tuple[Index, Tuple[Tuple[str, function], ...]], ...]
    s = s && attach(m, "contents", map_as_tuple(schema.contents, [](auto const &x) {
        Object o;
        if (auto p = x.second.template target<Index>()) { // a type index
            o = as_object(*p);
        } else if (auto p = x.second.template target<Vector<Index>>()) { // a type table
            o = tables_to_object(*p);
        // } else if (auto p = x.second.template target<Ref>()) {
        //     o = ref_to_object(*p);
        } else { // anything else
            o = value_to_object(Value(x.second));
        }
        return tuple_from(as_object(x.first), std::move(o));
    }));

    // Configuration functions
    s = s && attach(m, "set_output_conversion", value_to_object(declare_function([](Object t, Object o) {
        output_conversions.insert_or_assign(std::move(t), std::move(o));
    })));
    s = s && attach(m, "set_input_conversion", value_to_object(declare_function([](Object t, Object o) {
        input_conversions.insert_or_assign(std::move(t), std::move(o));
    })));
    s = s && attach(m, "set_translation", value_to_object(declare_function([](Object t, Object o) {
        DUMP("set_translation ", repr(t), " -> ", repr(o));
        type_translations.insert_or_assign(std::move(t), std::move(o));
    })));

    s = s && attach(m, "clear_global_objects", value_to_object(declare_function(&clear_global_objects)));

    s = s && attach(m, "set_debug", value_to_object(declare_function([](bool b) {return std::exchange(Debug, b);})));

    s = s && attach(m, "debug", value_to_object(declare_function([] {return Debug;})));

    s = s && attach(m, "set_type_error", value_to_object(declare_function([](Object o) {
        DUMP("setting type error");
        TypeError = std::move(o);
    })));

    s = s && attach(m, "set_type", value_to_object(declare_function([](Index idx, Object cls, Object ref) {
        DUMP("set_type in");
        python_types[idx] = {std::move(cls), std::move(ref)};
        DUMP("set_type out");
    })));

    DUMP("attached all module objects, status = ", s);
    return s ? m : Object();
}

}

namespace rebind {

void init(Schema &schema);

}

extern "C" {

#if PY_MAJOR_VERSION > 2
    static struct PyModuleDef rebind_definition = {
        PyModuleDef_HEAD_INIT,
        REBIND_STRING(REBIND_MODULE),
        "A Python module to run C++ unit tests",
        -1,
    };

    PyObject* REBIND_CAT(PyInit_, REBIND_MODULE)(void) {
        Py_Initialize();
        return rebind::py::raw_object([&]() -> rebind::py::Object {
            rebind::py::Object mod {PyModule_Create(&rebind_definition), true};
            if (!mod) return {};
            rebind::init(rebind::schema());
            rebind::py::Object dict = rebind::py::initialize(rebind::schema());
            if (!dict) return {};
            rebind::py::incref(+dict);
            if (PyModule_AddObject(+mod, "schema", +dict) < 0) return {};
            return mod;
        });
    }
#else
    void REBIND_CAT(init, REBIND_MODULE)(void) {

    }
#endif
}
