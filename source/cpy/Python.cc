#include <cpy/PythonAPI.h>
#include <any>


namespace cpy {


bool from_python(Value &v, Object o) {
    if (+o == Py_None) {
        v = std::monostate();
    } else if (PyBool_Check(+o)) {
        v = (+o == Py_True) ? true : false;
    } else if (PyLong_Check(+o)) {
        v = static_cast<Integer>(PyLong_AsLongLong(+o));
    } else if (PyFloat_Check(+o)) {
        v = static_cast<Real>(PyFloat_AsDouble(+o));
    } else if (PyComplex_Check(+o)) {
        v = std::complex<double>{PyComplex_RealAsDouble(+o), PyComplex_ImagAsDouble(+o)};
    } else if (PyBytes_Check(+o)) {
        char *c;
        Py_ssize_t size;
        PyBytes_AsStringAndSize(+o, &c, &size);
        v = std::string(c, size);
    } else if (PyUnicode_Check(+o)) { // no use of wstring for now.
        Py_ssize_t size;
#if PY_MAJOR_VERSION > 2
        char const *c = PyUnicode_AsUTF8AndSize(+o, &size);
#else
        char *c;
        if (PyString_AsStringAndSize(+o, &c, &size)) return false;
#endif
        if (c) v = std::string(static_cast<char const *>(c), size);
        else return false;
    } else if (PyObject_CheckBuffer(+o)) {
// hmm
    } else if (PyMemoryView_Check(+o)) {
// hmm
    } else {
        PyErr_SetString(PyExc_TypeError, "Invalid type for conversion to C++");
    }
    return !PyErr_Occurred();
};

/******************************************************************************/

bool build_argpack(ArgPack &pack, Object pypack) {
    return cpy::vector_from_iterable(pack, pypack, [](cpy::Object &&o, bool &ok) {
        cpy::Value v;
        ok = ok && cpy::from_python(v, std::move(o));
        return v;
    });
}

/******************************************************************************/

}

extern "C" {

/******************************************************************************/

typedef struct {
    PyObject_HEAD // 16 bytes for the ref count and the type object
    std::any value; // I think stack is OK because this object is only casted to anyway.
} cpy_AnyObject;

int cpy_any_init(PyObject *self, PyObject *args, PyObject *kws) {
    PyObject *value;
    char const *keywords[] = {"value", nullptr};
    if (!PyArg_ParseTupleAndKeywords(args, kws, "|O", const_cast<char **>(keywords), &value)) return -1;
    reinterpret_cast<cpy_AnyObject *>(self)->value.reset();
    return 0;
}

PyObject *cpy_any_new(PyTypeObject *subtype, PyObject *, PyObject *) {
    PyObject* o = subtype->tp_alloc(subtype, 0); // 0 unused
    new (&(reinterpret_cast<cpy_AnyObject *>(o)->value)) std::any; // noexcept
    return o;
}

PyObject * cpy_any_call(PyObject *self, PyObject *args, PyObject *kws) {
    PyObject *value;
    char const *keywords[] = {"value", nullptr};
    if (!PyArg_ParseTupleAndKeywords(args, kws, "O", const_cast<char **>(keywords), &value)) return nullptr;
    return cpy::return_object([=] {
        reinterpret_cast<cpy_AnyObject *>(self)->value.reset();
        return cpy::Object(value, true);
    });
}


void cpy_any_delete(PyObject *o) {
    reinterpret_cast<cpy_AnyObject *>(o)->~cpy_AnyObject();
    Py_TYPE(o)->tp_free(o);
}

static PyMethodDef cpy_Any_methods = {
    .ml_name = "__call__",
    .ml_meth = (PyCFunction) cpy_any_call,
    .ml_flags = METH_VARARGS | METH_KEYWORDS,
    .ml_doc = "a test method"
};

static PyMethodDef cpy_methods[] = {nullptr};

static PyTypeObject cpy_AnyType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cpy.Any",
    .tp_basicsize = sizeof(cpy_AnyObject),
    .tp_dealloc = static_cast<destructor>(cpy_any_delete),
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = "std::any object",
    .tp_new = cpy_any_new,
    .tp_init = cpy_any_init,  // overload
    .tp_call = cpy_any_call,  // overload
    .tp_methods = cpy_methods // overload
};

/******************************************************************************/

}

namespace cpy {

bool define_any(PyObject *m) {
    if (PyType_Ready(&cpy_AnyType) < 0) return false;
    Py_INCREF(&cpy_AnyType);
    PyModule_AddObject(m, "Any", reinterpret_cast<PyObject *>(&cpy_AnyType));
    return true;
}

// PyObject *to_python(std::any const &a) {
//     return cpy_AnyObject{, a};
// }

}
