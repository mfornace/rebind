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

Binary Buffer::binary(Py_buffer *view, std::size_t len) {
    if (Debug) std::cout << "make new Binary from PyBuffer" << std::endl;
    Binary bin(len, typename Binary::value_type());
    if (PyBuffer_ToContiguous(bin.data(), view, bin.size(), 'C') < 0)
        throw python_error(type_error("C++: could not make contiguous buffer"));
    return bin;
}

Variable Buffer::binary_view(Py_buffer *view, std::size_t len) {
    if (Debug) std::cout << "Contiguous " << PyBuffer_IsContiguous(view, 'C') << PyBuffer_IsContiguous(view, 'F') << std::endl;
    if (PyBuffer_IsContiguous(view, 'F')) {
        if (view->readonly) {
            if (Debug) std::cout << "read only view from PyBuffer" << std::endl;
            return BinaryView(reinterpret_cast<unsigned char const *>(view), view->len);
        } else {
            if (Debug) std::cout << "mutable view from PyBuffer" << std::endl;
            return BinaryData(reinterpret_cast<unsigned char *>(view->buf), view->len);
        }
    }
    if (Debug) std::cout << "copy view from PyBuffer" << std::endl;
    return binary(view, len);
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
    if (PyFloat_Check(o)) v = static_cast<T>(PyFloat_AsDouble(+o));
    else if (PyLong_Check(o)) v = static_cast<T>(PyLong_AsLongLong(+o));
    else if (PyBool_Check(o)) v = static_cast<T>(+o == Py_True);
}

void *Response<Object>::operator()(Qualifier q, Object o, std::type_index t) const {
    if (Debug) std::cout << "    - trying to get reference from Object " << bool(o) << std::endl;
    if (auto p = cast_if<Variable>(o)) {
        if (Debug) std::cout << "    - its a variable" << std::endl;
        if (Debug) std::cout << "    - qualifier " << p->name() << " " << static_cast<int>(p->qualifier()) << std::endl;
        if (p->type() == t) return p->ptr;
    }
    return nullptr;
}

void Response<Object>::operator()(Variable &v, Object o, std::type_index t) const {
    if (Debug) {
        Object repr{PyObject_Repr(TypeObject{(+o)->ob_type}), false};

        if (Debug) std::cout << "    - trying to convert object to " << t.name() << " " << from_unicode(+repr) << std::endl;
        if (Debug) std::cout << bool(cast_if<Variable>(o)) << std::endl;
    }

    if (auto p = cast_if<Variable>(o)) {
        if (Debug) std::cout << "    - its a variable" << std::endl;
        v = p->request(t);
    } else if (t == typeid(Object)) {
        v = std::move(o);
    } else if (t == typeid(Function)) {
        if (Debug) std::cout << "    - requested function" << std::endl;
        if (+o == Py_None) v = Function();
        else if (auto p = cast_if<Function>(o)) v = *p;
        else v = Function().emplace(PythonFunction({+o, true}, {Py_None, true}), {});
    } else if (t == typeid(Vector<Variable>)) {
        if (PyTuple_Check(o) || PyList_Check(o)) {
            if (Debug) std::cout << "    - making a tuple" << std::endl;
            Vector<Variable> vals;
            vals.reserve(PyObject_Length(o));
            map_iterable(o, [&](Object o) {vals.emplace_back(std::move(o));});
            v = std::move(vals);
        }
    } else if (t == typeid(Real)) to_arithmetic<Real>(o, v);
    else if (t == typeid(Integer)) to_arithmetic<Integer>(o, v);
    else if (t == typeid(bool)) {
        if ((+o)->ob_type == Py_None->ob_type) { // fix, doesnt work with Py_None...
            v = false;
        }
        else to_arithmetic<bool>(o, v);
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
        Buffer buff(+o, PyBUF_FULL_RO); // Read in the shape but ignore strides, suboffsets
        if (Debug) std::cout << "    - cast buffer " << std::endl;
        if (buff.ok) {
            if (Debug) std::cout << "    - making data" << std::endl;
            if (Debug) std::cout << Buffer::format(buff.view.format ? buff.view.format : "").name() << std::endl;
            if (Debug) std::cout << buff.view.ndim << std::endl;
            if (Debug) std::cout << (nullptr == buff.view.buf) << bool(buff.view.readonly) << std::endl;
            auto a = ArrayData(buff.view.buf, Buffer::format(buff.view.format ? buff.view.format : ""),
                !buff.view.readonly, Vector<Integer>(buff.view.shape, buff.view.shape + buff.view.ndim),
                Vector<Integer>()
                // Vector<Integer>(buff.view.strides, buff.view.strides + buff.view.ndim)
                );
            if (Debug) for (auto i : a.shape) std::cout << i << ", ";
            if (Debug) std::cout << std::endl;
            if (Debug) // for (auto i : a.strides) std::cout << i << ", ";
            if (Debug) // std::cout << std::endl;
            if (Debug) std::cout << "made data" << std::endl;
            if (Debug) std::cout << *static_cast<float *>(buff.view.buf) << " " << *static_cast<float *>(a.data) << std::endl;
            if (Debug) std::cout << *static_cast<std::uint16_t *>(buff.view.buf) << " " << *static_cast<std::uint16_t *>(a.data) << std::endl;
            v = std::move(a);
        } else throw python_error(type_error("C++: could not get buffer"));
    } else if (t == typeid(std::complex<double>)) {
        if (PyComplex_Check(+o)) v = std::complex<double>{PyComplex_RealAsDouble(+o), PyComplex_ImagAsDouble(+o)};
    } else if (+o == Py_None) {
        if (Debug) std::cout << "got none " << t.name() << std::endl;
    } else {
        if (Debug) std::cout << "cannot create type " << t.name() << std::endl;
    }
    if (Debug) std::cout << "requested " << t.name() << std::endl;
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
    os << ')';
    auto s = std::move(os).str();
    if (!e.indices.empty()) {s.end()[-3] = ']'; s.end()[-2] = ')'; s.pop_back();}
    return s;
}

/******************************************************************************/

Variable variable_from_object(Object o) {
    if (auto p = cast_if<Function>(o)) return {Type<Function const &>(), *p};
    else if (auto p = cast_if<std::type_index>(o)) return {Type<std::type_index>(), *p};
    else if (auto p = cast_if<Variable>(o)) return p->reference();
    else return std::move(o);
}

// Store the objects in args in pack
ArgPack args_from_python(Object const &args) {
    ArgPack v;
    v.reserve(PyObject_Length(+args));
    map_iterable(args, [&v](Object o) {v.emplace_back(variable_from_object(std::move(o)));});
    return v;
}

/******************************************************************************/

}