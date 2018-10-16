/**
 * @brief Python-related C++ source code for cpy
 * @file Python.cc
 */
#include <cpy/PythonAPI.h>
#include <cpy/Document.h>
#include <any>
#include <iostream>

#ifndef CPY_MODULE
#   define CPY_MODULE libcpy
#endif

namespace cpy {

PyObject *one_argument(PyObject *args, PyTypeObject *type=nullptr) noexcept {
    Py_ssize_t n = PyTuple_Size(args);
    if (n != 1) {
        std::cout << "huh" << n << std::endl;
        return type_error("Expected single argument but got %zd", n);
    }
    PyObject *value = PyTuple_GET_ITEM(args, 0);
    if (type && !PyObject_TypeCheck(value, type))
        return type_error("C++: invalid argument type %R", value->ob_type);
    return value;
}

template <class T>
PyObject *tp_new(PyTypeObject *subtype, PyObject *, PyObject *) noexcept {
    PyObject *o = subtype->tp_alloc(subtype, 0); // 0 unused
    if (!o) return nullptr;
    static_assert(noexcept(T{}), "Default constructor should be noexcept");
    new (&cast_object<T>(o)) T; // Default construct the C++ type
    return o;
}

template <class T>
void tp_delete(PyObject *o) noexcept {
    reinterpret_cast<Holder<T> *>(o)->~Holder<T>();
    Py_TYPE(o)->tp_free(o);
}

template <class T>
PyObject * move_from(PyObject *self, PyObject *args) noexcept {
    PyObject *value = one_argument(args);
    if (!value) return nullptr;
    return raw_object([=] {
        cast_object<T>(self) = std::move(cast_object<T>(value));
        return Object(self, true);
    });
}

/******************************************************************************/

PyObject *type_index_new(PyTypeObject *subtype, PyObject *, PyObject *) noexcept {
    PyObject* o = subtype->tp_alloc(subtype, 0); // 0 unused
    new (&cast_object<std::type_index>(o)) std::type_index(typeid(void)); // noexcept
    return o;
}

long type_index_hash(PyObject *o) noexcept {
    return static_cast<long>(cast_object<std::type_index>(o).hash_code());
}

PyObject *type_index_repr(PyObject *o) noexcept {
    std::type_index const *p = cast_if<std::type_index>(o);
    if (p) return PyUnicode_FromFormat("TypeIndex('%s')", get_type_name(*p).data());
    return type_error("Expected instance of cpy.TypeIndex");
}

PyObject *type_index_str(PyObject *o) noexcept {
    std::type_index const *p = cast_if<std::type_index>(o);
    if (p) return PyUnicode_FromString(get_type_name(*p).data());
    return type_error("Expected instance of cpy.TypeIndex");
}

PyObject *type_index_compare(PyObject *self, PyObject *other, int op) {
    return raw_object([=]() -> Object {
        auto const &s = cast_object<std::type_index>(self);
        auto const &o = cast_object<std::type_index>(other);
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

PyTypeObject TypeIndexType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cpy.TypeIndex",
    .tp_hash = type_index_hash,
    .tp_str = type_index_str,
    .tp_repr = type_index_repr,
    .tp_richcompare = type_index_compare,
    .tp_basicsize = sizeof(Holder<std::type_index>),
    .tp_dealloc = tp_delete<std::type_index>,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = "C++ type_index object",
    .tp_new = type_index_new,
};

/******************************************************************************/

template <class T>
PyObject * copy_from(PyObject *self, PyObject *args) noexcept {
    PyObject *value = one_argument(args, &type_ref(Type<T>()));
    if (!value) return nullptr;
    return raw_object([=] {
        cast_object<T>(self) = cast_object<T>(value); // not notexcept
        return Object(Py_None, true);
    });
}

int value_bool(PyObject *self) noexcept {
    auto t = cast_if<Value>(self);
    return t ? t->has_value() : PyObject_IsTrue(self);
}

PyObject * value_has_value(PyObject *self, PyObject *) noexcept {
    return PyLong_FromLong(value_bool(self));
}

bool is_subclass(PyTypeObject *o, PyTypeObject *t) {
    int x = PyObject_IsSubclass(reinterpret_cast<PyObject *>(o), reinterpret_cast<PyObject *>(t));
    return (x < 0) ? throw python_error() : x;
}

/******************************************************************************/

Object as_value(Reference const &ref, Object const &o);

Object as_list(Reference const &ref, Object const &o) {
    Object args = {PyObject_GetAttrString(+o, "__args__"), true};
    if (Debug) std::cout << "is list " << PyList_Check(+o) << std::endl;
    if (args) {
        if (!PyTuple_Check(+args))
            return type_error("expected __args__ to be a tuple");
        Py_ssize_t const len = PyObject_Length(+args);
        if (len != 1)
            return type_error("expected __args__ to be length 1");
        Object vt{PyTuple_GET_ITEM(+args, 0), true};
        auto v = downcast<Vector<Value>>(ref);
        Object list{PyList_New(v.size()), false};
        for (Py_ssize_t i = 0; i != v.size(); ++i) {
            if (Debug) std::cout << "list index " << i << std::endl;
            Object item = as_value(std::move(v[i]).reference(), vt);
            if (!item) return {};
            Py_INCREF(+item);
            PyList_SET_ITEM(+list, i, +item);
        }
        return list;
    }
    return {};
}

Object as_tuple(Reference const &ref, Object const &o) {
    Object args = {PyObject_GetAttrString(+o, "__args__"), true};
    if (args) {
        if (!PyTuple_Check(+args))
            return type_error("expected __args__ to be a tuple");
        Py_ssize_t const len = PyObject_Length(+args);
        if (len == 2 && PyTuple_GET_ITEM(+args, 1) == Py_Ellipsis) {
            Object vt{PyTuple_GET_ITEM(+args, 0), true};
            auto v = downcast<Vector<Value>>(ref);
            Object tup{PyTuple_New(v.size()), false};
            for (Py_ssize_t i = 0; i != v.size(); ++i) {
                if (Debug) std::cout << "tuple index " << i << " of " << v.size() << std::endl;
                if (!set_tuple_item(tup, i, as_value(std::move(v[i]).reference(), vt))) return {};
            }
            return tup;
        } else {
            auto v = downcast<Vector<Value>>(ref);
            Object tup{PyTuple_New(len), false};
            for (Py_ssize_t i = 0; i != len; ++i) {
                if (!set_tuple_item(tup, i, as_value(v[i].reference(), {PyTuple_GET_ITEM(+args, i), true}))) return {};
            }
            return tup;
        }
    }
    return {};
}

Object as_dict(Reference const &ref, Object const &t={}) {
    if (Debug) std::cout << "bad dict" << std::endl;
    return {};
}

template <class T>
Object any_as_wrap(T &&t) {
    Object o{PyObject_CallObject(type_object(WrapType), nullptr), false};
    cast_object<Wrap>(+o) = Value(static_cast<T &&>(t));
    return o;
}

Object as_wrap(Reference const &ref, Object const &t={}) {
    auto x = t ? +t : reinterpret_cast<PyObject *>(&WrapType);
    Object o;
    if (x == reinterpret_cast<PyObject *>(&WrapType)) {
        o = {PyObject_CallObject(x, nullptr), false};
    } else {
        o = {PyObject_CallMethod(x, "__new__", "O", x), false};
    }
    if (!o) throw python_error();
    cast_object<Wrap>(+o) = Value(ref);
    return o;
}

/******************************************************************************/

Object as_object(bool b) {return {b ? Py_True : Py_False, true};}
Object as_object(Integer i) {return {PyLong_FromLongLong(static_cast<long long>(i)), false};}
Object as_object(Real x) {return {PyFloat_FromDouble(x), false};}
Object as_object(std::string const &s) {return {PyUnicode_FromStringAndSize(s.data(), s.size()), false};}
Object as_object(std::string_view s) {return {PyUnicode_FromStringAndSize(s.data(), s.size()), false};}
Object as_object(BinaryView s) {return {PyByteArray_FromStringAndSize(reinterpret_cast<char const *>(s.data()), s.size()), false};}
Object as_object(Binary const &s) {return {PyByteArray_FromStringAndSize(reinterpret_cast<char const *>(s.data()), s.size()), false};}

Object as_object(std::type_index t) {
    Object o{PyObject_CallObject(type_object(TypeIndexType), nullptr), false};
    cast_object<std::type_index>(+o) = std::move(t);
    return o;
}

Object as_object(Function f) {
    Object o{PyObject_CallObject(type_object(FunctionType), nullptr), false};
    cast_object<Function>(+o) = std::move(f);
    return o;
}

Object as_object(Reference const &ref) {
    if (Debug) std::cout << "asking for object" << std::endl;
    if (auto v = ref.request<bool>())             return as_object(std::move(*v));
    if (auto v = ref.request<Integer>())          return as_object(std::move(*v));
    if (auto v = ref.request<Real>())             return as_object(std::move(*v));
    if (auto v = ref.request<Object>())           return std::move(*v);
    if (auto v = ref.request<std::string_view>()) return as_object(std::move(*v));
    if (auto v = ref.request<std::string>())      return as_object(std::move(*v));
    if (auto v = ref.request<Function>())         return as_object(std::move(*v));
    if (auto v = ref.request<std::type_index>())  return as_object(std::move(*v));
    if (auto v = ref.request<Binary>())           return as_object(std::move(*v));
    if (auto v = ref.request<BinaryView>())       return as_object(std::move(*v));
    if (auto v = ref.request<Vector<Reference>>())
        return map_as_tuple(std::move(*v), [](auto &&x) {return as_object(std::move(x));});
    if (auto v = ref.request<Vector<Value>>())
        return map_as_tuple(std::move(*v), [](auto &&x) {return as_object(Reference(std::move(x)));});
    return {};
}

/******************************************************************************/

Object as_bool(Reference const &ref, Object const &t={}) {
    if (auto p = ref.request<bool>()) return as_object(*p);
    return {};
}

Object as_int(Reference const &ref, Object const &t={}) {
    if (auto p = ref.request<Integer>()) return as_object(*p);
    if (Debug) std::cout << "bad int" << std::endl;
    return {};
}

Object as_float(Reference const &ref, Object const &t={}) {
    if (auto p = ref.request<double>()) return as_object(*p);
    if (auto p = ref.request<Integer>()) return as_object(*p);
    if (Debug) std::cout << "bad float" << std::endl;
    return {};
}

Object as_str(Reference const &ref, Object const &t={}) {
    if (auto p = ref.request<std::string_view>()) return as_object(std::move(*p));
    if (auto p = ref.request<std::string>()) return as_object(std::move(*p));
    if (auto p = ref.request<std::wstring_view>())
        return {PyUnicode_FromWideChar(p->data(), static_cast<Py_ssize_t>(p->size())), false};
    if (auto p = ref.request<std::wstring>())
        return {PyUnicode_FromWideChar(p->data(), static_cast<Py_ssize_t>(p->size())), false};
    return {};
}

Object as_bytes(Reference const &ref, Object const &t={}) {
    if (auto p = ref.request<BinaryView>()) return as_object(std::move(*p));
    if (auto p = ref.request<Binary>()) return as_object(std::move(*p));
    return {};
}

Object as_type_index(Reference const &ref, Object const &t={}) {
    if (auto p = ref.request<std::type_index>()) return as_object(std::move(*p));
    else return {};
}

Object as_function(Reference const &ref, Object const &t={}) {
    if (auto p = ref.request<Function>()) return as_object(std::move(*p));
    else return {};
}

Object as_value(Reference const &ref, Object const &t={}) {
    if (!PyType_Check(+t))
        return type_error("expected type object");
    auto type = reinterpret_cast<PyTypeObject *>(+t);
    Object out;
    if (Debug) std::cout << "is wrap " << is_subclass(type, &WrapType) << std::endl;
    if (+t == Py_None) return out;
    else if (type == &PyBaseObject_Type)          out = as_object(ref);
    else if (is_subclass(type, &PyBool_Type))     out = as_bool(ref, t);
    else if (is_subclass(type, &PyLong_Type))     out = as_int(ref, t);
    else if (is_subclass(type, &PyFloat_Type))    out = as_float(ref, t);
    else if (is_subclass(type, &PyUnicode_Type))  out = as_str(ref, t);
    else if (is_subclass(type, &PyBytes_Type))    out = as_bytes(ref, t);
    else if (is_subclass(type, &TypeIndexType))   out = as_type_index(ref, t);
    else if (is_subclass(type, &PyList_Type))     out = as_list(ref, t);
    else if (is_subclass(type, &PyTuple_Type))    out = as_tuple(ref, t);
    else if (is_subclass(type, &PyDict_Type))     out = as_dict(ref, t);
    else if (is_subclass(type, &WrapType))        out = as_wrap(ref, t);
    else if (is_subclass(type, &FunctionType))    out = as_function(ref, t);
    else if (is_subclass(type, &PyFunction_Type)) out = as_function(ref, t);
    else if (auto p = type_conversions.find(t); p != type_conversions.end()) {
        Object v = as_wrap(ref);
        if (!v) return type_error("bad");
        return {PyObject_CallFunctionObjArgs(+p->second, +v, nullptr), false};
    }
    if (!out) return type_error("cannot convert value to type %R", +t);
    return out;
}

/******************************************************************************/

PyObject * value_request(PyObject *self, PyObject *args) noexcept {
    PyObject *t = one_argument(args);
    if (!t) return nullptr;
    return raw_object([=]() -> Object {
        auto &w = cast_object<Wrap>(self);
        Reference ref = std::holds_alternative<Value>(w) ?
            std::get<Value>(w).reference() : std::get<WeakReference>(w).lock();
        if (t == reinterpret_cast<PyObject *>(&PyBaseObject_Type)) return as_object(ref);
        else return as_value(ref, Object(t, true));
        // std::cout << "request without type" << std::endl;
        // if ( == )
        // if (auto p = std::get_if<Value>(&w)) return as_object(Reference(*p));
        // if (Debug) std::cout << "fix reference thing" << std::endl;
        // return as_wrap(std::get<WeakReference>(w).lock());
    });
}

PyObject * value_type_index(PyObject *self, PyObject *) noexcept {
    return raw_object([=] {
        Object o{PyObject_CallObject(type_object(TypeIndexType), nullptr), false};
        cast_object<std::type_index>(+o) = std::visit([](auto const &i) {return i.type();}, cast_object<Wrap>(self));
        return o;
    });
}

PyNumberMethods WrapNumberMethods = {
    .nb_bool = static_cast<inquiry>(value_bool),
};

PyMethodDef WrapTypeMethods[] = {
    {"type",      static_cast<PyCFunction>(value_type_index), METH_NOARGS, "index it"},
    {"move_from", static_cast<PyCFunction>(move_from<Wrap>), METH_VARARGS, "move it"},
    {"copy_from", static_cast<PyCFunction>(copy_from<Wrap>), METH_VARARGS, "copy it"},
    {"has_value", static_cast<PyCFunction>(value_has_value), METH_VARARGS, "has value"},
    {"request",   static_cast<PyCFunction>(value_request),   METH_VARARGS, "request given type"},
    {nullptr, nullptr, 0, nullptr}
};

PyTypeObject WrapType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cpy.Value",
    .tp_as_number = &WrapNumberMethods,
    .tp_basicsize = sizeof(Holder<Wrap>),
    .tp_dealloc = tp_delete<Wrap>,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = "C++ class object",
    .tp_new = tp_new<Wrap>, // no init (just use default constructor)
    .tp_methods = WrapTypeMethods
};

/******************************************************************************/

Object call_overload(ErasedFunction const &fun, Object const &args, bool gil) {
    if (auto py = fun.target<PythonFunction>())
        return {PyObject_CallObject(+py->function, +args), false};
    auto pack = positional_args(args);
    if (Debug) std::cout << "constructed args from python " << pack.size();
    if (Debug) for (auto const &p : pack) std::cout << ", " << p.type().name();
    if (Debug) std::cout << std::endl;
    Value out;
    {
        auto lk = std::make_shared<PythonEntry>(!gil);
        Caller ct(lk);
        if (Debug) std::cout << "calling the args: size=" << pack.size() << std::endl;
        out = fun(ct, std::move(pack));
    }
    if (Debug) std::cout << "got the output " << out.type().name() << std::endl;
    return any_as_wrap(std::move(out));
}

PyObject * function_call(PyObject *self, PyObject *args, PyObject *kws) noexcept {
    bool gil = true;
    PyObject *s = nullptr;
    if (kws && PyDict_Check(kws)) {
        PyObject *g = PyDict_GetItemString(kws, "gil");
        if (g) gil = PyObject_IsTrue(g);
        s = PyDict_GetItemString(kws, "signature"); // either typeindex or tuple of typeindex, none

        if (0 != PyObject_Length(kws) - bool(g) - bool(s))
            return type_error("Unexpected extra keywords");
    }
    if (Debug) std::cout << "gil = " << gil << " " << Py_REFCNT(self) << Py_REFCNT(args) << std::endl;
    if (Debug) std::cout << "number of signatures " << cast_object<Function>(self).overloads.size() << std::endl;

    return cpy::raw_object([=]() -> Object {
        auto const &overloads = cast_object<Function>(self).overloads;
        if (overloads.size() == 1)
            return call_overload(overloads[0].second, {args, true}, gil);
        for (auto const &o : overloads) {
            if (Debug) std::cout << "signature " << bool(s) << std::endl;
            if (s && s != Py_None) {
                if (PyTuple_Check(s)) {
                    auto const len = PyObject_Length(s);
                    if (len > o.first.size())
                        return type_error("too many types given");
                    for (Py_ssize_t i = 0; i != len; ++i) {
                        PyObject *x = PyTuple_GET_ITEM(s, i);
                        if (x != Py_None && cast_object<std::type_index>(x) != o.first.begin()[i]) continue;
                    }
                } else if (*o.first.begin() != cast_object<std::type_index>(s)) continue;
            }
            if (Debug) std::cout << "****call overload****" << std::endl;
            try {return call_overload(o.second, {args, true}, gil);}
            catch (DispatchError const &e) {
                std::cout << "error " << e.what() << std::endl;
            }
        }
        return type_error("C++: no overloads worked");
    });
}

PyObject * function_signatures(PyObject *self, PyObject *) noexcept {
    return cpy::raw_object([=]() -> Object {
        return map_as_tuple(cast_object<Function>(self).overloads, [](auto const &p) -> Object {
            if (!p.first) return {Py_None, true};
            return map_as_tuple(p.first, [](auto const &o) {return as_object(o);});
        });
    });
}

PyMethodDef FunctionTypeMethods[] = {
    {"move_from", static_cast<PyCFunction>(move_from<Function>),   METH_VARARGS, "move it"},
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
    cast_object<Function>(self).emplace(PythonFunction(Object(fun, true), Object(sig ? sig : Py_None, true)));
    return 0;
}

PyTypeObject FunctionType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cpy.Function",
    .tp_basicsize = sizeof(Holder<Function>),
    .tp_dealloc = tp_delete<Function>,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = "C++ function object",
    .tp_new = tp_new<Function>,
    .tp_call = function_call,  // overload
    .tp_methods = FunctionTypeMethods,
    .tp_init = function_init
};

/******************************************************************************/

bool attach_type(Object const &m, char const *name, PyTypeObject *t) noexcept {
    if (PyType_Ready(t) < 0) return false;
    Py_INCREF(t);
    return PyDict_SetItemString(+m, name, reinterpret_cast<PyObject *>(t)) >= 0;
}

bool attach(Object const &m, char const *name, Object o) noexcept {
    return o && PyDict_SetItemString(+m, name, +o) >= 0;
}

/******************************************************************************/

Object initialize(Document const &doc) {
    Object m{PyDict_New(), false};
    for (auto const &p : doc.types)
        if (p.second) type_names.emplace(p.first, p.first.name());//p.second->first);


    bool ok = attach_type(m, "Value", &WrapType)
        && attach_type(m, "Function", &FunctionType)
        && attach_type(m, "TypeIndex", &TypeIndexType)
            // Tuple[Tuple[int, TypeIndex, int], ...]
        && attach(m, "scalars", map_as_tuple(scalars, [](auto const &x) {
            return args_as_tuple(as_int(Reference(static_cast<Integer>(std::get<0>(x)))),
                                 as_type_index(Reference(std::get<1>(x))),
                                 as_int(Reference(std::get<2>(x))));
        }))
            // Tuple[Tuple[TypeIndex, Tuple[Tuple[str, function], ...]], ...]
        && attach(m, "contents", map_as_tuple(doc.contents, [](auto const &x) {
            Object o;
            if (auto p = x.second.template target<Function>()) o = as_object(std::move(*p));
            else if (auto p = x.second.template target<std::type_index>()) o = as_object(std::move(*p));
            else if (auto p = x.second.template target<TypeData>()) o = args_as_tuple(
                map_as_tuple(p->methods, [](auto const &x) {return args_as_tuple(as_str(Reference(x.first)), as_object(x.second));}),
                map_as_tuple(p->data, [](auto const &x) {return args_as_tuple(as_object(x.first), any_as_wrap(x.second));})
            );
            else o = any_as_wrap(x.second);
            return args_as_tuple(as_str(Reference(x.first)), std::move(o));
        }))
        && attach(m, "set_conversion", as_object(Function().emplace([](Object t, Object o) {
            std::cout << "insert " << (t == o) << (+t == Py_None) << std::endl;
            type_conversions.insert_or_assign(std::move(t), std::move(o));
        })))
        && attach(m, "set_type_names", as_object(Function().emplace(
            [](Zip<std::type_index, std::string_view> v) {
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
            Py_INCREF(+dict);
            if (PyModule_AddObject(+mod, "document", +dict) < 0) return {};
            return mod;
        });
    }
#else
    void CPY_CAT(init, CPY_MODULE)(void) {

    }
#endif
}
