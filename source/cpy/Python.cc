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
PythonError python_error() noexcept {
    PyObject *type, *value, *traceback;
    PyErr_Fetch(&type, &value, &traceback);
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

std::type_index Buffer::format(std::string_view s) {
    auto it = std::find_if(Buffer::formats.begin(), Buffer::formats.end(),
        [&](auto const &p) {return p.first == s;});
    return it == Buffer::formats.end() ? typeid(void) : it->second;
}

Binary Buffer::binary(Py_buffer *view, std::size_t len) {
    std::cout << "make new Binary from PyBuffer" << std::endl;
    Binary bin(len, typename Binary::value_type());
    if (PyBuffer_ToContiguous(bin.data(), view, bin.size(), 'C') < 0) {
        PyErr_SetString(PyExc_TypeError, "C++: could not make contiguous buffer");
        throw python_error();
    }
    return bin;
}

Value Buffer::binary_view(Py_buffer *view, std::size_t len) {
    std::cout << "Contiguous " << PyBuffer_IsContiguous(view, 'C') << PyBuffer_IsContiguous(view, 'F') << std::endl;
    if (PyBuffer_IsContiguous(view, 'F')) {
        if (view->readonly) {
            std::cout << "read only view from PyBuffer" << std::endl;
            return BinaryView(reinterpret_cast<unsigned char const *>(view), view->len);
        } else {
            std::cout << "mutable view from PyBuffer" << std::endl;
            return BinaryData(reinterpret_cast<unsigned char *>(view->buf), view->len);
        }
    }
    std::cout << "copy view from PyBuffer" << std::endl;
    return binary(view, len);
}

std::string_view as_string_view(PyObject *o) {
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

template <bool A>
Value from_python(Object const &o) {
    if (+o == Py_None) return {};

    if (PyBool_Check(+o)) return (+o == Py_True) ? true : false;

    if (PyLong_Check(+o)) return static_cast<Integer>(PyLong_AsLongLong(+o));

    if (PyFloat_Check(+o)) return static_cast<Real>(PyFloat_AsDouble(+o));

    if (PyTuple_Check(+o) || PyList_Check(+o)) {
        Vector<Value> vals;
        vals.reserve(PyObject_Length(+o));
        map_iterable(o, [&](Object o) {
            vals.emplace_back(from_python<A>(o));
        });
        return std::move(vals);
    }

    if (PyBytes_Check(+o)) {
        char *c;
        Py_ssize_t size;
        PyBytes_AsStringAndSize(+o, &c, &size);
        if (A) return std::string_view(c, size);
        else return std::string(c, size);
    }

    if (PyUnicode_Check(+o)) {
        auto v = as_string_view(+o);
        if (A) return v;
        else return std::string(v);
    }

    if (PyObject_TypeCheck(+o, &ValueType)) {
        if constexpr(A) return Reference(cast_object<Value>(+o));
        return cast_object<Value>(+o);
    }

    if (PyObject_TypeCheck(+o, &TypeIndexType)) return cast_object<std::type_index>(+o);

    if (PyObject_TypeCheck(+o, &FunctionType)) return cast_object<Function>(+o);

    if (PyComplex_Check(+o))
        return std::complex<double>{PyComplex_RealAsDouble(+o), PyComplex_ImagAsDouble(+o)};

    if (PyByteArray_Check(+o)) {
        char *data = PyByteArray_AS_STRING(+o);
        auto const len = PyByteArray_GET_SIZE(+o);
        if (Debug) std::cout << "binary from bytearray " << A << std::endl;
        if (A) return BinaryData(reinterpret_cast<unsigned char *>(data), len);
        else return Binary(data, data + len); // ignore null byte at end
    }

    if (PyObject_CheckBuffer(+o)) {
        // Buffer buff(+o, PyBUF_FULL_RO); // Read in the shape but ignore strides, suboffsets
        // if (!buff.ok) {
        //     PyErr_SetString(PyExc_TypeError, "C++: could not get buffer");
        //     throw python_error();
        // }
        // std::conditional_t<A, Value, Value> bin;
        // if (Debug) std::cout << "cast buffer " << A << std::endl;
        // if constexpr(A) bin = Buffer::binary_view(&buff.view, buff.view.len);
        // else bin = Buffer::binary(&buff.view, buff.view.len);
        // return std::conditional_t<A, ArgPack, ValuePack>::from_values(
        //     std::move(bin), Buffer::format(buff.view.format ? buff.view.format : ""),
        //     Vector<Integer>(buff.view.shape, buff.view.shape + buff.view.ndim));
    }

    PyErr_Format(PyExc_TypeError, "C++: object of type %R cannot be converted to a Value", (+o)->ob_type);
    throw python_error();
};

Value ToValue<PyObject>::operator()(PyObject const &ob, TypeRequest const &t) {
    Object o(const_cast<PyObject *>(&ob), true);

    std::cout << "trying to convert object" << std::endl;
    for (auto const &x : t) {
        std::cout << x.name() << std::endl;
    }

    if (+o == Py_None) return {};

    for (auto const &idx : t) {
        if (idx == typeid(Vector<Reference>)) {
            if (PyTuple_Check(+o) || PyList_Check(+o)) {
                std::cout << "making a tuple" << std::endl;
                Vector<Reference> refs;
                refs.reserve(PyObject_Length(+o));
                map_iterable(o, [&](Object o) {
                    refs.emplace_back(*(+o));
                });
                return std::move(refs);
            }
        } else if (idx == typeid(Real)) {
            if (PyFloat_Check(+o)) return static_cast<Real>(PyFloat_AsDouble(+o));
        } else if (idx == typeid(Integer)) {
            std::cout << "casting to int" << std::endl;
            // if (PyBool_Check(+o)) return (+o == Py_True) ? true : false;
            if (PyLong_Check(+o)) {
                std::cout << "casting to int2" << std::endl;
                return static_cast<Integer>(PyLong_AsLongLong(+o));
            }
            // if (PyFloat_Check(+o)) return static_cast<Real>(PyFloat_AsDouble(+o));
        } else if (idx == typeid(bool)) {
            if (PyBool_Check(+o)) return (+o == Py_True) ? true : false;
        } else if (idx == typeid(std::string_view)) {
            if (PyUnicode_Check(+o)) return as_string_view(+o);
        } else if (idx == typeid(std::string)) {
            if (PyUnicode_Check(+o)) return std::string(as_string_view(+o));
        }
        std::cout << "requested " << idx.name() << std::endl;
    }
    std::cout << "what is this" << std::endl;
    throw std::runtime_error("not known");
    return from_python<true>(std::move(o));
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

// Store the objects in args in pack
ArgPack positional_args(Object const &args) {
    ArgPack v;
    v.reserve(PyObject_Length(+args));
    map_iterable(args, [&v](Object o) {
        if (PyObject_TypeCheck(+o, &FunctionType))
            v.emplace_back(cast_object<Function>(+o));
        else if (PyObject_TypeCheck(+o, &TypeIndexType))
            v.emplace_back(cast_object<std::type_index>(+o));
        else if (PyObject_TypeCheck(+o, &ValueType))
            v.emplace_back(cast_object<Value>(+o));
        else v.emplace_back(*(+o));
    });
    return v;
}

/******************************************************************************/

}