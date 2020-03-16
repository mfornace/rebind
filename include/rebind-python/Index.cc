namespace rebind::py {

/******************************************************************************/

PyObject *type_index_new(PyTypeObject *subtype, PyObject *, PyObject *) noexcept {
    PyObject* o = subtype->tp_alloc(subtype, 0); // 0 unused
    if (o) new (&cast_object<Index>(o)) Index(typeid(void)); // noexcept
    return o;
}

long type_index_hash(PyObject *o) noexcept {
    return static_cast<long>(cast_object<Index>(o).hash_code());
}

PyObject *type_index_repr(PyObject *o) noexcept {
    Index const *p = cast_if<Index>(o);
    if (p) return PyUnicode_FromFormat("Index('%s')", get_type_name(*p).data());
    return type_error("Expected instance of rebind.Index");
}

PyObject *type_index_str(PyObject *o) noexcept {
    Index const *p = cast_if<Index>(o);
    if (p) return PyUnicode_FromString(get_type_name(*p).data());
    return type_error("Expected instance of rebind.Index");
}

PyObject *type_index_compare(PyObject *self, PyObject *other, int op) {
    return raw_object([=]() -> Object {
        return {compare(op, cast_object<Index>(self), cast_object<Index>(other)) ? Py_True : Py_False, true};
    });
}

template <>
PyTypeObject Wrap<Index>::type = []{
    auto o = type_definition<Index>("rebind.Index", "C++ type_index object");
    o.tp_repr = type_index_repr;
    o.tp_hash = type_index_hash;
    o.tp_str = type_index_str;
    o.tp_richcompare = type_index_compare;
    return o;
}();

/******************************************************************************/

}