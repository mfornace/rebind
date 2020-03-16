/**
 * @brief Python-related C++ source code for rebind
 * @file Python.cc
 */
#include <rebind-python/Cast.h>
#include <rebind-python/API.h>
#include <rebind/Document.h>
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

#include "Var.cc"
#include "Function.cc"
#include "Index.cc"

namespace rebind::py {

// template <class F>
// constexpr PyMethodDef method(char const *name, F fun, int type, char const *doc) noexcept {
//     if constexpr (std::is_convertible_v<F, PyCFunction>)
//         return {name, static_cast<PyCFunction>(fun), type, doc};
//     else return {name, reinterpret_cast<PyCFunction>(fun), type, doc};
// }

/******************************************************************************/

int array_data_buffer(PyObject *self, Py_buffer *view, int flags) {
    auto &p = cast_object<ArrayBuffer>(self);
    view->buf = p.data;
    if (p.base) {incref(p.base); view->obj = +p.base;}
    else view->obj = nullptr;
    view->itemsize = Buffer::itemsize(*p.type);
    view->len = p.n_elem;
    view->readonly = !p.mutate;
    view->format = const_cast<char *>(Buffer::format(*p.type).data());
    view->ndim = p.shape_stride.size() / 2;
    view->shape = p.shape_stride.data();
    view->strides = p.shape_stride.data() + view->ndim;
    view->suboffsets = nullptr;
    return 0;
}

PyBufferProcs buffer_procs{array_data_buffer, nullptr};

template <>
PyTypeObject Wrap<ArrayBuffer>::type = []{
    auto o = type_definition<ArrayBuffer>("rebind.ArrayBuffer", "C++ ArrayBuffer object");
    o.tp_as_buffer = &buffer_procs;
    return o;
}();

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
// where Table = Tuple[List[Method], bool]
// where Method = Tuple[str, Function]
Object tables_to_object(Vector<Table> const &v) {
    return map_as_tuple(v, [](auto const &t) {
        return tuple_from(
            map_as_tuple(t->methods, [](auto const &x) {return tuple_from(as_object(x.first), as_object(x.second));}),
            as_object(false)
            // map_as_tuple(p->data, [](auto const &x) {return tuple_from(as_object(x.first), variable_cast(Variable(x.second)));})
        );
    });
}

/******************************************************************************/

Object initialize(Document const &doc) {
    initialize_global_objects();

    auto m = Object::from(PyDict_New());
    for (auto const &p : doc.types)
        if (p.second) type_names.emplace(p.first, p.first.name());//p.second->first);

    if (PyType_Ready(type_object<ArrayBuffer>()) < 0) return {};
    incref(type_object<ArrayBuffer>());

    DUMP("making python");

    bool s = true;

    // Builtin types
    s = s && attach_type(m, "Value", type_object<Value>());
    s = s && attach_type(m, "Pointer", type_object<Pointer>());
    s = s && attach_type(m, "Function", type_object<Function>());
    // s = s && attach_type(m, "Overload", type_object<Overload>())
    s = s && attach_type(m, "Index", type_object<Index>());

    // scalars: exposed as Tuple[Tuple[int, Index, int], ...]
    s = s && attach(m, "scalars", map_as_tuple(scalars, [](auto const &x) {
        return tuple_from(as_object(static_cast<Integer>(std::get<0>(x))),
                                as_object(static_cast<Index>(std::get<1>(x))),
                                as_object(static_cast<Integer>(std::get<2>(x))));
    }));

    // Tuple[Tuple[Index, Tuple[Tuple[str, function], ...]], ...]
    s = s && attach(m, "contents", map_as_tuple(doc.contents, [](auto const &x) {
        Object o;
        if (auto p = x.second.template target<Function>()) { // a function
            o = as_object(*p);
        } else if (auto p = x.second.template target<Index>()) { // a type index
            o = as_object(*p);
        } else if (auto p = x.second.template target<Vector<Table>>()) { // a type table
            o = tables_to_object(*p);
        // } else if (auto p = x.second.template target<Pointer>()) {
        //     o = pointer_to_object(*p);
        } else { // anything else
            o = value_to_object(Value(x.second));
        }
        return tuple_from(as_object(x.first), std::move(o));
    }));

    // Configuration functions
    s = s && attach(m, "set_output_conversion", as_object(Overload::from([](Object t, Object o) {
        output_conversions.insert_or_assign(std::move(t), std::move(o));
    })));
    s = s && attach(m, "set_input_conversion", as_object(Overload::from([](Object t, Object o) {
        input_conversions.insert_or_assign(std::move(t), std::move(o));
    })));
    s = s && attach(m, "set_translation", as_object(Overload::from([](Object t, Object o) {
        type_translations.insert_or_assign(std::move(t), std::move(o));
    })));

    s = s && attach(m, "clear_global_objects", as_object(Overload::from(&clear_global_objects)));

    s = s && attach(m, "set_debug", as_object(Overload::from([](bool b) {return std::exchange(Debug, b);})));

    s = s && attach(m, "debug", as_object(Overload::from([] {return Debug;})));

    s = s && attach(m, "set_type_error", as_object(Overload::from([](Object o) {
        DUMP("setting type error");
        TypeError = std::move(o);
    })));

    s = s && attach(m, "set_type", as_object(Overload::from([](Index idx, Object o) {
        DUMP("set_type in");
        python_types.emplace(idx.info(), std::move(o));
        DUMP("set_type out");
    })));

    s = s && attach(m, "set_type_names", as_object(Overload::from([](Zip<Index, std::string_view> v) {
        for (auto const &p : v) type_names.insert_or_assign(p.first, p.second);
    })));

    DUMP("attached all module objects, status = ", s);
    return s ? m : Object();
}

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
            rebind::py::Object dict = rebind::py::initialize(rebind::document());
            if (!dict) return {};
            rebind::py::incref(+dict);
            if (PyModule_AddObject(+mod, "document", +dict) < 0) return {};
            return mod;
        });
    }
#else
    void REBIND_CAT(init, REBIND_MODULE)(void) {

    }
#endif
}
