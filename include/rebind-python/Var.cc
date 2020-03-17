#include "Methods.h"
#include "Wrap.h"

namespace rebind::py {

PyObject * c_value_from(PyObject *cls, PyObject *obj) noexcept {
    return raw_object([=]() -> Object {
        if (PyObject_TypeCheck(obj, reinterpret_cast<PyTypeObject *>(cls))) {
            // if already correct type
            return {obj, true};
        }
        if (auto p = cast_if<Value>(obj)) {
            // if a Value try .cast
            return cast_to_object(Pointer(*p), Object(cls, true), Object(obj, true));
        }
        // Try cls.__init__(obj)
        return Object::from(PyObject_CallFunctionObjArgs(cls, obj, nullptr));
    });
}

/******************************************************************************/

PyNumberMethods ValueNumberMethods = {
    .nb_bool = static_cast<inquiry>(c_operator_has_value<Pointer>),
};

PyMethodDef ValueMethods[] = {
    {"copy_from", static_cast<PyCFunction>(c_copy_from<Value>),
        METH_O, "assign from other using C++ copy assignment"},
    {"move_from",     static_cast<PyCFunction>(c_move_from<Value>),
        METH_O, "assign from other using C++ move assignment"},
    // {"address",       static_cast<PyCFunction>(address),
        // METH_NOARGS, "get C++ pointer address"},
    {"_ward",         static_cast<PyCFunction>(c_get_ward<PyValue>),
        METH_NOARGS, "get ward object"},
    {"_set_ward", static_cast<PyCFunction>(c_set_ward<PyValue>),
        METH_O,       "set ward object and return self"},
    // {"qualifier",     static_cast<PyCFunction>(qualifier),
    // METH_NOARGS, "return qualifier of self"},
    // {"is_stack_type", static_cast<PyCFunction>(var_is_stack_type),
    // METH_NOARGS, "return if object is held in stack storage"},
    {"index", static_cast<PyCFunction>(c_get_index<Value>),
        METH_NOARGS, "return Index of the held C++ object"},
    {"has_value", static_cast<PyCFunction>(c_has_value<Value>),
        METH_NOARGS, "return if a C++ object is being held"},
    {"cast", static_cast<PyCFunction>(c_cast<Value>),
        METH_O, "cast to a given Python type"},
    {"from_object", static_cast<PyCFunction>(c_value_from),
        METH_CLASS | METH_O, "cast an object to a given Python type"},
    {nullptr, nullptr, 0, nullptr}
};

template <>
PyTypeObject Wrap<PyValue>::type = []{
    auto o = type_definition<Value>("rebind.Value", "Object representing a C++ value");
    o.tp_as_number = &ValueNumberMethods;
    o.tp_methods = ValueMethods;
    // no init (just use default constructor)
    // tp_traverse, tp_clear
    // PyMemberDef, tp_members
    return o;
}();

/******************************************************************************/

PyNumberMethods PointerNumberMethods = {
    .nb_bool = static_cast<inquiry>(c_operator_has_value<Pointer>),
};

PyMethodDef PointerMethods[] = {
    {"copy_from", static_cast<PyCFunction>(c_copy_from<Pointer>),
        METH_O, "assign from other using C++ copy assignment"},
    {"move_from", static_cast<PyCFunction>(c_move_from<Pointer>),
        METH_O, "assign from other using C++ move assignment"},
    // {"address",       static_cast<PyCFunction>(address),
    // METH_NOARGS,  "get C++ pointer address"},
    {"_ward", static_cast<PyCFunction>(c_get_ward<PyPointer>),
        METH_NOARGS, "get ward object"},
    {"_set_ward", static_cast<PyCFunction>(c_set_ward<PyPointer>),
        METH_O, "set ward object and return self"},
    {"qualifier", static_cast<PyCFunction>(c_qualifier<Pointer>),
        METH_NOARGS, "return qualifier of self"},
    // {"is_stack_type", static_cast<PyCFunction>(var_is_stack_type),
    // METH_NOARGS, "return if object is held in stack storage"},
    {"index", static_cast<PyCFunction>(c_get_index<Pointer>),
        METH_NOARGS, "return Index of the held C++ object"},
    {"has_value", static_cast<PyCFunction>(c_has_value<Pointer>),
        METH_NOARGS, "return if a C++ object is being held"},
    {"cast", static_cast<PyCFunction>(c_cast<Pointer>),
        METH_O, "cast to a given Python type"},
    // {"from_object",   static_cast<PyCFunction>(from),
    // METH_CLASS | METH_O, "cast an object to a given Python type"},
    {nullptr, nullptr, 0, nullptr}
};

template <>
PyTypeObject Wrap<PyPointer>::type = []{
    auto o = type_definition<Pointer>("rebind.Pointer", "Object representing a C++ reference");
    o.tp_as_number = &PointerNumberMethods;
    o.tp_methods = PointerMethods;
    // no init (just use default constructor)
    // tp_traverse, tp_clear
    // PyMemberDef, tp_members
    return o;
}();

/******************************************************************************/

}
