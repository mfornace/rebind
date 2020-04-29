#pragma once
#include "Raw.h"

namespace ara::py {

/******************************************************************************/

inline std::string_view from_unicode(Ptr o) {
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

inline std::string_view from_bytes(Ptr o) {
    char *c;
    Py_ssize_t size;
    PyBytes_AsStringAndSize(+o, &c, &size);
    return std::string_view(c, size);
}

/******************************************************************************/

inline bool dump_object(Target &target, Ptr o) {
    DUMP("dumping object");
    if (target.accepts<std::string_view>()) {
        if (PyUnicode_Check(+o)) return target.emplace_if<std::string_view>(from_unicode(o));
        if (PyBytes_Check(+o)) return target.emplace_if<std::string_view>(from_bytes(o));
        return false;
    }
    return false;
}

/******************************************************************************/

}

namespace ara {

template <>
struct Dumpable<py::Ptr> {
    bool operator()(Target &v, py::Ptr o) const {
        DUMP("dumping object!");
        return py::dump_object(v, o);
    }
};

}