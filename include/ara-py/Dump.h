#pragma once
#include "Raw.h"
#include <ara/Core.h>

namespace ara::py {

/******************************************************************************/

inline Str from_unicode(Instance<PyUnicodeObject> o) {
    Py_ssize_t size;
#if PY_MAJOR_VERSION > 2
    char const *c = PyUnicode_AsUTF8AndSize(o.object(), &size);
#else
    char *c;
    if (PyString_AsStringAndSize(o.object(), &c, &size)) throw PythonError();
#endif
    if (!c) throw PythonError();
    return Str(static_cast<char const *>(c), size);
}

/******************************************************************************/

inline Str from_bytes(Instance<PyBytesObject> o) {
    char *c;
    Py_ssize_t size;
    PyBytes_AsStringAndSize(o.object(), &c, &size);
    if (!c) throw PythonError();
    return Str(c, size);
}

/******************************************************************************/

template <class T>
bool dump_arithmetic(Target &target, Instance<> o) {
    DUMP("cast arithmetic in:", target.name());
    if (PyFloat_Check(+o)) return target.emplace_if<T>(PyFloat_AsDouble(+o));
    if (PyLong_Check(+o))  return target.emplace_if<T>(PyLong_AsLongLong(+o));
    if (PyBool_Check(+o))  return target.emplace_if<T>(+o == Py_True);
    if (PyNumber_Check(+o)) { // This can be hit for e.g. numpy.int64
        if (std::is_integral_v<T>) {
            if (auto i = Shared::from(PyNumber_Long(+o)))
                return target.emplace_if<T>(PyLong_AsLongLong(+i));
        } else {
            if (auto i = Shared::from(PyNumber_Float(+o)))
               return target.emplace_if<T>(PyFloat_AsDouble(+i));
        }
    }
    DUMP("cast arithmetic out:", target.name());
    return false;
}

inline PyUnicodeObject* get_unicode(Instance<> o) {
    return PyUnicode_Check(+o) ? reinterpret_cast<PyUnicodeObject *>(+o) : nullptr;
}

inline PyBytesObject* get_bytes(Instance<> o) {
    return PyBytes_Check(+o) ? reinterpret_cast<PyBytesObject *>(+o) : nullptr;
}

/******************************************************************************/

inline bool dump_span(Target &target, Instance<> o) {
    DUMP("dump_span");
    if (PyTuple_Check(+o)) {
        PyObject** start = reinterpret_cast<PyTupleObject*>(+o)->ob_item;
        std::size_t size = PyTuple_GET_SIZE(+o);
        DUMP("dumping tuple into span", start, size ? *start : nullptr);
        return target.emplace_if<Span>(reinterpret_cast<Export**>(start), size);
    }
    // depends on the guarantee below...hmmm...probably better to get rid of this approach
    if (PyList_Check(+o)) {
        PyObject** start = reinterpret_cast<PyListObject*>(+o)->ob_item;
        std::size_t size = PyList_GET_SIZE(+o);
        DUMP("dumping list into span", start, size ? *start : nullptr);
        return target.emplace_if<Span>(reinterpret_cast<Export**>(start), size);
    }
    return false;
}

/******************************************************************************/

static void dump_array_deleter(ara_index, void* storage) {Py_DECREF(static_cast<PyObject*>(storage));}

inline bool dump_array(Target &target, Instance<> o) {
    DUMP("dump_array");
    if (PyTuple_Check(+o)) {
        PyObject** start = reinterpret_cast<PyTupleObject*>(+o)->ob_item;
        std::size_t size = PyTuple_GET_SIZE(+o);
        if (auto p = target.emplace_if<Array>(reinterpret_cast<Export**>(start), size, Array::Deleter{+o, &dump_array_deleter})) {
            Py_INCREF(static_cast<PyObject*>(p->c.storage));
            return true;
        }
    }
    if (PyList_Check(+o)) {
        PyObject** start = reinterpret_cast<PyListObject*>(+o)->ob_item;
        std::size_t size = PyList_GET_SIZE(+o);
        if (auto p = target.emplace_if<Array>(reinterpret_cast<Export**>(start), size, Array::Deleter{+o, &dump_array_deleter})) {
            Py_INCREF(static_cast<PyObject*>(p->c.storage));
            return true;
        }
    }
    if (PyDict_Check(+o)) {
        std::size_t len = PyDict_Size(+o);
        auto a = std::make_unique<Export*[]>(2 * len);
        // if (auto p = target.emplace_if<Array>(a, std::array<std::size_t, 2>{len, 2})) {
        //     return true;
        // }
    }
    return false;
}

/******************************************************************************/

inline std::pair<std::unique_ptr<Ref[]>, std::size_t> allocated_view(Instance<> o) {
    if (PyList_Check(+o)) {
        auto const size = PyList_GET_SIZE(+o);
        auto refs = std::make_unique<Ref[]>(size);
        for (Py_ssize_t i = 0; i !=  size; ++i)
            refs[i] = Ref(reinterpret_cast<Export&>(*PyList_GET_ITEM(+o, i)));
        return {std::move(refs), size};
    }
    if (PyTuple_Check(+o)) {
        auto const size = PyTuple_GET_SIZE(+o);
        auto refs = std::make_unique<Ref[]>(size);
        for (Py_ssize_t i = 0; i !=  size; ++i)
            refs[i] = Ref(reinterpret_cast<Export&>(*PyTuple_GET_ITEM(+o, i)));
        return {std::move(refs), size};
    }

    Shared iter(PyObject_GetIter(+o), true);
    if (!iter) {
        PyErr_Clear();
        return {};
    }
    std::vector<Ref> v;
    Shared item;
    while ((item = {PyIter_Next(+iter), true}))
        v.emplace_back(reinterpret_cast<Export&>(*(+o)));

    auto const size = v.size();
    auto refs = std::make_unique<Ref[]>(size);
    std::copy(std::make_move_iterator(v.begin()), std::make_move_iterator(v.end()), refs.get());
    return {std::move(refs), size};
}

/******************************************************************************/

inline bool dump_view(Target& target, Instance<> o) {
    auto p = allocated_view(o);
    return p.first && target.emplace_if<View>(std::move(p.first), p.second);
}

struct DeleteObject {
    void operator()(Ignore, Ignore, void* p) const noexcept {Py_DECREF(reinterpret_cast<PyObject*>(p));}
};

inline bool dump_tuple(Target& target, Instance<> o) {
    auto p = allocated_view(o);
    if (p.first) if (auto t = target.emplace_if<Tuple>(std::move(p.first), p.second, +o, Type<DeleteObject>())) {
        Py_INCREF(reinterpret_cast<PyObject*>(t->c.storage));
        return true;
    }
    return false;
}

/******************************************************************************/

inline bool dump_object(Target &target, Instance<> o) {
    DUMP("dumping object");

    if (auto v = cast_if<Variable>(+o)) {
        auto acquired = acquire_ref(*v, LockType::Read);
        return acquired.ref.load_to(target);
    }

    if (target.accepts<Str>()) {
        if (auto p = get_unicode(o)) return target.emplace_if<Str>(from_unicode(instance(p)));
        if (auto p = get_bytes(o)) return target.emplace_if<Str>(from_bytes(instance(p)));
        return false;
    }

    if (target.accepts<String>()) {
        if (auto p = get_unicode(o)) return target.emplace_if<String>(from_unicode(instance(p)));
        if (auto p = get_bytes(o)) return target.emplace_if<String>(from_bytes(instance(p)));
        return false;
    }

    if (target.accepts<Index>()) {
        if (auto p = cast_if<Index>(+o)) return target.set_if(*p);
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
        return target.emplace_if<Bool>(Bool{static_cast<bool>(PyObject_IsTrue(+o))});
    }

    return false;
}

/******************************************************************************/

}

namespace ara {

template <>
struct Dumpable<py::Export> {
    bool operator()(Target &v, py::Export const &o) const {return false;}

    bool operator()(Target &v, py::Export &o) const {
        DUMP("dumping object!");
        return py::dump_object(v, py::instance(reinterpret_cast<PyObject*>(&o)));
    }
};

template <>
struct Dumpable<py::Export*> {
    bool operator()(Target &v, py::Export* o) const {
        DUMP("dumping object pointer!", bool(o), o, reinterpret_cast<std::uintptr_t>(o));
        return py::dump_object(v, py::instance(reinterpret_cast<PyObject*>(o)));
    }
};

}