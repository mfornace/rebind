#pragma once
#include "Raw.h"
#include <ara/Core.h>

namespace ara::py {

/******************************************************************************/

template <class T>
bool dump_arithmetic(Target &target, Always<> o) {
    DUMP("cast arithmetic in:", target.name());
    if (PyFloat_Check(+o)) return target.emplace<T>(PyFloat_AsDouble(+o));
    if (PyLong_Check(+o))  return target.emplace<T>(PyLong_AsLongLong(+o));
    if (PyBool_Check(+o))  return target.emplace<T>(+o == Py_True);
    if (PyNumber_Check(+o)) { // This can be hit for e.g. numpy.int64
        if (std::is_integral_v<T>) {
            if (auto i = Value<>::from(PyNumber_Long(+o)))
                return target.emplace<T>(PyLong_AsLongLong(+i));
        } else {
            if (auto i = Value<>::from(PyNumber_Float(+o)))
               return target.emplace<T>(PyFloat_AsDouble(+i));
        }
    }
    DUMP("cast arithmetic out:", target.name());
    return false;
}

inline ara::Str exact_cast(Always<> o, ara::Type<ara::Str>) {
    if (auto s = Maybe<pyStr>(o)) return as_string_view(*s);
    throw PythonError(type_error("expected str"));
}

inline Integer exact_cast(Always<> o, ara::Type<Integer>) {
    if (PyLong_Check(+o)) return PyLong_AsLongLong(+o);
    throw PythonError(type_error("expected int"));
}

/******************************************************************************/

inline bool dump_span(Target &target, Always<> o) {
    DUMP("dump_span");
    if (PyTuple_Check(+o)) {
        PyObject** start = reinterpret_cast<PyTupleObject*>(+o)->ob_item;
        std::size_t size = PyTuple_GET_SIZE(+o);
        DUMP("dumping tuple into span", start, size ? *start : nullptr);
        return target.emplace<Span>(reinterpret_cast<Export**>(start), size);
    }
    // depends on the guarantee below...hmmm...probably better to get rid of this approach
    if (PyList_Check(+o)) {
        PyObject** start = reinterpret_cast<PyListObject*>(+o)->ob_item;
        std::size_t size = PyList_GET_SIZE(+o);
        DUMP("dumping list into span", start, size ? *start : nullptr);
        return target.emplace<Span>(reinterpret_cast<Export**>(start), size);
    }
    return false;
}

/******************************************************************************/

struct ObjectGuard {
    void operator()(PyObject* object) {Py_DECREF(object);}

