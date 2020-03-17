#pragma once
#include "Cast.h"

namespace rebind::py {

/******************************************************************************/

// move_from is called 1) during init, V.move_from(V), to transfer the object (here just use Var move constructor)
//                     2) during assignment, R.move_from(L), to transfer the object (here cast V to new object of same type, swap)
//                     2) during assignment, R.move_from(V), to transfer the object (here cast V to new object of same type, swap)
template <class Self>
PyObject * c_copy_from(PyObject *self, PyObject *value) noexcept {
    return raw_object([=]() -> Object {
        DUMP("- copy_from ", typeid(Self).name());
        Object val(value, true);
        bool ok = Pointer(cast_object<Self>(self)).assign_if(pointer_from_object(val, false));
        if (!ok) return type_error("could not assign");
        return Object(self, true);
    });
}

template <class Self>
PyObject * c_move_from(PyObject *self, PyObject *value) noexcept {
    return raw_object([=] {
        DUMP("- move_from");
        Object val(value, true);
        Pointer(cast_object<Self>(self)).assign_if(pointer_from_object(val, false));
        // if (auto p = cast_if<Value>(value)) p->reset();
        return Object(self, true);
    });
}

/******************************************************************************/

template <class Self>
int c_operator_has_value(PyObject *self) noexcept {
    if (auto v = cast_if<Self>(self)) return v->has_value();
    else return PyObject_IsTrue(self);
}

template <class Self>
PyObject * c_has_value(PyObject *self, PyObject *) noexcept {
    return PyLong_FromLong(c_operator_has_value<Self>(self));
}

/******************************************************************************/

template <class Self>
PyObject * c_get_index(PyObject *self, PyObject *) noexcept {
    return raw_object([=] {
        auto o = Object::from(PyObject_CallObject(type_object<Index>(), nullptr));
        cast_object<Index>(o) = cast_object<Self>(self).index();
        return o;
    });
}

template <class Self>
PyObject * c_qualifier(PyObject *self, PyObject *) noexcept {
    return raw_object([=] {
        return as_object(static_cast<Integer>(cast_object<Self>(self).qualifier()));
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
PyObject * c_get_ward(PyObject *self, PyObject *) noexcept {
    return raw_object([=] {
        Object out = cast_object<Self>(self).ward;
        return out ? out : Object(Py_None, true);
    });
}

template <class Self>
PyObject * c_set_ward(PyObject *self, PyObject *arg) noexcept {
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

/******************************************************************************/

}