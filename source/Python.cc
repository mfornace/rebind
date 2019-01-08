/**
 * @brief Python-related C++ source code for cpy
 * @file Python.cc
 */
#include <cpy/PythonAPI.h>
#include <cpy/Document.h>
#include <any>
#include <iostream>

namespace cpy {

/******************************************************************************/

// Assuming a Python exception has been raised, fetch its string and put it in
// a C++ exception type. Does not clear the Python error status.
PythonError python_error(std::nullptr_t) noexcept {
    PyObject *type, *value, *traceback;
    PyErr_Fetch(&type, &value, &traceback);
    if (!type) {
        std::cout << "bad no error" << std::endl;
    }
    PyObject *str = PyObject_Str(value);
    char const *c = nullptr;
    if (str) {
#       if PY_MAJOR_VERSION > 2
            c = PyUnicode_AsUTF8(str); // PyErr_Clear
#       else
            c = PyString_AsString(str);
#       endif
        Py_DECREF(str);
    }
    PyErr_Restore(type, value, traceback);
    return PythonError(c ? c : "Python error with failed str()");
}

/******************************************************************************/

/// type_index from PyBuffer format string (excludes constness)
std::type_index Buffer::format(std::string_view s) {
    auto it = std::find_if(Buffer::formats.begin(), Buffer::formats.end(),
        [&](auto const &p) {return p.first == s;});
    return it == Buffer::formats.end() ? typeid(void) : it->second;
}

std::string_view Buffer::format(std::type_index t) {
    auto it = std::find_if(Buffer::formats.begin(), Buffer::formats.end(),
        [&](auto const &p) {return p.second == t;});
    return it == Buffer::formats.end() ? std::string_view() : it->first;
}

std::size_t Buffer::itemsize(std::type_index t) {
    auto it = std::find_if(scalars.begin(), scalars.end(),
        [&](auto const &p) {return std::get<1>(p) == t;});
    return it == scalars.end() ? 0u : std::get<2>(*it) / CHAR_BIT;
}

std::string_view from_unicode(PyObject *o) {
    Py_ssize_t size;
#if PY_MAJOR_VERSION > 2
    char const *c = PyUnicode_AsUTF8AndSize(o, &size);
#else
    char *c;
    if (PyString_AsStringAndSize(o, &c, &size)) throw python_error();
#endif
    if (!c) throw python_error();
    return std::string_view(static_cast<char const *>(c), size);
}

std::string_view from_bytes(PyObject *o) {
    char *c;
    Py_ssize_t size;
    PyBytes_AsStringAndSize(+o, &c, &size);
    return std::string_view(c, size);
}

template <class T>
void to_arithmetic(Object const &o, Variable &v) {
    DUMP("cast arithmetic", v.name(), v.qualifier());
    if (PyFloat_Check(o)) v = static_cast<T>(PyFloat_AsDouble(+o));
    else if (PyLong_Check(o)) v = static_cast<T>(PyLong_AsLongLong(+o));
    else if (PyBool_Check(o)) v = static_cast<T>(+o == Py_True);
    DUMP("cast arithmetic", v.name(), v.qualifier());
}

void Response<Object>::operator()(Variable &v, Object o, std::type_index t, Qualifier q) const {
    DUMP("trying to get reference from Object ", bool(o));
    if (auto p = cast_if<Variable>(o)) {
        Dispatch msg;
        DUMP("requested qualified variable", q, t.name(), p->type().name(), p->qualifier());
        v = p->reference().request_variable(msg, t, q);
        DUMP(p->type().name(), t.name(), v.name());
        // Dispatch msg;
        // if (p->type() == t) {
        //     DUMP("worked");
        //     v = {Type<decltype(*p)>(), *p};
        // }
    }
}

void Response<Object>::operator()(Variable &v, Object o, std::type_index t) const {
    if (Debug) {
        Object repr{PyObject_Repr(TypeObject{(+o)->ob_type}), false};

        DUMP("trying to convert object to ", t.name(), " ", from_unicode(+repr));
        DUMP(bool(cast_if<Variable>(o)));
    }

    if (auto p = cast_if<Variable>(o)) {
        DUMP("its a variable");
        Dispatch msg;
        v = p->request_variable(msg, t);
    } else if (auto p = cast_if<std::type_index>(o)) {
        v = *p;
    } else if (t == typeid(Object)) {
        v = std::move(o);
    } else if (t == typeid(Function)) {
        DUMP("requested function");
        if (+o == Py_None) v = Function();
        else if (auto p = cast_if<Function>(o)) v = *p;
        else {
            Function f;
            f.emplace(PythonFunction({+o, true}, {Py_None, true}), {});
            v = std::move(f);
        }
    } else if (t == typeid(Sequence)) {
        if (PyTuple_Check(o) || PyList_Check(o)) {
            DUMP("making a tuple");
            Sequence vals;
            vals.reserve(PyObject_Length(o));
            map_iterable(o, [&](Object o) {vals.emplace_back(std::move(o));});
            v = std::move(vals);
        }
    } else if (t == typeid(Real)) to_arithmetic<Real>(o, v);
    else if (t == typeid(Integer)) to_arithmetic<Integer>(o, v);
    else if (t == typeid(bool)) {
        if ((+o)->ob_type == Py_None->ob_type) { // fix, doesnt work with Py_None...
            v = false;
        } else to_arithmetic<bool>(o, v);
        // if (!v) {
        //     throw python_error(type_error("what %R %R", +o, (+o)->ob_type));
        // }
    }
    else if (t == typeid(std::string_view)) {
        if (PyUnicode_Check(+o)) v = from_unicode(+o);
        if (PyBytes_Check(+o)) v = from_bytes(+o);
    } else if (t == typeid(std::string)) {
        if (PyUnicode_Check(+o)) v = std::string(from_unicode(+o));
        if (PyBytes_Check(+o)) v = std::string(from_bytes(+o));
    } else if (t == typeid(ArrayData)) {
        if (PyObject_CheckBuffer(+o)) {
            Buffer buff(o, PyBUF_FULL_RO); // Read in the shape but ignore strides, suboffsets
            DUMP("cast buffer", buff.ok);
            if (buff.ok) {
                DUMP("making data");
                DUMP(Buffer::format(buff.view.format ? buff.view.format : "").name());
                DUMP("ndim", buff.view.ndim);
                DUMP((nullptr == buff.view.buf), bool(buff.view.readonly));
                auto a = ArrayData(buff.view.buf, Buffer::format(buff.view.format ? buff.view.format : ""),
                    !buff.view.readonly, Vector<Integer>(buff.view.shape, buff.view.shape + buff.view.ndim),
                    Vector<Integer>(buff.view.strides, buff.view.strides + buff.view.ndim));
                DUMP("itemsize", buff.view.itemsize);
                for (auto i : a.strides) DUMP(i);
                for (auto &x : a.strides) x /= buff.view.itemsize;
                for (auto i : a.strides) DUMP(i);
                for (auto i : a.shape) DUMP(i);
                DUMP(*static_cast<float *>(buff.view.buf), " ", *static_cast<float *>(a.data));
                DUMP(*static_cast<std::uint16_t *>(buff.view.buf), " ", *static_cast<std::uint16_t *>(a.data));
                DUMP("made data! ", a.strides.size());
                v = std::move(a);
            } else throw python_error(type_error("C++: could not get buffer"));
        }
    } else if (t == typeid(std::complex<double>)) {
        if (PyComplex_Check(+o)) v = std::complex<double>{PyComplex_RealAsDouble(+o), PyComplex_ImagAsDouble(+o)};
    } else if (+o == Py_None) {
        DUMP("got none ", t.name());
    } else {
        DUMP("cannot create type ", t.name());
    }
    DUMP("requested ", v.name(), t.name());
}

/******************************************************************************/

std::string_view get_type_name(std::type_index idx) noexcept {
    auto it = type_names.find(idx);
    if (it == type_names.end() || it->second.empty()) return idx.name();
    else return it->second;
}

std::string wrong_type_message(WrongType const &e) {
    std::ostringstream os;
    os << "C++: " << e.what() << " (#" << e.index << ", "
        << get_type_name(e.source) << " \u2192 " << get_type_name(e.dest);
    if (!e.indices.empty()) {
        os << ", scopes=[";
        for (auto i : e.indices) os << i << ", ";
    };
    os << " expected=" << e.expected << " received=" << e.received;
    os << ')';
    auto s = std::move(os).str();
    if (!e.indices.empty()) {s.end()[-3] = ']'; s.end()[-2] = ')'; s.pop_back();}
    return s;
}

/******************************************************************************/

Variable variable_from_object(Object o) {
    if (auto p = cast_if<Function>(o)) return {Type<Function const &>(), *p};
    else if (auto p = cast_if<std::type_index>(o)) return {Type<std::type_index>(), *p};
    else if (auto p = cast_if<Variable>(o)) {
        DUMP(p, p->data());
        DUMP(p->qualifier(), p->reference().qualifier());
        DUMP(p->qualifier(), p->reference().qualifier());
        return p->reference();
    }
    else return std::move(o);
}

// Store the objects in args in pack
Sequence args_from_python(Object const &args) {
    Sequence v;
    v.reserve(PyObject_Length(+args));
    map_iterable(args, [&v](Object o) {v.emplace_back(variable_from_object(std::move(o)));});
    return v;
}

/******************************************************************************/

}
