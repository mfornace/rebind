/**
 * @brief Python-related C++ source code for cpy
 * @file Python.cc
 */
#include <cpy/PythonCast.h>
#include <cpy/PythonAPI.h>
#include <cpy/Document.h>
#include <any>
#include <iostream>
#include <numeric>

#ifndef CPY_MODULE
#   define CPY_MODULE libcpy
#endif

#define CPY_CAT_IMPL(s1, s2) s1##s2
#define CPY_CAT(s1, s2) CPY_CAT_IMPL(s1, s2)

#define CPY_STRING_IMPL(x) #x
#define CPY_STRING(x) CPY_STRING_IMPL(x)

namespace cpy {

// Parse argument tuple for 1 argument. Set error and return NULL if parsing fails.
PyObject *one_argument(PyObject *args, PyTypeObject *type=nullptr) noexcept {
    Py_ssize_t n = PyTuple_Size(args);
    if (n != 1) {
        DUMP(n);
        return type_error("Expected single argument but got %zd", n);
    }
    PyObject *value = PyTuple_GET_ITEM(args, 0);
    if (type && !PyObject_TypeCheck(value, type))
        return type_error("C++: invalid argument type %R", value->ob_type);
    return value;
}

template <class T>
PyObject *tp_new(PyTypeObject *subtype, PyObject *, PyObject *) noexcept {
    static_assert(noexcept(T{}), "Default constructor should be noexcept");
    PyObject *o = subtype->tp_alloc(subtype, 0); // 0 unused
    if (o) new (&cast_object<T>(o)) T; // Default construct the C++ type
    return o;
}

template <class T>
void tp_delete(PyObject *o) noexcept {
    reinterpret_cast<Holder<T> *>(o)->~Holder<T>();
    Py_TYPE(o)->tp_free(o);
}

// move_from is called 1) during init, V.move_from(V), to transfer the object (here just use Var move constructor)
//                     2) during assignment, R.move_from(L), to transfer the object (here cast V to new object of same type, swap)
//                     2) during assignment, R.move_from(V), to transfer the object (here cast V to new object of same type, swap)
PyObject * var_copy_assign(PyObject *self, PyObject *args) noexcept {
    if (PyObject *value = one_argument(args)) {
        return raw_object([=] {
            DUMP("- copying variable");
            cast_object<Var>(self).assign(variable_from_object({value, true}));
            return Object(self, true);
        });
    } else return nullptr;
}

PyObject * var_move_assign(PyObject *self, PyObject *args) noexcept {
    if (PyObject *value = one_argument(args)) {
        return raw_object([=] {
            DUMP("- moving variable");
            auto &s = cast_object<Var>(self);
            Variable v = variable_from_object({value, true});
            v.move_if_lvalue();
            s.assign(std::move(v));
            // if (auto p = cast_if<Variable>(value)) p->reset();
            return Object(self, true);
        });
    } else return nullptr;
}

/******************************************************************************/

int array_data_buffer(PyObject *self, Py_buffer *view, int flags) {
    auto &p = cast_object<ArrayBuffer>(self);
    view->buf = p.data;
    if (p.base) {incref(p.base); view->obj = +p.base;}
    else view->obj = nullptr;
    view->itemsize = Buffer::itemsize(*p.type);
    view->len = p.n_elem;
    view->readonly = !p.mutate;
    view->format = const_cast<char *>(Buffer::format(*p.type).data());
    view->ndim = p.shape_stride.size() / 2;
    view->shape = p.shape_stride.data();
    view->strides = p.shape_stride.data() + view->ndim;
    view->suboffsets = nullptr;
    return 0;
}

PyBufferProcs buffer_procs{array_data_buffer, nullptr};

template <>
PyTypeObject Holder<ArrayBuffer>::type = []{
    PyTypeObject o{PyVarObject_HEAD_INIT(NULL, 0)};
    o.tp_name = "cpy.ArrayBuffer";
    o.tp_basicsize = sizeof(Holder<ArrayBuffer>);
    o.tp_dealloc = tp_delete<ArrayBuffer>;
    o.tp_as_buffer = &buffer_procs;
    o.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    o.tp_doc = "C++ ArrayBuffer object";
    o.tp_new = tp_new<ArrayBuffer>;
    return o;
}();

/******************************************************************************/

PyObject *type_index_new(PyTypeObject *subtype, PyObject *, PyObject *) noexcept {
    PyObject* o = subtype->tp_alloc(subtype, 0); // 0 unused
    if (o) new (&cast_object<TypeIndex>(o)) TypeIndex(typeid(void)); // noexcept
    return o;
}

long type_index_hash(PyObject *o) noexcept {
    return static_cast<long>(cast_object<TypeIndex>(o).hash_code());
}

PyObject *type_index_repr(PyObject *o) noexcept {
    TypeIndex const *p = cast_if<TypeIndex>(o);
    if (p) return PyUnicode_FromFormat("TypeIndex('%s')", get_type_name(*p).data());
    return type_error("Expected instance of cpy.TypeIndex");
}

PyObject *type_index_str(PyObject *o) noexcept {
    TypeIndex const *p = cast_if<TypeIndex>(o);
    if (p) return PyUnicode_FromString(get_type_name(*p).data());
    return type_error("Expected instance of cpy.TypeIndex");
}

PyObject *type_index_compare(PyObject *self, PyObject *other, int op) {
    return raw_object([=]() -> Object {
        auto const &s = cast_object<TypeIndex>(self);
        auto const &o = cast_object<TypeIndex>(other);
        bool out;
        switch(op) {
            case(Py_LT): out = s < o;
            case(Py_GT): out = s > o;
            case(Py_LE): out = s <= o;
            case(Py_EQ): out = s == o;
            case(Py_NE): out = s != o;
            case(Py_GE): out = s >= o;
        }
        return {out ? Py_True : Py_False, true};
    });
}

template <>
PyTypeObject Holder<TypeIndex>::type = []{
    PyTypeObject o{PyVarObject_HEAD_INIT(NULL, 0)};
    o.tp_name = "cpy.TypeIndex";
    o.tp_basicsize = sizeof(Holder<TypeIndex>);
    o.tp_dealloc = tp_delete<TypeIndex>;
    o.tp_repr = type_index_repr;
    o.tp_hash = type_index_hash;
    o.tp_str = type_index_str;
    o.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    o.tp_richcompare = type_index_compare;
    o.tp_doc = "C++ type_index object";
    o.tp_new = type_index_new;
    return o;
}();

/******************************************************************************/

template <class T>
PyObject * copy_from(PyObject *self, PyObject *args) noexcept {
    PyObject *value = one_argument(args);
    if (!value) return nullptr;
    return raw_object([=] {
        cast_object<T>(self) = cast_object<T>(value); // not notexcept
        return Object(Py_None, true);
    });
}

int var_bool(PyObject *self) noexcept {
    if (auto v = cast_if<Variable>(self)) return v->has_value();
    else return PyObject_IsTrue(self);
}

PyObject * var_has_value(PyObject *self, PyObject *) noexcept {
    return PyLong_FromLong(var_bool(self));
}

/******************************************************************************/

PyObject * var_cast(PyObject *self, PyObject *args) noexcept {
    PyObject *t = one_argument(args);
    if (!t) return nullptr;
    return raw_object([=] {
        return python_cast(std::move(cast_object<Variable>(self)), Object(t, true), Object(self, true));
    });
}

PyObject * var_type(PyObject *self, PyObject *) noexcept {
    return raw_object([=] {
        auto o = Object::from(PyObject_CallObject(type_object<TypeIndex>(), nullptr));
        cast_object<TypeIndex>(o) = cast_object<Variable>(self).type();
        return o;
    });
}

PyObject * var_qualifier(PyObject *self, PyObject *) noexcept {
    return raw_object([=] {
        return as_object(static_cast<Integer>(cast_object<Variable>(self).qualifier()));
    });
}

PyObject * var_is_stack_type(PyObject *self, PyObject *) noexcept {
    return raw_object([=] {
        return as_object(cast_object<Variable>(self).is_stack_type());
    });
}

PyObject * var_address(PyObject *self, PyObject *) noexcept {
    return raw_object([=] {
        return as_object(Integer(reinterpret_cast<std::uintptr_t>(cast_object<Variable>(self).data())));
    });
}

PyObject * var_ward(PyObject *self, PyObject *) noexcept {
    return raw_object([=] {
        Object out = cast_object<Var>(self).ward;
        return out ? out : Object(Py_None, true);
    });
}

PyObject * var_set_ward(PyObject *self, PyObject *args) noexcept {
    return raw_object([=]() -> Object {
        if (auto arg = one_argument(args)) {
            Object root{arg, true};
            while (true) { // recurse upwards to find the governing lifetime
                auto p = cast_if<Var>(root);
                if (!p || !p->ward) break;
                root = p->ward;
            }
            cast_object<Var>(self).ward = std::move(root);
            return {self, true};
        } else return {};
    });
}

PyObject * var_from(PyObject *cls, PyObject *obj) noexcept {
    return raw_object([=]() -> Object {
        if (PyObject_TypeCheck(obj, reinterpret_cast<PyTypeObject *>(cls))) {
            // if already correct type
            return {obj, true};
        }
        if (auto p = cast_if<Variable>(obj)) {
            // if a Variable try .cast
            return python_cast(static_cast<Variable const &>(*p).reference(), Object(cls, true), Object(obj, true));
        }
        // Try cls.__init__(obj)
        return Object::from(PyObject_CallFunctionObjArgs(cls, obj, nullptr));
    });
}

PyNumberMethods VarNumberMethods = {
    .nb_bool = static_cast<inquiry>(var_bool),
};

PyMethodDef VarMethods[] = {
    {"copy_from",     static_cast<PyCFunction>(var_copy_assign),   METH_VARARGS, "assign from other using C++ copy assignment"},
    {"move_from",     static_cast<PyCFunction>(var_copy_assign),   METH_VARARGS, "assign from other using C++ move assignment"},
    {"address",       static_cast<PyCFunction>(var_address),       METH_NOARGS,  "get C++ pointer address"},
    {"_ward",         static_cast<PyCFunction>(var_ward),          METH_NOARGS,  "get ward object"},
    {"_set_ward",     static_cast<PyCFunction>(var_set_ward),      METH_VARARGS, "set ward object and return self"},
    {"qualifier",     static_cast<PyCFunction>(var_qualifier),     METH_NOARGS,  "return qualifier of self"},
    {"is_stack_type", static_cast<PyCFunction>(var_is_stack_type), METH_NOARGS, "return if object is held in stack storage"},
    {"type",          static_cast<PyCFunction>(var_type),          METH_NOARGS,  "return TypeIndex of the held C++ object"},
    {"has_value",     static_cast<PyCFunction>(var_has_value),     METH_VARARGS, "return if a C++ object is being held"},
    {"cast",          static_cast<PyCFunction>(var_cast),          METH_VARARGS, "cast to a given Python type"},
    {"from_object",   static_cast<PyCFunction>(var_from),          METH_CLASS | METH_O, "cast an object to a given Python type"},
    {nullptr, nullptr, 0, nullptr}
};

template <>
PyTypeObject Holder<Var>::type = []{
    PyTypeObject o{PyVarObject_HEAD_INIT(NULL, 0)};
    o.tp_name = "cpy.Variable";
    o.tp_as_number = &VarNumberMethods;
    o.tp_basicsize = sizeof(Holder<Var>);
    o.tp_dealloc = tp_delete<Var>;
    o.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    o.tp_doc = "C++ class object";
    o.tp_methods = VarMethods;
    o.tp_new = tp_new<Var>; // no init (just use default constructor)
    return o;
}();

/******************************************************************************/

Object call_overload(ErasedFunction const &fun, Object const &args, bool gil) {
    if (auto py = fun.target<PythonFunction>())
        return {PyObject_CallObject(+py->function, +args), false};
    auto pack = args_from_python(args);
    DUMP("constructed python args ", pack.size());
    for (auto const &p : pack) DUMP(p.type());
    Variable out;
    {
        auto lk = std::make_shared<PythonFrame>(!gil);
        Caller ct(lk);
        DUMP("calling the args: size=", pack.size());
        out = fun(ct, std::move(pack));
    }
    DUMP("got the output ", out.type());
    if (auto p = out.target<Object const &>()) return *p;
    // if (auto p = out.target<PyObject * &>()) return {*p, true};
    // Convert the C++ Variable to a cpy.Variable
    return variable_cast(std::move(out));
}

PyObject * not_none(PyObject *o) {return o == Py_None ? nullptr : o;}

/******************************************************************************/

PyObject * function_call(PyObject *self, PyObject *pyargs, PyObject *kws) noexcept {
    return raw_object([=]() -> Object {
        bool gil = true;
        std::optional<TypeIndex> t0, t1;
        PyObject *sig=nullptr;
        if (kws && PyDict_Check(kws)) {
            PyObject *g = PyDict_GetItemString(kws, "gil");
            if (g) gil = PyObject_IsTrue(g);
            sig = not_none(PyDict_GetItemString(kws, "signature")); // either int or Tuple[TypeIndex] or None
            auto r = not_none(PyDict_GetItemString(kws, "return_type")); // either TypeIndex or None
            auto f = not_none(PyDict_GetItemString(kws, "first_type")); // either TypeIndex or None
            // std::cout << PyObject_Length(kws) << bool(g) << bool(sig) << bool(f) << bool(r) << std::endl;
            // if (PyObject_Length(kws) != unsigned(bool(g)) + (sig || r || f))
                // return type_error("C++: unexpected extra keywords");
            if (r) t0 = cast_object<TypeIndex>(r);
            if (f) t1 = cast_object<TypeIndex>(f);
        }
        DUMP("specified types", bool(t0), bool(t1));
        DUMP("gil = ", gil, " ", Py_REFCNT(self), Py_REFCNT(pyargs));
        DUMP("number of signatures ", cast_object<Function>(self).overloads.size());

        Object args(pyargs, true);
        auto const &overloads = cast_object<Function>(self).overloads;

        if (overloads.size() == 1) // only 1 overload
            return call_overload(overloads[0].second, args, gil);

        if (sig && PyLong_Check(sig)) { // signature given as an integer index
            auto i = PyLong_AsLongLong(sig);
            if (i < 0) i += overloads.size();
            if (i <= overloads.size() || i < 0)
                return call_overload(overloads[i].second, args, gil);
            PyErr_SetString(PyExc_IndexError, "signature index out of bounds");
            return Object();
        }

        auto const n = PyTuple_GET_SIZE(+args);
        Variable *first = n ? cast_if<Variable>(PyTuple_GET_ITEM(+args, 0)) : nullptr;
        auto errors = Object::from(PyList_New(0));

        //  Check for equivalence on the first argument first -- provides short-circuiting for methods
        for (auto const exact : {true, false}) {
            for (auto const &o : overloads) {
                bool const match = (o.first.size() < 2) || (first && first->type().matches(o.first[1]));
                if (match != exact) continue;
                if (sig) { // check the explicit signature that was passed in
                    if (PyTuple_Check(sig)) {
                        auto const len = PyObject_Length(sig);
                        if (len > o.first.size())
                            return type_error("C++: too many types given in signature");
                        for (Py_ssize_t i = 0; i != len; ++i) {
                            PyObject *x = PyTuple_GET_ITEM(sig, i);
                            if (x != Py_None && !cast_object<TypeIndex>(x).matches(o.first[i])) continue;
                        }
                    } else return type_error("C++: expected 'signature' to be a tuple");
                } else {
                    if (t0 && o.first.size() > 0 && !o.first[0].matches(*t0)) continue; // check that the return type matches if specified
                    if (t1 && o.first.size() > 1 && !o.first[1].matches(*t1)) continue; // check that the first argument type matches if specified
                }

                try {
                    return call_overload(o.second, args, gil);
                } catch (WrongType const &e) {
                    if (PyList_Append(+errors, +as_object(wrong_type_message(e)))) return {};
                } catch (WrongNumber const &e) {
                    unsigned int n0 = e.expected, n = e.received;
                    auto s = Object::from(PyUnicode_FromFormat("C++: wrong number of arguments (expected %u, got %u)", n0, n));
                    if (PyList_Append(+errors, +s)) return {};
                } catch (DispatchError const &e) {
                    if (PyList_Append(+errors, +as_object(std::string_view(e.what())))) return {};
                }
            }
        }
        // Raise an exception with a list of the messages
        return PyErr_SetObject(TypeErrorObject, +errors), nullptr;
    });
}

/******************************************************************************/

PyObject * function_signatures(PyObject *self, PyObject *) noexcept {
    return raw_object([=] {
        return map_as_tuple(cast_object<Function>(self).overloads, [](auto const &p) -> Object {
            if (!p.first) return {Py_None, true};
            return map_as_tuple(p.first, [](auto const &o) {return as_object(o);});
        });
    });
}

PyMethodDef FunctionTypeMethods[] = {
    // {"move_from", static_cast<PyCFunction>(move_from<Function>),   METH_VARARGS, "move it"},
    {"copy_from", static_cast<PyCFunction>(copy_from<Function>),   METH_VARARGS, "copy it"},
    {"signatures", static_cast<PyCFunction>(function_signatures),   METH_NOARGS, "get signature"},
    {nullptr, nullptr, 0, nullptr}
};

int function_init(PyObject *self, PyObject *args, PyObject *kws) noexcept {
    static char const * keys[] = {"function", "signature", nullptr};
    PyObject *fun = nullptr;
    PyObject *sig = nullptr;
    if (!PyArg_ParseTupleAndKeywords(args, kws, "|OO", const_cast<char **>(keys), &fun, &sig))
        return -1;
    if (!fun || +fun == Py_None) return 0;

    if (!PyCallable_Check(fun))
        return type_error("Expected callable type but got %R", fun->ob_type), -1;
    if (sig && sig != Py_None && !PyTuple_Check(sig))
        return type_error("Expected signature to be tuple or None but got %R", sig->ob_type), -1;
    cast_object<Function>(self).emplace(PythonFunction(Object(fun, true), Object(sig ? sig : Py_None, true)), {});
    return 0;
}

template <>
PyTypeObject Holder<Function>::type = []{
    PyTypeObject o{PyVarObject_HEAD_INIT(NULL, 0)};
    o.tp_name = "cpy.Function";
    o.tp_basicsize = sizeof(Holder<Function>);
    o.tp_dealloc = tp_delete<Function>;
    o.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    o.tp_doc = "C++ function object";
    o.tp_new = tp_new<Function>;
    o.tp_call = function_call;  // overload
    o.tp_methods = FunctionTypeMethods;
    o.tp_init = function_init;
    return o;
}();

/******************************************************************************/

bool attach_type(Object const &m, char const *name, PyTypeObject *t) noexcept {
    if (PyType_Ready(t) < 0) return false;
    incref(reinterpret_cast<PyObject *>(t));
    return PyDict_SetItemString(+m, name, reinterpret_cast<PyObject *>(t)) >= 0;
}

bool attach(Object const &m, char const *name, Object o) noexcept {
    return o && PyDict_SetItemString(m, name, o) >= 0;
}

/******************************************************************************/

Object initialize(Document const &doc) {
    auto m = Object::from(PyDict_New());
    for (auto const &p : doc.types)
        if (p.second) type_names.emplace(p.first, p.first.name());//p.second->first);

    if (PyType_Ready(type_object<ArrayBuffer>()) < 0) return {};
    incref(type_object<ArrayBuffer>());

    bool ok = attach_type(m, "Variable", type_object<Variable>())
        && attach_type(m, "Function", type_object<Function>())
        && attach_type(m, "TypeIndex", type_object<TypeIndex>())
            // Tuple[Tuple[int, TypeIndex, int], ...]
        && attach(m, "scalars", map_as_tuple(scalars, [](auto const &x) {
            return args_as_tuple(as_object(static_cast<Integer>(std::get<0>(x))),
                                 as_object(static_cast<TypeIndex>(std::get<1>(x))),
                                 as_object(static_cast<Integer>(std::get<2>(x))));
        }))
            // Tuple[Tuple[TypeIndex, Tuple[Tuple[str, function], ...]], ...]
        && attach(m, "contents", map_as_tuple(doc.contents, [](auto const &x) {
            Object o;
            if (auto p = x.second.template target<Function const &>()) o = as_object(*p);
            else if (auto p = x.second.template target<TypeIndex const &>()) o = as_object(*p);
            else if (auto p = x.second.template target<TypeData const &>()) o = args_as_tuple(
                map_as_tuple(p->methods, [](auto const &x) {return args_as_tuple(as_object(x.first), as_object(x.second));}),
                map_as_tuple(p->data, [](auto const &x) {return args_as_tuple(as_object(x.first), variable_cast(Variable(x.second)));})
            );
            else o = variable_cast(Variable(x.second));
            return args_as_tuple(as_object(x.first), std::move(o));
        }))
        && attach(m, "set_output_conversion", as_object(Function::of([](Object t, Object o) {
            output_conversions.insert_or_assign(std::move(t), std::move(o));
        })))
        && attach(m, "set_input_conversion", as_object(Function::of([](Object t, Object o) {
            input_conversions.insert_or_assign(std::move(t), std::move(o));
        })))
        && attach(m, "clear_global_objects", as_object(Function::of([] {
            input_conversions.clear();
            output_conversions.clear();
            python_types.clear();
        })))
        && attach(m, "set_debug", as_object(Function::of([](bool b) {return std::exchange(Debug, b);})))
        && attach(m, "debug", as_object(Function::of([] {return Debug;})))
        && attach(m, "set_type_error", as_object(Function::of([](Object o) {
            // Add a ward in global storage to ensure the lifetime of this raw object
            TypeErrorObject = +(python_types[typeid(DispatchError)] = std::move(o));
        })))
        && attach(m, "set_type", as_object(Function::of([](TypeIndex idx, Object o) {
            DUMP("set_type in");
            python_types.emplace(idx.info(), std::move(o));
            DUMP("set_type out");
        })))
        && attach(m, "set_type_names", as_object(Function::of([](Zip<TypeIndex, std::string_view> v) {
            for (auto const &p : v) type_names.insert_or_assign(p.first, p.second);
        })));
    return ok ? m : Object();
}

}

extern "C" {

#if PY_MAJOR_VERSION > 2
    static struct PyModuleDef cpy_definition = {
        PyModuleDef_HEAD_INIT,
        CPY_STRING(CPY_MODULE),
        "A Python module to run C++ unit tests",
        -1,
    };

    PyObject* CPY_CAT(PyInit_, CPY_MODULE)(void) {
        Py_Initialize();
        return cpy::raw_object([&]() -> cpy::Object {
            cpy::Object mod {PyModule_Create(&cpy_definition), true};
            if (!mod) return {};
            cpy::Object dict = initialize(cpy::document());
            if (!dict) return {};
            cpy::incref(+dict);
            if (PyModule_AddObject(+mod, "document", +dict) < 0) return {};
            return mod;
        });
    }
#else
    void CPY_CAT(init, CPY_MODULE)(void) {

    }
#endif
}
