/**
 * @brief Python-related C++ source code for cpy
 * @file Python.cc
 */
#include <cpy-python/Cast.h>
#include <cpy-python/API.h>
#include <cpy/Document.h>
#include <any>
#include <iostream>
#include <numeric>

#ifndef CPY_MODULE
#   define CPY_MODULE libcpy
#endif

#define CPY_CAT_IMPL(s1, s2) s1##s2
#define CPY_CAT(s1, s2) CPY_CAT_IMPL(s1, s2)

#define CPY_STRING_IMPL(x) #x
#define CPY_STRING(x) CPY_STRING_IMPL(x)

#include "Var.cc"
#include "Function.cc"

namespace cpy {

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
PyTypeObject Holder<ArrayBuffer>::type = []{
    auto o = type_definition<ArrayBuffer>("cpy.ArrayBuffer", "C++ ArrayBuffer object");
    o.tp_as_buffer = &buffer_procs;
    return o;
}();

/******************************************************************************/

PyObject *type_index_new(PyTypeObject *subtype, PyObject *, PyObject *) noexcept {
    PyObject* o = subtype->tp_alloc(subtype, 0); // 0 unused
    if (o) new (&cast_object<TypeIndex>(o)) TypeIndex(typeid(void)); // noexcept
    return o;
}

long type_index_hash(PyObject *o) noexcept {
    return static_cast<long>(cast_object<TypeIndex>(o).hash_code());
}

PyObject *type_index_repr(PyObject *o) noexcept {
    TypeIndex const *p = cast_if<TypeIndex>(o);
    if (p) return PyUnicode_FromFormat("TypeIndex('%s')", get_type_name(*p).data());
    return type_error("Expected instance of cpy.TypeIndex");
}

PyObject *type_index_str(PyObject *o) noexcept {
    TypeIndex const *p = cast_if<TypeIndex>(o);
    if (p) return PyUnicode_FromString(get_type_name(*p).data());
    return type_error("Expected instance of cpy.TypeIndex");
}

PyObject *type_index_compare(PyObject *self, PyObject *other, int op) {
    return raw_object([=]() -> Object {
        return {compare(op, cast_object<TypeIndex>(self), cast_object<TypeIndex>(other)) ? Py_True : Py_False, true};
    });
}

template <>
PyTypeObject Holder<TypeIndex>::type = []{
    auto o = type_definition<TypeIndex>("cpy.TypeIndex", "C++ type_index object");
    o.tp_repr = type_index_repr;
    o.tp_hash = type_index_hash;
    o.tp_str = type_index_str;
    o.tp_richcompare = type_index_compare;
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

Object initialize(Document const &doc) {
    initialize_global_objects();

    auto m = Object::from(PyDict_New());
    for (auto const &p : doc.types)
        if (p.second) type_names.emplace(p.first, p.first.name());//p.second->first);

    if (PyType_Ready(type_object<ArrayBuffer>()) < 0) return {};
    incref(type_object<ArrayBuffer>());

    bool ok = attach_type(m, "Variable", type_object<Variable>())
        && attach_type(m, "Function", type_object<Function>())
        && attach_type(m, "TypeIndex", type_object<TypeIndex>())
            // Tuple[Tuple[int, TypeIndex, int], ...]
        && attach(m, "scalars", map_as_tuple(scalars, [](auto const &x) {
            return args_as_tuple(as_object(static_cast<Integer>(std::get<0>(x))),
                                 as_object(static_cast<TypeIndex>(std::get<1>(x))),
                                 as_object(static_cast<Integer>(std::get<2>(x))));
        }))
            // Tuple[Tuple[TypeIndex, Tuple[Tuple[str, function], ...]], ...]
        && attach(m, "contents", map_as_tuple(doc.contents, [](auto const &x) {
            Object o;
            if (auto p = x.second.template target<Function const &>()) o = as_object(*p);
            else if (auto p = x.second.template target<TypeIndex const &>()) o = as_object(*p);
            else if (auto p = x.second.template target<TypeData const &>()) o = args_as_tuple(
                map_as_tuple(p->methods, [](auto const &x) {return args_as_tuple(as_object(x.first), as_object(x.second));}),
                map_as_tuple(p->data, [](auto const &x) {return args_as_tuple(as_object(x.first), variable_cast(Variable(x.second)));})
            );
            else o = variable_cast(Variable(x.second));
            return args_as_tuple(as_object(x.first), std::move(o));
        }))
        && attach(m, "set_output_conversion", as_object(Function::of([](Object t, Object o) {
            output_conversions.insert_or_assign(std::move(t), std::move(o));
        })))
        && attach(m, "set_input_conversion", as_object(Function::of([](Object t, Object o) {
            input_conversions.insert_or_assign(std::move(t), std::move(o));
        })))
        && attach(m, "set_translation", as_object(Function::of([](Object t, Object o) {
            type_translations.insert_or_assign(std::move(t), std::move(o));
        })))
        && attach(m, "clear_global_objects", as_object(Function::of(&clear_global_objects)))
        && attach(m, "set_debug", as_object(Function::of([](bool b) {return std::exchange(Debug, b);})))
        && attach(m, "debug", as_object(Function::of([] {return Debug;})))
        && attach(m, "set_type_error", as_object(Function::of([](Object o) {TypeError = std::move(o);})))
        && attach(m, "set_type", as_object(Function::of([](TypeIndex idx, Object o) {
            DUMP("set_type in");
            python_types.emplace(idx.info(), std::move(o));
            DUMP("set_type out");
        })))
        && attach(m, "set_type_names", as_object(Function::of([](Zip<TypeIndex, std::string_view> v) {
            for (auto const &p : v) type_names.insert_or_assign(p.first, p.second);
        })));
    return ok ? m : Object();
}

}

extern "C" {

#if PY_MAJOR_VERSION > 2
    static struct PyModuleDef cpy_definition = {
        PyModuleDef_HEAD_INIT,
        CPY_STRING(CPY_MODULE),
        "A Python module to run C++ unit tests",
        -1,
    };

    PyObject* CPY_CAT(PyInit_, CPY_MODULE)(void) {
        Py_Initialize();
        return cpy::raw_object([&]() -> cpy::Object {
            cpy::Object mod {PyModule_Create(&cpy_definition), true};
            if (!mod) return {};
            cpy::Object dict = initialize(cpy::document());
            if (!dict) return {};
            cpy::incref(+dict);
            if (PyModule_AddObject(+mod, "document", +dict) < 0) return {};
            return mod;
        });
    }
#else
    void CPY_CAT(init, CPY_MODULE)(void) {

    }
#endif
}
