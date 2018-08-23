/**
 * @brief Python-related C++ source code for cpy
 * @file Python.cc
 */
#include <cpy/PythonAPI.h>
#include <cpy/Document.h>
#include <any>
#include <iostream>

#ifndef CPY_MODULE
#   define CPY_MODULE libcpy
#endif

namespace cpy {

std::unordered_map<std::type_index, std::string_view> type_names = {
    {typeid(void), "void"},
    {typeid(std::monostate), "void"},
    {typeid(bool), "bool"},
    {typeid(Integer), "int"},
    {typeid(Real), "float"},
    {typeid(std::string_view), "str"},
    {typeid(std::string), "str"},
    {typeid(std::type_index), "TypeIndex"},
    {typeid(Binary), "Binary"},
    {typeid(BinaryView), "BinaryView"},
    {typeid(Function), "Function"},
    {typeid(Any), "Any"},
    {typeid(Sequence), "Sequence"},
};

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

Zip<std::string_view, std::type_index> Buffer::formats = {
    {"d", typeid(double)},
    {"f", typeid(float)},
    {"c", typeid(char)},
    {"b", typeid(signed char)},
    {"B", typeid(unsigned char)},
    {"?", typeid(bool)},
    {"h", typeid(short)},
    {"H", typeid(unsigned short)},
    {"i", typeid(int)},
    {"I", typeid(unsigned int)},
    {"l", typeid(long)},
    {"L", typeid(unsigned long)},
    {"q", typeid(long long)},
    {"Q", typeid(unsigned long long)},
    {"n", typeid(ssize_t)},
    {"s", typeid(char[])},
    {"p", typeid(char[])},
    {"N", typeid(size_t)},
    {"P", typeid(void *)}
};

std::type_index Buffer::format(std::string_view s) {
    auto it = std::find_if(Buffer::formats.begin(), Buffer::formats.end(),
        [&](auto const &p) {return p.first == s;});
    return it == Buffer::formats.end() ? typeid(void) : it->second;
}

Binary Buffer::binary(Py_buffer *view, std::size_t len) {
    Binary bin(len, typename Binary::value_type());
    if (PyBuffer_ToContiguous(bin.data(), view, bin.size(), 'C') < 0) {
        PyErr_SetString(PyExc_TypeError, "C++: could not make contiguous buffer");
        throw python_error();
    }
    return bin;
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

Value from_python(Object const &o, bool view) {
    if (+o == Py_None) return std::monostate();

    if (PyBool_Check(+o)) return (+o == Py_True) ? true : false;

    if (PyLong_Check(+o)) return static_cast<Integer>(PyLong_AsLongLong(+o));

    if (PyFloat_Check(+o)) return static_cast<Real>(PyFloat_AsDouble(+o));

    if (PyTuple_Check(+o) || PyList_Check(+o)) {
        Vector<Value> vals;
        vals.reserve(PyObject_Length(+o));
        map_iterable(o, [&](Object o) {
            vals.emplace_back(from_python(o, view));
        });
        return Sequence(vals);
    }

    if (PyBytes_Check(+o)) {
        char *c;
        Py_ssize_t size;
        PyBytes_AsStringAndSize(+o, &c, &size);
        if (view) return std::string_view(c, size);
        else return std::string(c, size);
    }

    if (PyUnicode_Check(+o)) {
        auto v = as_string_view(+o);
        if (view) return v;
        else return std::string(v);
    }

    if (PyObject_TypeCheck(+o, &AnyType)) {
        if (view) return Any(AnyReference(cast_object<Any>(+o)));
        else return cast_object<Any>(+o);
    }

    if (PyObject_TypeCheck(+o, &TypeIndexType)) return cast_object<std::type_index>(+o);

    if (PyObject_TypeCheck(+o, &FunctionType)) return cast_object<Function>(+o);

    if (PyComplex_Check(+o))
        return {std::in_place_t(), std::complex<double>{PyComplex_RealAsDouble(+o), PyComplex_ImagAsDouble(+o)}};

    if (PyByteArray_Check(+o)) {
        char const *data = PyByteArray_AS_STRING(+o);
        auto const len = PyByteArray_GET_SIZE(+o) - 1;
        if (view) return BinaryView(reinterpret_cast<unsigned char const *>(data), len);
        else return Binary(data, data + len); // ignore null byte at end
    }

    if (PyObject_CheckBuffer(+o)) {
        Buffer buff(+o, PyBUF_FULL_RO); // Read in the shape but ignore strides, suboffsets
        if (!buff.ok) {
            PyErr_SetString(PyExc_TypeError, "C++: could not get buffer");
            throw python_error();
        }
        return Sequence::vector(
            Buffer::binary(&buff.view, buff.view.len),
            Buffer::format(buff.view.format ? buff.view.format : ""),
            SmallVec<Integer>(buff.view.shape, buff.view.shape + buff.view.ndim));
    }

    PyErr_Format(PyExc_TypeError, "C++: Object of type %R cannot be converted to a Value", (+o)->ob_type);
    throw python_error();
};

/******************************************************************************/

// Store the objects in args in pack
ArgPack positional_args(Object const &args) {
    ArgPack pack;
    pack.reserve(PyObject_Length(+args));
    map_iterable(std::move(args), [&pack](Object o) {
        pack.emplace_back(from_python(o, true));
    });
    return pack;
}

/******************************************************************************/

PyObject *one_argument(PyObject *args, PyTypeObject *type=nullptr) noexcept {
    Py_ssize_t n = PyTuple_Size(args);
    if (n != 1) {
        PyErr_Format(PyExc_TypeError, "Expected single argument but got %zd", n);
        return nullptr;
    }
    PyObject *value = PyTuple_GET_ITEM(args, 0);
    if (type && !PyObject_TypeCheck(value, type)) { // call repr
        PyErr_Format(PyExc_TypeError, "Invalid argument type for C++: %R", value->ob_type);
        return nullptr;
    }
    return value;
}

Object raised(PyObject *exc, char const *message) noexcept {
    PyErr_SetString(exc, message);
    return {};
}

template <class T>
PyObject *tp_new(PyTypeObject *subtype, PyObject *, PyObject *) noexcept {
    PyObject *o = subtype->tp_alloc(subtype, 0); // 0 unused
    if (!o) return nullptr;
    static_assert(noexcept(T{}), "Default constructor should be noexcept");
    new (&cast_object<T>(o)) T;
    return o;
}

template <class T>
void tp_delete(PyObject *o) noexcept {
    reinterpret_cast<Holder<T> *>(o)->~Holder<T>();
    Py_TYPE(o)->tp_free(o);
}

template <class T>
PyObject * move_from(PyObject *self, PyObject *args) noexcept {
    PyObject *value = one_argument(args);
    if (!value) return nullptr;
    return raw_object([=] {
        cast_object<T>(self) = std::move(cast_object<T>(value));
        return Object(self, true);
    });
}

/******************************************************************************/

PyObject *type_index_new(PyTypeObject *subtype, PyObject *, PyObject *) noexcept {
    PyObject* o = subtype->tp_alloc(subtype, 0); // 0 unused
    new (&cast_object<std::type_index>(o)) std::type_index(typeid(void)); // noexcept
    return o;
}

long type_index_hash(PyObject *o) noexcept {
    return static_cast<long>(cast_object<std::type_index>(o).hash_code());
}

PyObject *type_index_name(PyObject *o) noexcept {
    std::type_index const *p = cast_if<std::type_index>(o);
    if (p) {
        auto it = type_names.find(*p);
        return PyUnicode_FromFormat("TypeIndex('%s')", it == type_names.end() ? p->name() : it->second.data());
    }
    PyErr_SetString(PyExc_TypeError, "Expected instance of cpy.TypeIndex");
    return nullptr;
}

PyObject *type_index_compare(PyObject *self, PyObject *other, int op) {
    return raw_object([=] {
        auto const &s = cast_object<std::type_index>(self);
        auto const &o = cast_object<std::type_index>(other);
        bool out;
        switch(op) {
            case(Py_LT): out = s < o;
            case(Py_GT): out = s > o;
            case(Py_LE): out = s <= o;
            case(Py_EQ): out = s == o;
            case(Py_NE): out = s != o;
            case(Py_GE): out = s >= o;
        }
        return to_python(out);
    });
}

PyTypeObject TypeIndexType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cpy.TypeIndex",
    .tp_hash = type_index_hash,
    .tp_str = type_index_name,
    .tp_repr = type_index_name,
    .tp_richcompare = type_index_compare,
    .tp_basicsize = sizeof(Holder<std::type_index>),
    .tp_dealloc = tp_delete<std::type_index>,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = "C++ type_index object",
    .tp_new = type_index_new,
};

/******************************************************************************/

template <class T>
PyObject * copy_from(PyObject *self, PyObject *args) noexcept {
    PyObject *value = one_argument(args, &type_ref(Type<T>()));
    if (!value) return nullptr;
    return raw_object([=] {
        cast_object<T>(self) = cast_object<T>(value); // not notexcept
        return Object(Py_None, true);
    });
}

int has_value_bool(PyObject *self) noexcept {
    auto t = cast_if<Any>(self);
    return t ? t->has_value() : PyObject_IsTrue(self);
}

PyObject * any_has_value(PyObject *self, PyObject *) noexcept {
    return raw_object([=] {return to_python(cast_object<Any>(self).has_value());});
}

PyObject * any_type_index(PyObject *self, PyObject *) noexcept {
    return raw_object([=] {
        Object o{PyObject_CallObject(type_object(TypeIndexType), nullptr), false};
        cast_object<std::type_index>(+o) = cast_object<Any>(self).type();
        return o;
    });
}

PyNumberMethods AnyNumberMethods = {
    .nb_bool = static_cast<inquiry>(has_value_bool),
};

PyMethodDef AnyTypeMethods[] = {
    {"type",      static_cast<PyCFunction>(any_type_index), METH_NOARGS, "index it"},
    {"move_from", static_cast<PyCFunction>(move_from<Any>), METH_VARARGS, "move it"},
    {"copy_from", static_cast<PyCFunction>(copy_from<Any>), METH_VARARGS, "copy it"},
    {"has_value", static_cast<PyCFunction>(any_has_value), METH_VARARGS, "has value"},
    {nullptr, nullptr, 0, nullptr}
};

PyTypeObject AnyType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cpy.Any",
    .tp_as_number = &AnyNumberMethods,
    .tp_basicsize = sizeof(Holder<Any>),
    .tp_dealloc = tp_delete<Any>,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = "C++ class object",
    .tp_new = tp_new<Any>,
    .tp_methods = AnyTypeMethods
};

