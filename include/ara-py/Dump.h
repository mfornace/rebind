#pragma once
#include "Raw.h"
#include <ara/Core.h>

namespace ara::py {

bool dump_object(Target &target, Always<> o);

/******************************************************************************/

template <class T>
bool dump_arithmetic(Target &target, Always<> o) {
    DUMP("cast arithmetic in:", target.name());
    if (PyFloat_Check(+o)) return target.emplace<T>(PyFloat_AsDouble(+o));
    if (PyLong_Check(+o))  return target.emplace<T>(PyLong_AsLongLong(+o));
    if (PyBool_Check(+o))  return target.emplace<T>(+o == Py_True);
    if (PyNumber_Check(+o)) { // This can be hit for e.g. numpy.int64
        if (std::is_integral_v<T>) {
            if (auto i = Value<pyInt>::take(PyNumber_Long(+o)))
                return target.emplace<T>(PyLong_AsLongLong(+i));
        } else {
            if (auto i = Value<pyFloat>::take(PyNumber_Float(+o)))
               return target.emplace<T>(PyFloat_AsDouble(+i));
        }
    }
    DUMP("cast arithmetic out:", target.name());
    return false;
}

/******************************************************************************/

struct ObjectGuard {
    void operator()(PyObject* object) {
        DUMP("what happening", object);
        DUMP("what happening", reference_count(Ptr<>{object}));
        DUMP("decrementing object", Ptr<>{object});
        Py_DECREF(object);}

    static auto make_unique(Always<> o) noexcept {
        DUMP("make_unique", +o, reference_count(o));
        Py_INCREF(+o);
        DUMP("make_unique", +o, reference_count(o));
        return std::unique_ptr<PyObject, ObjectGuard>(+o);
    }
};

/******************************************************************************/

}

/******************************************************************************/

namespace ara {

/******************************************************************************/

template <>
struct Impl<py::Export> : Default<py::Export> {
    static bool dump(Target &v, py::Export &o) {
        DUMP("dumping object!");
        return py::dump_object(v, reinterpret_cast<PyObject&>(o));
    }

    static bool dump(Target &v, py::Export const &o) {
        return dump(v, const_cast<py::Export&>(o));
    }
};

template <>
struct Impl<py::Export*> : Default<py::Export*> {
    static bool dump(Target &v, py::Export* o) {
        DUMP("dumping object pointer!", bool(o), o, reinterpret_cast<std::uintptr_t>(o));
        return py::dump_object(v, *reinterpret_cast<PyObject*>(o));
    }
};

/******************************************************************************/

}

/******************************************************************************/