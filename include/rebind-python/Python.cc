/**
 * @brief Python-related C++ source code for rebind
 * @file Python.cc
 */
#include "Common.h"
#include "Variable.h"
#include <rebind-cpp/Core.h>

#include <complex>
#include <any>
#include <iostream>

namespace rebind::py {

/******************************************************************************/

std::pair<Object, char const *> str(PyObject *o) {
    std::pair<Object, char const *> out({PyObject_Str(o), false}, nullptr);
    if (out.first) {
#       if PY_MAJOR_VERSION > 2
            out.second = PyUnicode_AsUTF8(out.first); // PyErr_Clear
#       else
            out.second = PyString_AsString(out.first);
#       endif
    }
    return out;
}

std::string repr(PyObject *o) {
    if (!o) return "null";
    Object out(PyObject_Repr(o), false);
    if (!out) throw python_error();
#   if PY_MAJOR_VERSION > 2
        return PyUnicode_AsUTF8(out); // PyErr_Clear
#   else
        return PyString_AsString(out);
#   endif
}

void print(PyObject *o) {
    auto p = str(o);
    if (p.second) std::cout << p.second << std::endl;
}

// Assuming a Python exception has been raised, fetch its string and put it in
// a C++ exception type. Does not clear the Python error status.
PythonError python_error(std::nullptr_t) noexcept {
    PyObject *type, *value, *traceback;
    PyErr_Fetch(&type, &value, &traceback);
    if (!type) return PythonError("Expected Python exception to be set");
    auto p = str(value);
    PyErr_Restore(type, value, traceback);
    return PythonError(p.second ? p.second : "Python error with failed str()");
}

/******************************************************************************/

/// type_index from PyBuffer format string (excludes constness)
Index Buffer::format(std::string_view s) {
    auto it = std::find_if(Buffer::formats.begin(), Buffer::formats.end(),
        [&](auto const &p) {return p.first == s;});
    return it == Buffer::formats.end() ? Index() : it->second;
}

std::string_view Buffer::format(Index i) {
    auto it = std::find_if(Buffer::formats.begin(), Buffer::formats.end(),
        [i](auto const &p) {return p.second == i;});
    return it == Buffer::formats.end() ? std::string_view() : it->first;
}

std::size_t Buffer::itemsize(Index i) {
    auto it = std::find_if(scalars.begin(), scalars.end(),
        [i](auto const &p) {return std::get<1>(p) == i;});
    return it == scalars.end() ? 0u : std::get<2>(*it) / CHAR_BIT;
}

/******************************************************************************/

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

/******************************************************************************/

template <class T>
bool arithmetic_to_value(Target &v, Object const &o) {
    DUMP("cast arithmetic in: ", v.name());
    if (PyFloat_Check(o)) return v.emplace_if<T>(PyFloat_AsDouble(+o));
    if (PyLong_Check(o))  return v.emplace_if<T>(PyLong_AsLongLong(+o));
    if (PyBool_Check(o))  return v.emplace_if<T>(+o == Py_True);
    if (PyNumber_Check(+o)) { // This can be hit for e.g. numpy.int64
        if (std::is_integral_v<T>) {
            if (auto i = Object::from(PyNumber_Long(+o)))
                return v.emplace_if<T>(PyLong_AsLongLong(+i));
        } else {
            if (auto i = Object::from(PyNumber_Float(+o)))
               return v.emplace_if<T>(PyFloat_AsDouble(+i));
        }
    }
    DUMP("cast arithmetic out: ", v.name());
    return false;
}

/******************************************************************************/

bool dump_object(Target &v, Object o) {
    if (Debug) {
        auto repr = Object::from(PyObject_Repr(SubClass<PyTypeObject>{(+o)->ob_type}));
        DUMP("input object reference count = ", reference_count(o));
        DUMP("trying to convert object to ", v.name(), " ", from_unicode(+repr));
        DUMP("is Value = ", bool(cast_if<Value>(o)));
    }
    if (!o) return false;

#warning "need to finish ToValue<py::Object>?"

    Object type = Object(reinterpret_cast<PyObject *>((+o)->ob_type), true);

    if (auto p = input_conversions.find(type); p != input_conversions.end()) {
        Object guard(+o, false); // PyObject_CallFunctionObjArgs increments reference
        o = Object::from(PyObject_CallFunctionObjArgs(+p->second, +o, nullptr));
        type = Object(reinterpret_cast<PyObject *>((+o)->ob_type), true);
    }

    if (auto p = cast_if<Value>(o)) {
        DUMP("object_to_value Value");
        return p->load_to(v);
    }

    if (v.accepts<Index>()) {
        if (auto p = cast_if<Index>(o)) return v.set_if(*p);
        else return false;
    }

    if (v.accepts<std::nullptr_t>()) {
        DUMP("nullptr....");
#warning "not sure"
        if (+o == Py_None) return true;
    }

    // if (v.accepts<Overload>()) {
    //     DUMP("requested function");
    //     if (+o == Py_None) return v.emplace_if<Overload>();
    //     else if (auto p = cast_if<Overload>(o)) v.set_if(*p);
    //     // general python function has no signature associated with it right now.
    //     // we could get them out via function.__annotations__ and process them into a tuple
    //     else v.set_if(Overload(PythonFunction({+o, true}, {Py_None, true}), {}));
    //     return true;
    // }

    if (v.accepts<Sequence>()) {
        if (PyTuple_Check(o) || PyList_Check(o)) {
            DUMP("making a Sequence");
            if (auto s = v.emplace_if<Sequence>()) {
                s->reserve(PyObject_Length(o));
                map_iterable(o, [&](Object o) {s->emplace_back(std::move(o));});
            }
            return true;
        }
        return false;
    }

    if (v.accepts<Real>())
        return arithmetic_to_value<Real>(v, o);

    if (v.accepts<Integer>())
        return arithmetic_to_value<Integer>(v, o);

    if (v.accepts<bool>()) {
        if ((+o)->ob_type == Py_None->ob_type) { // fix, doesnt work with Py_None...
            return v.set_if(false);
        } else return arithmetic_to_value<bool>(v, o);
    }

    if (v.accepts<std::string_view>()) {
        if (PyUnicode_Check(+o)) return v.emplace_if<std::string_view>(from_unicode(+o));
        if (PyBytes_Check(+o)) return v.emplace_if<std::string_view>(from_bytes(+o));
        return false;
    }

    if (v.accepts<std::string>()) {
        if (PyUnicode_Check(+o)) return v.emplace_if<std::string>(from_unicode(+o));
        if (PyBytes_Check(+o)) return v.emplace_if<std::string>(from_bytes(+o));
        return false;
    }

    if (v.accepts<ArrayView>()) {
        if (PyObject_CheckBuffer(+o)) {
            // Read in the shape but ignore strides, suboffsets
            DUMP("cast buffer", reference_count(o));
            if (auto buff = Buffer(o, PyBUF_FULL_RO)) {
                DUMP("making data", reference_count(o));
                DUMP(Buffer::format(buff.view.format ? buff.view.format : ""));
                DUMP("ndim", buff.view.ndim);
                DUMP((nullptr == buff.view.buf), bool(buff.view.readonly));
                for (auto i = 0; i != buff.view.ndim; ++i) DUMP(i, buff.view.shape[i], buff.view.strides[i]);
                DUMP("itemsize", buff.view.itemsize);
                ArrayLayout lay;
                lay.contents.reserve(buff.view.ndim);
                for (std::size_t i = 0; i != buff.view.ndim; ++i)
                    lay.contents.emplace_back(buff.view.shape[i], buff.view.strides[i] / buff.view.itemsize);
                DUMP("layout", lay, reference_count(o));
                DUMP("depth", lay.depth());
#warning "fix"
                // Ref data{buff.view.format ? Buffer::format(buff.view.format) : fetch<void>(), buff.view.buf, buff.view.readonly ? Const : Lvalue};
                // return v.emplace_if<ArrayView>(std::move(data), std::move(lay));
            } else throw python_error(type_error("C++: could not get buffer from Python obhect"));
        } else return false;
    }

    if (v.accepts<std::complex<double>>()) {
        if (PyComplex_Check(+o)) return v.emplace_if<std::complex<double>>(PyComplex_RealAsDouble(+o), PyComplex_ImagAsDouble(+o));
        return false;
    }

    DUMP("request failed for py::Object to ", v.name());

    // if (!ok) { // put diagnostic for the source type
    //     auto o = py::Object::from(PyObject_Repr(+type));
    //     DUMP("setting object error description", from_unicode(o));
    //     v = {Type<std::string>(), from_unicode(o)};
    // }

    return false;
}

/******************************************************************************/

bool object_to_ref(Ref &v, Object o) {
    return false;
}

/******************************************************************************/

std::string_view get_type_name(Index idx) noexcept {return idx.name();}

/******************************************************************************/

std::string wrong_type_message(WrongType const &e, std::string_view prefix) {
    std::ostringstream os;
    os << prefix << e.what() << " (#" << e.index << ", ";
    if (!e.source.empty())
        os << e.source << " \u2192 " << get_type_name(e.dest) << ", ";
    if (!e.indices.empty()) {
        auto it = e.indices.begin();
        os << "scopes=[" << *it;
        while (++it != e.indices.end()) os << ", " << *it;
        os << "], ";
    }
    if (e.expected != -1)
        os << "expected=" << e.expected << " received=" << e.received << ", ";
    std::string s = os.str();
    s.pop_back();
    s.back() = ')';
    return s;
}

/******************************************************************************/

Ref ref_from_object(Object &o, bool move) {
    if (auto p = cast_if<Index>(o)) {
        DUMP("ref_from_object: Index = ", p->name());
        return Ref(*p);
    } else if (auto p = cast_if<Variable>(o)) {
        DUMP("ref_from_object: Value = ", p->name());
        return Ref();//(p->index(), p->address(), move ? Rvalue : Lvalue);
    // } else if (auto p = cast_if<Ref>(o)) {
    //     DUMP("ref_from_object: Ref = ", p->name());
    //     return *p;
    } else {
        return Ref(o);
    }
}

/******************************************************************************/

}