/******************************************************************************/

PyObject * function_call(PyObject *self, PyObject *args, PyObject *kws) noexcept {
    bool gil = true;
    if (kws && PyDict_Check(kws)) {
        PyObject *g = PyDict_GetItemString(kws, "gil");
        if (g) gil = PyObject_IsTrue(g);
        if (bool(g) != PyObject_Length(kws)) {
            PyErr_SetString(PyExc_ValueError, "Invalid keyword");
            return nullptr;
        }
    }
    std::cout << "gil = " << gil << " " << Py_REFCNT(self) << Py_REFCNT(args) << std::endl;

    return cpy::raw_object([=]() -> Object {
        auto &fun = cast_object<Function>(self);
        auto py = fun.target<PythonFunction>();
        if (py) return {PyObject_CallObject(+py->function, args), false};
        if (!fun) return raised(PyExc_ValueError, "Invalid C++ function");
        auto pack = positional_args({args, true});
        Value out;
        {
            ReleaseGIL lk(!gil);
            CallingContext ct{&lk};
            out = fun(ct, std::move(pack));
        }
        return cpy::to_python(std::move(out));
    });
}

PyMethodDef FunctionTypeMethods[] = {
    // {"index",     static_cast<PyCFunction>(type_method<Function>), METH_NOARGS, "index it"},
    {"move_from", static_cast<PyCFunction>(move_from<Function>),   METH_VARARGS, "move it"},
    {"copy_from", static_cast<PyCFunction>(copy_from<Function>),   METH_VARARGS, "copy it"},
    // {"has_value", static_cast<PyCFunction>(has_value<Function>),   METH_VARARGS, "has value"},
    {nullptr, nullptr, 0, nullptr}
};

