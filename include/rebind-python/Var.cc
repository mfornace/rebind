#include "Wrap.h"

namespace rebind {

// move_from is called 1) during init, V.move_from(V), to transfer the object (here just use Var move constructor)
//                     2) during assignment, R.move_from(L), to transfer the object (here cast V to new object of same type, swap)
//                     2) during assignment, R.move_from(V), to transfer the object (here cast V to new object of same type, swap)
template <class Self>
PyObject * copy_assign(PyObject *self, PyObject *value) noexcept {
    return raw_object([=] {
        DUMP("- copying variable");
        // cast_object<T>(self).assign(variable_from_object({value, true}));
        return Object(self, true);
    });
}

template <class Self>
PyObject * move_assign(PyObject *self, PyObject *value) noexcept {
    return raw_object([=] {
        DUMP("- moving variable");
        auto &s = cast_object<Self>(self);
        // Value v = value_from_object({value, true});
        // s.assign(std::move(v));
        // if (auto p = cast_if<Value>(value)) p->reset();
        return Object(self, true);
    });
}

/******************************************************************************/

template <class Self>
int operator_bool(PyObject *self) noexcept {
    if (auto v = cast_if<Self>(self)) return v->has_value();
    else return PyObject_IsTrue(self);
}

template <class Self>
PyObject * has_value(PyObject *self, PyObject *) noexcept {
    return PyLong_FromLong(operator_bool<Self>(self));
}

/******************************************************************************/

template <class Self>
PyObject * cast(PyObject *self, PyObject *type) noexcept {
    return raw_object([=] {
        return python_cast(Pointer::from(cast_object<Self>(self)), Object(type, true), Object(self, true));
    });
}

template <class Self>
PyObject * get_index(PyObject *self, PyObject *) noexcept {
    return raw_object([=] {
        auto o = Object::from(PyObject_CallObject(type_object<TypeIndex>(), nullptr));
        cast_object<TypeIndex>(o) = cast_object<Self>(self).index();
        return o;
    });
}

PyObject * ptr_qualifier(PyObject *self, PyObject *) noexcept {
    return raw_object([=] {
        return as_object(static_cast<Integer>(cast_object<Pointer>(self).qualifier()));
    });
}

// PyObject * var_is_stack_type(PyObject *self, PyObject *) noexcept {
//     return raw_object([=] {
//         return as_object(cast_object<Value>(self).is_stack_type());
//     });
// }

// PyObject * var_address(PyObject *self, PyObject *) noexcept {
//     return raw_object([=] {
//         return as_object(Integer(reinterpret_cast<std::uintptr_t>(cast_object<Value>(self).data())));
//     });
// }

template <class Self>
PyObject * get_ward(PyObject *self, PyObject *) noexcept {
    return raw_object([=] {
        Object out = cast_object<Self>(self).ward;
        return out ? out : Object(Py_None, true);
    });
}

template <class Self>
PyObject * set_ward(PyObject *self, PyObject *arg) noexcept {
    return raw_object([=]() -> Object {
        Object root{arg, true};
        while (true) { // recurse upwards to find the governing lifetime
            auto p = cast_if<Self>(root);
            if (!p || !p->ward) break;
            root = p->ward;
        }
        cast_object<Self>(self).ward = std::move(root);
        return {self, true};
    });
}

PyObject * value_from(PyObject *cls, PyObject *obj) noexcept {
    return raw_object([=]() -> Object {
        if (PyObject_TypeCheck(obj, reinterpret_cast<PyTypeObject *>(cls))) {
            // if already correct type
            return {obj, true};
        }
        if (auto p = cast_if<Value>(obj)) {
            // if a Value try .cast
            return python_cast(Pointer::from(*p), Object(cls, true), Object(obj, true));
        }
        // Try cls.__init__(obj)
        return Object::from(PyObject_CallFunctionObjArgs(cls, obj, nullptr));
    });
}

/******************************************************************************/

PyNumberMethods ValueNumberMethods = {
    .nb_bool = static_cast<inquiry>(operator_bool<Pointer>),
};

PyMethodDef ValueMethods[] = {
    {"copy_from",     static_cast<PyCFunction>(copy_assign<Value>),   METH_O,       "assign from other using C++ copy assignment"},
    {"move_from",     static_cast<PyCFunction>(move_assign<Value>),   METH_O,       "assign from other using C++ move assignment"},
    // {"address",       static_cast<PyCFunction>(address),       METH_NOARGS,  "get C++ pointer address"},
    {"_ward",         static_cast<PyCFunction>(get_ward<PyValue>),          METH_NOARGS,  "get ward object"},
    {"_set_ward",     static_cast<PyCFunction>(set_ward<PyValue>),      METH_O,       "set ward object and return self"},
    // {"qualifier",     static_cast<PyCFunction>(qualifier),     METH_NOARGS,  "return qualifier of self"},
    // {"is_stack_type", static_cast<PyCFunction>(var_is_stack_type), METH_NOARGS,  "return if object is held in stack storage"},
    {"type",          static_cast<PyCFunction>(get_index<Value>),          METH_NOARGS,  "return TypeIndex of the held C++ object"},
    {"has_value",     static_cast<PyCFunction>(has_value<Value>), METH_NOARGS,  "return if a C++ object is being held"},
    {"cast",          static_cast<PyCFunction>(cast<Value>), METH_O,       "cast to a given Python type"},
    {"from_object",   static_cast<PyCFunction>(value_from), METH_CLASS | METH_O, "cast an object to a given Python type"},
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
    .nb_bool = static_cast<inquiry>(operator_bool<Pointer>),
};

PyMethodDef PointerMethods[] = {
    {"copy_from",     static_cast<PyCFunction>(copy_assign<Pointer>),   METH_O,       "assign from other using C++ copy assignment"},
    {"move_from",     static_cast<PyCFunction>(move_assign<Pointer>),   METH_O,       "assign from other using C++ move assignment"},
    // {"address",       static_cast<PyCFunction>(address),       METH_NOARGS,  "get C++ pointer address"},
    {"_ward",         static_cast<PyCFunction>(get_ward<PyPointer>),          METH_NOARGS,  "get ward object"},
    {"_set_ward",     static_cast<PyCFunction>(set_ward<PyPointer>),      METH_O,       "set ward object and return self"},
    // {"qualifier",     static_cast<PyCFunction>(qualifier),     METH_NOARGS,  "return qualifier of self"},
    // {"is_stack_type", static_cast<PyCFunction>(var_is_stack_type), METH_NOARGS,  "return if object is held in stack storage"},
    {"type",          static_cast<PyCFunction>(get_index<Pointer>),          METH_NOARGS,  "return TypeIndex of the held C++ object"},
    {"has_value",     static_cast<PyCFunction>(has_value<Pointer>),     METH_NOARGS,  "return if a C++ object is being held"},
    {"cast",          static_cast<PyCFunction>(cast<Pointer>),          METH_O,       "cast to a given Python type"},
    // {"from_object",   static_cast<PyCFunction>(from),          METH_CLASS | METH_O, "cast an object to a given Python type"},
    {nullptr, nullptr, 0, nullptr}
};

template <>
PyTypeObject Wrap<PyPointer>::type = []{
    auto o = type_definition<Value>("rebind.Pointer", "Object representing a C++ reference");
    o.tp_as_number = &PointerNumberMethods;
    o.tp_methods = PointerMethods;
    // no init (just use default constructor)
    // tp_traverse, tp_clear
    // PyMemberDef, tp_members
    return o;
}();

/******************************************************************************/

}
