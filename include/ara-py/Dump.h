#pragma once
#include "Raw.h"

namespace ara::py {

/******************************************************************************/

inline std::string_view from_unicode(Instance<> o) {
    Py_ssize_t size;
#if PY_MAJOR_VERSION > 2
    char const *c = PyUnicode_AsUTF8AndSize(+o, &size);
#else
    char *c;
    if (PyString_AsStringAndSize(o, &c, &size)) throw PythonError();
#endif
    if (!c) throw PythonError();
    return std::string_view(static_cast<char const *>(c), size);
}

/******************************************************************************/

inline std::string_view from_bytes(Instance<> o) {
    char *c;
    Py_ssize_t size;
    PyBytes_AsStringAndSize(+o, &c, &size);
    return std::string_view(c, size);
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

/******************************************************************************/

inline bool dump_object(Target &target, Instance<> o) {
    DUMP("dumping object");

    if (auto v = cast_if<Variable>(+o)) {
        return v->as_ref().load_to(target);
    }

    if (target.accepts<std::string_view>()) {
        if (PyUnicode_Check(+o)) return target.emplace_if<std::string_view>(from_unicode(o));
        if (PyBytes_Check(+o)) return target.emplace_if<std::string_view>(from_bytes(o));
        return false;
    }

    if (target.accepts<std::string>()) {
        if (PyUnicode_Check(+o)) return target.emplace_if<std::string>(from_unicode(o));
        if (PyBytes_Check(+o)) return target.emplace_if<std::string>(from_bytes(o));
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

    if (target.accepts<bool>()) {
        if ((+o)->ob_type == Py_None->ob_type) { // fix, doesnt work with Py_None...
            return target.set_if(false);
        } else return dump_arithmetic<bool>(target, o);
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

}