int function_init(PyObject *self, PyObject *args, PyObject *kws) noexcept {
    static char const * keys[] = {"function", nullptr};
    PyObject *fun = nullptr;
    if (!PyArg_ParseTupleAndKeywords(args, kws, "|O", const_cast<char **>(keys), &fun))
        return -1;
    if (!fun) return 0;
    if (!PyCallable_Check(fun)) {
        PyErr_Format(PyExc_TypeError, "Expected callable type but got: %R", fun->ob_type);
        return -1;
    }
    cast_object<Function>(self) = PythonFunction{Object(fun, true)};
    return 0;
}

PyTypeObject FunctionType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cpy.Function",
    .tp_basicsize = sizeof(Holder<Function>),
    .tp_dealloc = tp_delete<Function>,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = "C++ function object",
    .tp_new = tp_new<Function>,
    .tp_call = function_call,  // overload
    .tp_methods = FunctionTypeMethods,
    .tp_init = function_init
};

/******************************************************************************/

bool attach_type(Object const &m, char const *name, PyTypeObject *t) noexcept {
    if (PyType_Ready(t) < 0) return false;
    Py_INCREF(t);
    return PyDict_SetItemString(+m, name, reinterpret_cast<PyObject *>(t)) >= 0;
}

bool attach(Object const &m, char const *name, Object o) noexcept {
    return o && PyDict_SetItemString(+m, name, +o) >= 0;
}

