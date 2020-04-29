/**
 * @brief Python-related C++ source code for ara
 * @file Python.cc
 */
#include <ara-python/Cast.h>
#include <ara-python/Common.h>
#include <ara-cpp/Schema.h>
#include <any>
#include <iostream>
#include <numeric>

#ifndef ARA_MODULE
#   define ARA_MODULE libara
#endif

#define ARA_CAT_IMPL(s1, s2) s1##s2
#define ARA_CAT(s1, s2) ARA_CAT_IMPL(s1, s2)

#define ARA_STRING_IMPL(x) #x
#define ARA_STRING(x) ARA_STRING_IMPL(x)

#include "Function.h"
#include "Definitions.cc"

namespace ara::py {

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
            as_object(t)
            // map_as_tuple(t->properties, [](auto const &x) {
            //     return tuple_from(as_object(x.first), Variable::from(Value(x.second)));
            // })
        );
    });
}

/******************************************************************************/

Object initialize(Schema const &schema) {
    initialize_global_objects();

    auto m = Object::from(PyDict_New());

    if (PyType_Ready(TypePtr::from<ArrayBuffer>()) < 0) return {};
    incref(TypePtr::from<ArrayBuffer>());

    DUMP("making python");

    bool s = true;

    // Builtin types
    s = s && attach_type(m, "Variable", TypePtr::from<Variable>());
    // s = s && attach_type(m, "Ref", TypePtr::from<Ref>());
    s = s && attach_type(m, "Index", TypePtr::from<Index>());

    // scalars: exposed as Tuple[Tuple[int, Index, int], ...]
    s = s && attach(m, "scalars", map_as_tuple(scalars, [](auto const &x) {
        return tuple_from(as_object(static_cast<Integer>(std::get<0>(x))),
                          as_object(static_cast<Index>(std::get<1>(x))),
                          as_object(static_cast<Integer>(std::get<2>(x))));
    }));

    // Tuple[Tuple[Index, Tuple[Tuple[str, function], ...]], ...]
    s = s && attach(m, "contents", map_as_tuple(schema.raw.contents, [](auto const &x) {
        Object o;
        if (auto p = x.second.template target<Index>()) { // a type index
            o = as_object(*p);
        // } else if (auto p = x.second.template target<Vector<Index>>()) { // a type table
            // o = tables_to_object(*p);
        // } else if (auto p = x.second.template target<Ref>()) {
        //     o = ref_to_object(*p);
        } else { // anything else
            o = Variable::from(x.second.as_ref());
        }
        return tuple_from(as_object(x.first), std::move(o));
    }));

    // Configuration functions
    s = s && attach(m, "set_output_conversion", Variable::from(make_functor([](Object t, Object o) {
        output_conversions.insert_or_assign(std::move(t), std::move(o));
    })));
    s = s && attach(m, "set_input_conversion", Variable::from(make_functor([](Object t, Object o) {
        input_conversions.insert_or_assign(std::move(t), std::move(o));
    })));
    s = s && attach(m, "set_translation", Variable::from(make_functor([](Object t, Object o) {
        DUMP("set_translation ", repr(t), " -> ", repr(o));
        type_translations.insert_or_assign(std::move(t), std::move(o));
    })));

    s = s && attach(m, "clear_global_objects", Variable::from(make_functor(&clear_global_objects)));

    s = s && attach(m, "set_debug", Variable::from(make_functor([](bool b) {
        DUMP("set_debug ", b);
        return std::exchange(Debug, b);})));

    s = s && attach(m, "debug", Variable::from(make_functor([] {return Debug;})));

    s = s && attach(m, "set_type_error", Variable::from(make_functor([](Object o) {
        DUMP("setting type error");
        TypeError = std::move(o);
    })));

    s = s && attach(m, "set_type", Variable::from(make_functor([](Index idx, Object cls, Object ref) {
        DUMP("set_type in");
        python_types[idx] = {std::move(cls), std::move(ref)};
        DUMP("set_type out");
    })));

    DUMP("attached all module objects, status (1 is good) = ", s);
    return s ? m : Object();
}

}

namespace ara {

void init(Schema &schema);

}

extern "C" {

#if PY_MAJOR_VERSION > 2
    static struct PyModuleDef ara_definition = {
        PyModuleDef_HEAD_INIT,
        ARA_STRING(ARA_MODULE),
        "A Python module to run C++ unit tests",
        -1,
    };

    PyObject* ARA_CAT(PyInit_, ARA_MODULE)(void) {
        Py_Initialize();
        return ara::py::raw_object([&]() -> ara::py::Object {
            ara::py::Object mod {PyModule_Create(&ara_definition), true};
            if (!mod) return {};
            ara::Schema schema{ara::global_schema()};
            ara::init(schema);
            ara::py::Object dict = ara::py::initialize(schema);
            if (!dict) return {};
            ara::py::incref(+dict);
            if (PyModule_AddObject(+mod, "schema", +dict) < 0) return {};
            return mod;
        });
    }
#else
    void ARA_CAT(init, ARA_MODULE)(void) {

    }
#endif
}
