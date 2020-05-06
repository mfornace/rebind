#pragma once
#include "Load.h"

namespace ara::py {

template <class F>
PyObject *raw_object(F &&f) noexcept {
    try {
        Shared o = static_cast<F &&>(f)();
        Py_XINCREF(+o);
        return +o;
    } catch (PythonError const &) {
        return nullptr;
    } catch (std::bad_alloc const &e) {
        PyErr_SetString(PyExc_MemoryError, "C++: out of memory (std::bad_alloc)");
    } catch (std::exception const &e) {
        if (!PyErr_Occurred())
            PyErr_Format(PyExc_RuntimeError, "C++: %s", e.what());
    } catch (...) {
        if (!PyErr_Occurred())
            PyErr_SetString(PyExc_RuntimeError, unknown_exception_description());
    }
    return nullptr;
}

/******************************************************************************/

template <class T>
PyObject *c_new(PyTypeObject* subtype, PyObject*, PyObject*) noexcept {
    static_assert(noexcept(T{}), "Default constructor should be noexcept");
    PyObject *o = subtype->tp_alloc(subtype, 0); // 0 unused
    if (o) new (&cast_object_unsafe<T>(o)) T; // Default construct the C++ type
    return o;
}

/******************************************************************************/

template <class T>
void c_delete(PyObject *o) noexcept {
    reinterpret_cast<Wrap<T> *>(o)->~Wrap<T>();
    Py_TYPE(o)->tp_free(o);
}

/******************************************************************************/

template <class T>
void define_type(Instance<PyTypeObject> o, char const *name, char const *doc) noexcept {
    DUMP("define type ", name);
    (+o)->tp_name = name;
    (+o)->tp_basicsize = sizeof(Wrap<T>);
    (+o)->tp_dealloc = c_delete<T>;
    (+o)->tp_new = c_new<T>;
    (+o)->tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    (+o)->tp_doc = doc;
}

/******************************************************************************/

// move_from is called 1) during init, V.move_from(V), to transfer the object (here just use Var move constructor)
//                     2) during assignment, R.move_from(L), to transfer the object (here cast V to new object of same type, swap)
//                     2) during assignment, R.move_from(V), to transfer the object (here cast V to new object of same type, swap)
template <class Self>
PyObject * c_copy_from(PyObject* self, PyObject* value) noexcept {
    return raw_object([=]() -> Shared {
        // DUMP("- copy_from ", typeid(Self).name());
        // Shared val(value, true);
        // bool ok = Ref(cast_object<Self>(self)).assign_if(ref_from_object(val, false));
        // if (!ok) return type_error("could not assign");
        return Shared(self, true);
    });
}

template <class Self>
PyObject * c_move_from(PyObject* self, PyObject* value) noexcept {
    return raw_object([=] {
        // DUMP("- move_from");
        // Shared val(value, true);
        // Ref(cast_object<Self>(self)).assign_if(ref_from_object(val, false));
        // if (auto p = cast_if<Value>(value)) p->reset();
        return Shared(self, true);
    });
}

/******************************************************************************/

template <class Self>
int c_operator_has_value(PyObject* self) noexcept {
    if (auto v = cast_if<Self>(self)) return v->has_value();
    else return PyObject_IsTrue(self);
}

template <class Self>
PyObject* c_has_value(PyObject* self, PyObject*) noexcept {
    PyObject* out = c_operator_has_value<Self>(self) ? Py_True : Py_False;
    Py_INCREF(out);
    return out;
}

/******************************************************************************/

template <class Self>
PyObject * c_get_index(PyObject* self, PyObject*) noexcept {
    return raw_object([=] {
        auto o = Shared::from(PyObject_CallObject(static_type<Index>().object(), nullptr));
        cast_object_unsafe<Index>(+o) = cast_object<Self>(self).index();
        return o;
    });
}

// template <class Self>
// PyObject * c_qualifier(Ptr self, Ptr) noexcept {
//     return raw_object([=] {
//         return as_object(static_cast<Integer>(cast_object<Self>(self).qualifier()));
//     });
// }

// PyObject * var_is_stack_type(Ptr self, Ptr) noexcept {
//     return raw_object([=] {
//         return as_object(cast_object<Value>(self).is_stack_type());
//     });
// }

// template <class Self>
// PyObject * c_address(Ptr self, Ptr) noexcept {
//     return raw_object([=] {
//         return as_object(Integer(reinterpret_cast<std::uintptr_t>(cast_object<Self>(self).address())));
//     });
// }
// template <class Self>

template <class Self>
PyObject * c_set_ward(PyObject* self, PyObject *arg) noexcept {
    return raw_object([=]() -> Shared {
        Shared root{arg, true};
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
PyObject* c_method(PyObject* s, PyObject* args, PyObject* kws) noexcept {
    return raw_object([=]() -> Shared {
        return {};
        // auto argv = objects_from_argument_tuple(args);
        // auto const [tag, out, gil] = function_call_keywords(kws);

        // if (!PyUnicode_Check(argv[0])) return type_error("expected instance of str for method name");
        // std::string_view name = from_unicode(argv[0]);

        // auto &self = cast_object<Self>(s);
        // if (!self) return type_error("cannot lookup method on a null object");

        // auto lk = std::make_shared<PythonFrame>(!gil);
        // Caller c(lk);

        // auto v = arguments_from_objects(c, name, Ref(tag), argv.begin()+1, argv.end());
        // return {};
        // return function_call_impl(out, Ref(self), ArgView(v.data(), argv.size()-1), is_value);

        // return function_call_impl(out, Ref(std::as_const(it->second)), std::move(refs), is_value, gil, tag);
        // return type_error("not implemented");
    });
}

/******************************************************************************/

}