/******************************************************************************/

Object initialize(Document const &doc) {
    Object m{PyDict_New(), false};
    for (auto const &p : doc.types)
        type_names.insert_or_assign(p.first, p.second.name);

    bool ok = attach_type(m, "Any", &AnyType)
        && attach_type(m, "Function", &FunctionType)
        && attach_type(m, "TypeIndex", &TypeIndexType)
        && attach(m, "value_type", to_python(function(type_in_value)))
        && attach(m, "objects", to_tuple(doc.values))
        && attach(m, "types",   to_dict(doc.types))
        && attach(m, "set_type_names", to_python(function([](Zip<std::type_index, std::string_view> v) {
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
        // cpy_methods
    };

    PyObject* CPY_CAT(PyInit_, CPY_MODULE)(void) {
        Py_Initialize();
        return cpy::raw_object([&]() -> cpy::Object {
            cpy::Object mod {PyModule_Create(&cpy_definition), true};
            if (!mod) return {};
            cpy::Object dict = initialize(cpy::document());
            if (!dict) return {};
            Py_INCREF(+dict);
            if (PyModule_AddObject(+mod, "document", +dict) < 0) return {};
            return mod;
        });
    }

    // PyObject * testtest(void *m) {
    //     Py_Initialize();
    //     auto doc = reinterpret_cast<cpy::Document const *>(m);
    //     PyObject *out = cpy::raw_object([&]() -> cpy::Object {
    //         return cpy::initialize(*doc);
    //     });
    //     delete doc;
    //     return out;
    // }
#else
    void CPY_CAT(init, CPY_MODULE)(void) {

    }
#endif
}
