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
        bool ok = Ref(cast_object<Self>(self)).assign_if(ref_from_object(val, false));
        if (!ok) return type_error("could not assign");
        return Object(self, true);
    });
}

template <class Self>
PyObject * c_move_from(PyObject *self, PyObject *value) noexcept {
    return raw_object([=] {
        DUMP("- move_from");
        Object val(value, true);
        Ref(cast_object<Self>(self)).assign_if(ref_from_object(val, false));
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

template <class Self>
PyObject * c_address(PyObject *self, PyObject *) noexcept {
    return raw_object([=] {
        return as_object(Integer(reinterpret_cast<std::uintptr_t>(cast_object<Self>(self).address())));
    });
}

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

template <class Self>
PyObject * c_call_method(PyObject *s, PyObject *args, PyObject *kws) noexcept {
    return raw_object([=]() -> Object {
        auto objects = objects_from_argument_tuple(args);
        auto const [tag, out, is_value, gil] = function_call_keywords(kws);

        auto refs = arguments_from_objects(objects);

        if (!PyUnicode_Check(objects[0])) return type_error("expected instance of str for method name");
        std::string_view name = from_unicode(objects[0]);

        auto &self = cast_object<Self>(s);
        refs[0] = Ref(self);

        if (auto t = self.table()) {
            if (auto it = t->methods.find(name); it != t->methods.end()) {
                return function_call_impl(out, it->second, std::move(refs), is_value, gil, tag);
            } else {
                return type_error("method not found");
            }
        } else {
            return type_error("cannot lookup method on a null object");
        }
    });
}

/******************************************************************************/

}