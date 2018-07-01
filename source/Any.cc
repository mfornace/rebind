#include <any>

extern "C" {

/******************************************************************************/

typedef struct {
    PyObject_HEAD
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

void cpy_any_delete(PyObject *o) {
    reinterpret_cast<cpy_AnyObject *>(o)->~cpy_AnyObject();
    Py_TYPE(o)->tp_free(o);
}

static PyTypeObject cpy_AnyType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cpy.Any",
    .tp_basicsize = sizeof(cpy_AnyObject),
    .tp_dealloc = static_cast<destructor>(cpy_any_delete),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "std::any object",
    .tp_init = cpy_any_init,
    .tp_new = cpy_any_new
};

/******************************************************************************/

}

namespace cpy {

PyObject *to_python(std::any const &a) {
    return cpy_AnyObject{, a};
}

}