    static auto make_unique(Always<> o) noexcept {
        Py_INCREF(+o);
        return std::unique_ptr<PyObject, ObjectGuard>(+o);
    }
};

inline bool dump_array(Target &target, Always<> o) {
    DUMP("dump_array");
    if (PyTuple_Check(+o)) {
        PyObject** start = reinterpret_cast<PyTupleObject*>(+o)->ob_item;
        std::size_t size = PyTuple_GET_SIZE(+o);
        return target.emplace<Array>(reinterpret_cast<Export**>(start), size, ObjectGuard::make_unique(o));
    }
    if (PyList_Check(+o)) {
        PyObject** start = reinterpret_cast<PyListObject*>(+o)->ob_item;
        std::size_t size = PyList_GET_SIZE(+o);
        return target.emplace<Array>(reinterpret_cast<Export**>(start), size, ObjectGuard::make_unique(o));
    }
    if (PyDict_Check(+o)) {
        std::size_t const len = PyDict_Size(+o);
        auto a = Value<>::from(PyTuple_New(len));

        Py_ssize_t pos = 0;
        for (std::size_t i = 0; i != len; ++i) {
            PyObject *key, *value;
            PyDict_Next(+o, &pos, &key, &value);
            Py_INCREF(key);
            Py_INCREF(value);
            PyTuple_SET_ITEM(+a, 2 * i, key);
            PyTuple_SET_ITEM(+a, 2 * i + 1, value);
        }
        return target.emplace<Array>(reinterpret_cast<PyTupleObject*>(+a)->ob_item,
            std::array<std::size_t, 2>{len, 2}, ObjectGuard::make_unique(*a));
    }
    return false;
}

/******************************************************************************/

inline View::Alloc allocated_view(Always<> o) {
    if (PyList_Check(+o)) {
        View::Alloc alloc(PyList_GET_SIZE(+o));
        for (Py_ssize_t i = 0; i != alloc.size; ++i)
            alloc.data[i] = Ref(reinterpret_cast<Export&>(*PyList_GET_ITEM(+o, i)));
        return alloc;
    }
    if (PyTuple_Check(+o)) {
        View::Alloc alloc(PyTuple_GET_SIZE(+o));
        for (Py_ssize_t i = 0; i != alloc.size; ++i)
            alloc.data[i] = Ref(reinterpret_cast<Export&>(*PyTuple_GET_ITEM(+o, i)));
        return alloc;
    }

    Value<> iter(PyObject_GetIter(+o), true);
    if (!iter) {
        PyErr_Clear();
        return {};
    }
    std::vector<Ref> v;
    Value<> item;
    while ((item = {PyIter_Next(+iter), true}))
        v.emplace_back(reinterpret_cast<Export&>(*(+o)));

    View::Alloc alloc(v.size());
    std::copy(std::make_move_iterator(v.begin()), std::make_move_iterator(v.end()), alloc.data.get());
    return alloc;
}

/******************************************************************************/

inline bool dump_view(Target& target, Always<> o) {
    auto alloc = allocated_view(o);
    return alloc.data && target.emplace<View>(std::move(alloc));
}

inline bool dump_tuple(Target& target, Always<> o) {
    auto p = allocated_view(o);
    return p.data && target.emplace<Tuple>(std::move(p.data), p.size, ObjectGuard::make_unique(o));
    return false;
}

/******************************************************************************/

inline bool dump_object(Target &target, Always<> o) {
    DUMP("dumping object");

    if (auto v = Maybe<Variable>(o)) {
        auto acquired = acquire_ref(**v, LockType::Read);
        return acquired.ref.get_to(target);
    }

    if (target.accepts<Str>()) {
        if (auto p = Maybe<pyStr>(o)) return target.emplace<ara::Str>(as_string_view(*p));
        if (auto p = Maybe<pyBytes>(o)) return target.emplace<ara::Str>(as_string_view(*p));
        return false;
    }

    if (target.accepts<String>()) {
        if (auto p = Maybe<pyStr>(o)) return target.emplace<String>(as_string_view(*p));
        if (auto p = Maybe<pyBytes>(o)) return target.emplace<String>(as_string_view(*p));
        return false;
    }

    if (target.accepts<Index>()) {
        if (auto p = Maybe<pyIndex>(o)) return target.emplace<Index>(*p);
        else return false;
    }

    if (target.accepts<Float>())
        return dump_arithmetic<Float>(target, o);

    if (target.accepts<Integer>())
        return dump_arithmetic<Integer>(target, o);

    if (target.accepts<Span>())
        return dump_span(target, o);

    if (target.accepts<Array>())
        return dump_array(target, o);

    if (target.accepts<View>())
        return dump_view(target, o);

    if (target.accepts<Tuple>())
        return dump_tuple(target, o);

    if (target.accepts<Bool>()) {
        return target.emplace<Bool>(Bool{static_cast<bool>(PyObject_IsTrue(+o))});
    }

    return false;
}

/******************************************************************************/

}

namespace ara {

template <>
struct Impl<py::Export> : Default<py::Export> {
    static bool dump(Target &v, py::Export const &o) {return false;}

    static bool dump(Target &v, py::Export &o) {
        DUMP("dumping object!");
        return py::dump_object(v, reinterpret_cast<PyObject&>(o));
    }
};

template <>
struct Impl<py::Export*> : Default<py::Export*> {
    static bool dump(Target &v, py::Export* o) {
        DUMP("dumping object pointer!", bool(o), o, reinterpret_cast<std::uintptr_t>(o));
        return py::dump_object(v, *reinterpret_cast<PyObject*>(o));
    }
};

}