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
        // DUMP("- copy_from ", typeid(Self).name());
        // Object val(value, true);
        // bool ok = Ref(cast_object<Self>(self)).assign_if(ref_from_object(val, false));
        // if (!ok) return type_error("could not assign");
        return Object(self, true);
    });
}

template <class Self>
PyObject * c_move_from(PyObject *self, PyObject *value) noexcept {
    return raw_object([=] {
        // DUMP("- move_from");
        // Object val(value, true);
        // Ref(cast_object<Self>(self)).assign_if(ref_from_object(val, false));
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
    if (auto v = cast_if<Self>(self)) return as_object(v->has_value());
    else return as_object(bool(PyObject_IsTrue(self)));
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

// template <class Self>
// PyObject * c_qualifier(PyObject *self, PyObject *) noexcept {
//     return raw_object([=] {
//         return as_object(static_cast<Integer>(cast_object<Self>(self).qualifier()));
//     });
// }

// PyObject * var_is_stack_type(PyObject *self, PyObject *) noexcept {
//     return raw_object([=] {
//         return as_object(cast_object<Value>(self).is_stack_type());
//     });
// }

// template <class Self>
// PyObject * c_address(PyObject *self, PyObject *) noexcept {
//     return raw_object([=] {
//         return as_object(Integer(reinterpret_cast<std::uintptr_t>(cast_object<Self>(self).address())));
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

template <class Self>
PyObject * c_method(PyObject *s, PyObject *args, PyObject *kws) noexcept {
    return raw_object([=]() -> Object {
        auto argv = objects_from_argument_tuple(args);
        auto const [tag, out, gil] = function_call_keywords(kws);

        if (!PyUnicode_Check(argv[0])) return type_error("expected instance of str for method name");
        std::string_view name = from_unicode(argv[0]);

        auto &self = cast_object<Self>(s);
        if (!self) return type_error("cannot lookup method on a null object");

        auto lk = std::make_shared<PythonFrame>(!gil);
        Caller c(lk);

        auto v = arguments_from_objects(c, name, Ref(tag), argv.begin()+1, argv.end());
        return {};
        // return function_call_impl(out, Ref(self), ArgView(v.data(), argv.size()-1), is_value);

        // return function_call_impl(out, Ref(std::as_const(it->second)), std::move(refs), is_value, gil, tag);
        // return type_error("not implemented");
    });
}

/******************************************************************************/

}