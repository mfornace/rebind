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
    if (o) new(&cast_object_unsafe<T>(o)) T; // Default construct the C++ type
    return o;
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

// template <class Self>
// PyObject* c_has_value(PyObject* self, PyObject*) noexcept {
//     PyObject* out = c_operator_has_value<Self>(self) ? Py_True : Py_False;
//     Py_INCREF(out);
//     return out;
// }

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

/******************************************************************************/


struct BoundMethod {
    PyObject *method;
    PyObject *self;
    BoundMethod(Shared m, Shared s) noexcept : method(m.leak()), self(s.leak()) {}
    ~BoundMethod() noexcept {Py_DECREF(method); Py_DECREF(self);}
};


struct Method {
    PyObject* signature;
    PyObject* docstring;
    PyObject* doc;
    std::string name;

    ~Method() noexcept {Py_DECREF(signature); Py_DECREF(docstring); Py_DECREF(doc);}

    static Shared repr(Always<Method> self) {return {self->doc, true};}

    static Shared str(Always<Method> self) {return {self->docstring, true};}

    static Shared get(Always<Method> self, PyObject* instance, PyObject* cls) {
        if (instance) {
            BoundMethod({self, true}, {instance, true});
            return {};
        } else return {self, true};
    }

    static Shared call(PyObject*self, PyObject* args, PyObject* kws) {
        return {};
    }
};


#define ARA_WRAP_OFFSET(type, member) offsetof(Wrap< type >, value) + offsetof(Method, member)

PyMemberDef MethodMembers[] = {
    const_cast<char*>("__signature__"), T_OBJECT_EX, ARA_WRAP_OFFSET(Method, signature), READONLY, const_cast<char*>("method signature"),
    const_cast<char*>("docstring"), T_OBJECT_EX, ARA_WRAP_OFFSET(Method, docstring), READONLY, const_cast<char*>("doc string"),
    const_cast<char*>("doc"), T_OBJECT_EX, ARA_WRAP_OFFSET(Method, doc), READONLY, const_cast<char*>("doc"),
    nullptr
};


template <>
void Wrap<Method>::initialize(Instance<PyTypeObject> o) noexcept {
    define_type<Method>(o, "ara.Method", "ara Method type");
    (+o)->tp_repr = Method::repr;
    (+o)->tp_str = api<Method::str>;
    (+o)->tp_descr_get = api<Method::get>;
    (+o)->tp_call = Method::call;
    (+o)->tp_members = MethodMembers;
    // tp_traverse, tp_clear
    // PyMemberDef, tp_members
};


}