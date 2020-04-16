#include "Methods.h"
#include "Wrap.h"

namespace rebind::py {

PyObject * c_value_from(PyObject *cls, PyObject *obj) noexcept {
    return raw_object([=]() -> Object {
        if (PyObject_TypeCheck(obj, reinterpret_cast<PyTypeObject *>(cls)))
            // if already correct type
            return {obj, true};

        // if a Value try .cast
        if (auto p = cast_if<Value>(obj))
            return cast_to_object(Ref(*p), Object(cls, true), Object(obj, true));

        // if a Ref try .cast
        if (auto p = cast_if<Ref>(obj))
            return cast_to_object(*p, Object(cls, true), Object(obj, true));

        // Try cls.__init__(obj)
        return Object::from(PyObject_CallFunctionObjArgs(cls, obj, nullptr));
    });
}

/******************************************************************************/

PyNumberMethods ValueNumberMethods = {
    .nb_bool = static_cast<inquiry>(c_operator_has_value<Ref>),
};

PyMethodDef ValueMethods[] = {
    {"copy_from", c_function(c_copy_from<Value>),
        METH_O, "assign from other using C++ copy assignment"},

    {"move_from", c_function(c_move_from<Value>),
        METH_O, "assign from other using C++ move assignment"},

    {"method", c_function(c_method<Value>),
        METH_VARARGS | METH_KEYWORDS, "call a method given a name and arguments"},

    // {"address", c_function(c_address<Value>),
    //     METH_NOARGS, "get C++ pointer address"},

    {"_ward", c_function(c_get_ward<PyValue>),
        METH_NOARGS, "get ward object"},

    {"_set_ward", c_function(c_set_ward<PyValue>),
        METH_O, "set ward object and return self"},

    // {"is_stack_type", c_function(var_is_stack_type),
    // METH_NOARGS, "return if object is held in stack storage"},

    {"index", c_function(c_get_index<Value>),
        METH_NOARGS, "return Index of the held C++ object"},

    {"has_value", c_function(c_has_value<Value>),
        METH_NOARGS, "return if a C++ object is being held"},

    {"cast", c_function(c_cast<Value>),
        METH_O, "cast to a given Python type"},

    {"from_object", c_function(c_value_from),
        METH_CLASS | METH_O, "cast an object to a given Python type"},
    {nullptr, nullptr, 0, nullptr}
};

template <>
PyTypeObject Wrap<PyValue>::type = []{
    auto o = type_definition<Value>("rebind.Value", "Object representing a C++ value");
    o.tp_as_number = &ValueNumberMethods;
    o.tp_methods = ValueMethods;
    o.tp_call = function_call;
    // no init (just use default constructor)
    // tp_traverse, tp_clear
    // PyMemberDef, tp_members
    return o;
}();

/******************************************************************************/

PyNumberMethods RefNumberMethods = {
    .nb_bool = static_cast<inquiry>(c_operator_has_value<Ref>),
};

PyMethodDef RefMethods[] = {
    {"copy_from", c_function(c_copy_from<Ref>),
        METH_O, "assign from other using C++ copy assignment"},

    {"move_from", c_function(c_move_from<Ref>),
        METH_O, "assign from other using C++ move assignment"},

    {"method", c_function(c_method<Ref>),
        METH_VARARGS | METH_KEYWORDS, "call a method given a name and arguments"},

    // {"address", c_function(c_address<Ref>),
    //     METH_NOARGS,  "get C++ pointer address"},

    {"_ward", c_function(c_get_ward<PyRef>),
        METH_NOARGS, "get ward object"},

    {"_set_ward", c_function(c_set_ward<PyRef>),
        METH_O, "set ward object and return self"},

    // {"qualifier", c_function(c_qualifier<Ref>),
    //     METH_NOARGS, "return qualifier of self"},
    // {"is_stack_type", c_function(var_is_stack_type),
    // METH_NOARGS, "return if object is held in stack storage"},
    {"index", c_function(c_get_index<Ref>),
        METH_NOARGS, "return Index of the held C++ object"},

    {"has_value", c_function(c_has_value<Ref>),
        METH_NOARGS, "return if a C++ object is being held"},

    {"cast", c_function(c_cast<Ref>),
        METH_O, "cast to a given Python type"},

    {nullptr, nullptr, 0, nullptr}
};

template <>
PyTypeObject Wrap<PyRef>::type = []{
    auto o = type_definition<Ref>("rebind.Ref", "Object representing a C++ reference");
    o.tp_as_number = &RefNumberMethods;
    o.tp_methods = RefMethods;
    o.tp_call = function_call;
    // no init (just use default constructor)
    // tp_traverse, tp_clear
    // PyMemberDef, tp_members
    return o;
}();

/******************************************************************************/

PyObject *index_new(PyTypeObject *subtype, PyObject *, PyObject *) noexcept {
    PyObject* o = subtype->tp_alloc(subtype, 0); // 0 unused
    if (o) new (&cast_object<Index>(o)) Index(); // noexcept
    return o;
}

long index_hash(PyObject *o) noexcept {
    return static_cast<long>(std::hash<Index>()(cast_object<Index>(o)));
}

PyObject *index_repr(PyObject *o) noexcept {
    Index const *p = cast_if<Index>(o);
    if (p) return PyUnicode_FromFormat("Index('%s')", get_type_name(*p).data());
    return type_error("Expected instance of rebind.Index");
}

PyObject *index_str(PyObject *o) noexcept {
    Index const *p = cast_if<Index>(o);
    if (p) return PyUnicode_FromString(get_type_name(*p).data());
    return type_error("Expected instance of rebind.Index");
}

PyObject *index_compare(PyObject *self, PyObject *other, int op) {
    return raw_object([=]() -> Object {
        return {compare(op, cast_object<Index>(self), cast_object<Index>(other)) ? Py_True : Py_False, true};
    });
}

template <>
PyTypeObject Wrap<Index>::type = []{
    auto o = type_definition<Index>("rebind.Index", "C++ type Index");
    o.tp_repr = index_repr;
    o.tp_hash = index_hash;
    o.tp_str = index_str;
    o.tp_richcompare = index_compare;
    return o;
}();

/******************************************************************************/

int array_data_buffer(PyObject *self, Py_buffer *view, int flags) {
    auto &p = cast_object<ArrayBuffer>(self);
    // view->buf = p.data.address();
#warning "nope"
    if (p.base) {incref(p.base); view->obj = +p.base;}
    else view->obj = nullptr;
    view->itemsize = Buffer::itemsize(p.data.index());
    view->len = p.n_elem;
    // view->readonly = p.data.qualifier() == Const;
    view->format = const_cast<char *>(Buffer::format(p.data.index()).data());
    view->ndim = p.shape_stride.size() / 2;
    view->shape = p.shape_stride.data();
    view->strides = p.shape_stride.data() + view->ndim;
    view->suboffsets = nullptr;
    return 0;
}

PyBufferProcs buffer_procs{array_data_buffer, nullptr};

template <>
PyTypeObject Wrap<ArrayBuffer>::type = []{
    auto o = type_definition<ArrayBuffer>("rebind.ArrayBuffer", "C++ ArrayBuffer object");
    o.tp_as_buffer = &buffer_procs;
    return o;
}();

/******************************************************************************/

}
