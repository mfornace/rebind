#include <rebind-python/Cast.h>
#include <rebind-python/Variable.h>
#include <numeric>

namespace rebind::py {

bool is_subclass(PyTypeObject *o, PyTypeObject *t) {
    int x = PyObject_IsSubclass(reinterpret_cast<PyObject *>(o), reinterpret_cast<PyObject *>(t));
    return (x < 0) ? throw python_error() : x;
}

/******************************************************************************/

Object type_args(Object const &o) {
    auto out = Object::from(PyObject_GetAttrString(o, "__args__"));
    if (out && !PyTuple_Check(out))
        throw python_error(type_error("expected __args__ to be a tuple"));
    return out;
}

Object type_args(Object const &o, Py_ssize_t n) {
    auto out = type_args(o);
    if (out) {
        Py_ssize_t const m = PyTuple_GET_SIZE(+out);
        if (m != n) throw python_error(type_error("expected __args__ to be length %zd (got %zd)", n, m));
    }
    return out;
}

Object list_cast(Ref &ref, Object const &o, Object const &root) {
    DUMP("Cast to list ", ref.name());
    if (auto args = type_args(o, 1)) {
        DUMP("is list ", PyList_Check(o));
        Scope s;
        if (auto p = ref.load<Sequence>(s)) {
        //     Object vt{PyTuple_GET_ITEM(+args, 0), true};
        //     auto list = Object::from(PyList_New(v.size()));
        //     for (Py_ssize_t i = 0; i != v.size(); ++i) {
        //         DUMP("list index ", i);
        //         Object item = cast_to_object(std::move(v[i]), vt, root);
        //         if (!item) return {};
        //         incref(+item);
        //         PyList_SET_ITEM(+list, i, +item);
        //     }
        //     return list;
        }
    }
    return {};
}

Object tuple_cast(Ref &ref, Object const &o, Object const &root) {
    DUMP("Cast to tuple ", ref.name());
    if (auto args = type_args(o)) {
        Py_ssize_t const len = PyTuple_GET_SIZE(+args);
        Scope s;
        if (auto v = ref.load<Sequence>(s)) {
            // if (len == 2 && PyTuple_GET_ITEM(+args, 1) == Py_Ellipsis) {
            //     Object vt{PyTuple_GET_ITEM(+args, 0), true};
            //     auto tup = Object::from(PyTuple_New(v.size()));
            //     for (Py_ssize_t i = 0; i != v.size(); ++i)
            //         if (!set_tuple_item(tup, i, cast_to_object(std::move(v[i]), vt, root))) return {};
            //     return tup;
            // } else if (len == v.size()) {
            //     auto tup = Object::from(PyTuple_New(len));
            //     for (Py_ssize_t i = 0; i != len; ++i)
            //         if (!set_tuple_item(tup, i, cast_to_object(std::move(v[i]), {PyTuple_GET_ITEM(+args, i), true}, root))) return {};
            //     return tup;
            // }
        }
    }
    return {};
}

Object dict_cast(Ref &ref, Object const &o, Scope &s, Object const &root) {
    DUMP("Cast to dict ", ref.name());
    if (auto args = type_args(o, 2)) {
        Object key{PyTuple_GET_ITEM(+args, 0), true};
        Object val{PyTuple_GET_ITEM(+args, 1), true};

        if (+key == SubClass<PyTypeObject>{&PyUnicode_Type}) {
            if (auto v = ref.load<Dictionary>(s)) {
                auto out = Object::from(PyDict_New());
                for (auto &x : *v) {
                    Object k = as_object(x.first);
                    Object v = cast_to_object(Ref(std::move(x.second)), val, root);
                    if (!k || !v || PyDict_SetItem(out, k, v)) return {};
                }
                return out;
            }
        }

        if (auto v = ref.load<Vector<std::pair<Value, Value>>>(s)) {
            auto out = Object::from(PyDict_New());
            for (auto &x : *v) {
                Object k = cast_to_object(Ref(std::move(x.first)), key, root);
                Object v = cast_to_object(Ref(std::move(x.second)), val, root);
                if (!k || !v || PyDict_SetItem(out, k, v)) return {};
            }
            return out;
        }
    }
    return {};
}

// Convert Value to a class which is a subclass of rebind.Value
Object value_to_object(Value &&v, Object const &t) {
    PyObject *x;
    if (t) x = +t;
    else if (!v.has_value()) return {Py_None, true};
    else if (auto it = python_types.find(v.index()); it != python_types.end()) x = +it->second.first;
    else x = type_object<Value>();

    auto o = Object::from((x == type_object<Value>()) ?
        PyObject_CallObject(x, nullptr) : PyObject_CallMethod(x, "__new__", "O", x));

    cast_object<Value>(o) = std::move(v);
    DUMP("made python Value ", cast_object<Value>(o).name());
    return o;
}

// Convert Ref to a class which is a subclass of rebind.Value
Object ref_to_object(Ref &&v, Object const &t) {
    PyObject *x;
    if (t) x = +t;
    else if (!v.has_value()) return {Py_None, true};
    else if (auto it = python_types.find(v.index()); it != python_types.end()) x = +it->second.second;
    else x = type_object<Ref>();

    auto o = Object::from((x == type_object<Ref>()) ?
        PyObject_CallObject(x, nullptr) : PyObject_CallMethod(x, "__new__", "O", x));

    // DUMP("making Ref ", v.name(), " ", v.has_value());
    cast_object<Ref>(o) = std::move(v);
    DUMP("made Ref ", cast_object<Ref>(o).name());//, " ", cast_object<Ref>(o).qualifier());
    return o;
}

/******************************************************************************/

Object bool_cast(Ref &ref, Scope &s) {
    DUMP("bool_cast");
    if (auto p = ref.load<bool>(s)) return as_object(*p);
    return {};//return type_error("could not convert to bool");
}

Object int_cast(Ref &ref, Scope &s) {
    if (auto p = ref.load<Integer>(s)) return as_object(*p);
    DUMP("bad int");
    return {};
}

Object float_cast(Ref &ref, Scope &s) {
    if (auto p = ref.load<double>(s)) return as_object(*p);
    if (auto p = ref.load<Integer>(s)) return as_object(*p);
    DUMP("bad float");
    return {};
}

Object str_cast(Ref &ref, Scope &s) {
    DUMP("converting ", ref.name(), " to str");
    if (auto p = ref.load<std::string_view>(s)) return as_object(std::move(*p));
    if (auto p = ref.load<std::string>(s)) return as_object(std::move(*p));
    if (auto p = ref.load<std::wstring_view>(s))
        return {PyUnicode_FromWideChar(p->data(), static_cast<Py_ssize_t>(p->size())), false};
    if (auto p = ref.load<std::wstring>(s))
        return {PyUnicode_FromWideChar(p->data(), static_cast<Py_ssize_t>(p->size())), false};
    return {};
}

Object bytes_cast(Ref &ref, Scope &s) {
    if (auto p = ref.load<BinaryView>(s)) return as_object(std::move(*p));
    if (auto p = ref.load<Binary>(s)) return as_object(std::move(*p));
    return {};
}

Object type_index_cast(Ref &ref, Scope &s) {
    if (auto p = ref.load<Index>(s)) return as_object(std::move(*p));
    else return {};
}

Object function_cast(Ref &ref, Scope &s) {
    // if (auto p = ref.load<Function>()) return as_object(std::move(*p));
    // if (auto p = ref.load<Overload>()) return as_object(Function(std::move(*p)));
    return {};
}

Object memoryview_cast(Ref &ref, Scope &s, Object const &root) {
    if (auto p = ref.load<ArrayView>(s)) {
        auto x = type_object<ArrayBuffer>();
        auto obj = Object::from(PyObject_CallObject(x, nullptr));
        cast_object<ArrayBuffer>(obj) = {std::move(*p), root};
        return Object::from(PyMemoryView_FromObject(obj));
    } else return {};
}

Object getattr(PyObject *obj, char const *name) {
    if (PyObject_HasAttrString(obj, name))
        return {PyObject_GetAttrString(obj, name), false};
    return {};
}

// condition: PyType_CheckExact(type) is false
bool is_structured_type(PyObject *type, PyObject *origin) {
    if constexpr(PythonVersion >= Version(3, 7, 0)) {
        DUMP("is_structure_type 3.7A");
        // in this case, origin may or may not be a PyTypeObject *
        return origin == +getattr(type, "__origin__");
    } else {
        // case like typing.Union: type(typing.Union[int, float] == typing.Union)
        return (+type)->ob_type == reinterpret_cast<PyTypeObject *>(origin);
    }
}

bool is_structured_type(PyObject *type, PyTypeObject *origin) {
    if constexpr(PythonVersion >= Version(3, 7, 0)) {
        DUMP("is_structure_type 3.7B");
        return reinterpret_cast<PyObject *>(origin) == +getattr(type, "__origin__");
    } else {
        // case like typing.Tuple: issubclass(typing.Tuple[int, float], tuple)
        return is_subclass(reinterpret_cast<PyTypeObject *>(type), reinterpret_cast<PyTypeObject *>(origin));
    }
}

Object union_cast(Ref &v, Object const &t, Object const &root) {
    if (auto args = type_args(t)) {
        auto const n = PyTuple_GET_SIZE(+args);
        for (Py_ssize_t i = 0; i != n; ++i) {
            Object o = cast_to_object(std::move(v), {PyTuple_GET_ITEM(+args, i), true}, root);
            if (o) return o;
            else PyErr_Clear();
        }
    }
    return {};
    // return type_error("cannot convert value to %R from type %S", +t, +type_index_cast(v.name()));
}


// Convert C++ Value to a requested Python type
// First explicit types are checked:
// None, object, bool, int, float, str, bytes, Index, list, tuple, dict, Value, Overload, memoryview
// Then, the output_conversions map is queried for Python function callable with the Value
Object try_python_cast(Ref &r, Object const &t, Object const &root) {
    DUMP("try_python_cast ", r.name(), " to type ", repr(t), " with root ", repr(root));
    Scope s;
    if (auto it = type_translations.find(t); it != type_translations.end()) {
        DUMP("type_translation found");
        return try_python_cast(r, it->second, root);
    } else if (PyType_CheckExact(+t)) {
        auto type = reinterpret_cast<PyTypeObject *>(+t);
        DUMP("is Variable ", is_subclass(type, type_object<Variable>()));
        if (+type == Py_None->ob_type || +t == Py_None)       return {Py_None, true};                        // NoneType
        else if (type == &PyBool_Type)                        return bool_cast(r, s);                // bool
        else if (type == &PyLong_Type)                        return int_cast(r, s);                 // int
        else if (type == &PyFloat_Type)                       return float_cast(r, s);               // float
        else if (type == &PyUnicode_Type)                     return str_cast(r, s);                 // str
        else if (type == &PyBytes_Type)                       return bytes_cast(r, s);               // bytes
        else if (type == &PyBaseObject_Type)                  return as_deduced_object(std::move(r));        // object
        // else if (is_subclass(type, type_object<Value>()))     return value_to_object(r, t);       // Value
        // else if (is_subclass(type, type_object<Ref>()))       return ref_to_object(std::move(r), t);         // Ref
        else if (type == type_object<Index>())                return type_index_cast(r, s);          // type(Index)
        else if (is_subclass(type, &PyFunction_Type))         return function_cast(r, s);            // Function
        else if (type == &PyMemoryView_Type)                  return memoryview_cast(r, s, root);    // memory_view
    } else {
        DUMP("Not type and not in translations");
        if (auto p = cast_if<Index>(t)) { // Index
#warning "need to add custom conversions"
            // if (auto var = r.request_to(*p))
            //     return value_to_object(std::move(var));
            auto c1 = r.name();
            auto c2 = p->name();
            return type_error("could not convert object of type %s to type %s", c1.data(), c2.data());
        }
        else if (is_structured_type(t, UnionType))     return union_cast(r, t, root);
        else if (is_structured_type(t, &PyList_Type))  return list_cast(r, t, root);       // List[T] for some T (compound type)
        else if (is_structured_type(t, &PyTuple_Type)) return tuple_cast(r, t, root);      // Tuple[Ts...] for some Ts... (compound type)
        else if (is_structured_type(t, &PyDict_Type))  return dict_cast(r, t, s, root);       // Dict[K, V] for some K, V (compound type)
        DUMP("Not one of the structure types");
    }

    DUMP("custom convert ", output_conversions.size());
    if (auto p = output_conversions.find(t); p != output_conversions.end()) {
        DUMP(" conversion ");
        // Object o = ref_to_object(std::move(r), {});
        // if (!o) return type_error("could not cast Value to Python object");
        // DUMP("calling function");
        // auto &obj = cast_object<Variable>(o).ward;
        // if (!obj) obj = root;
        // return Object::from(PyObject_CallFunctionObjArgs(+p->second, +o, nullptr));
    }

    return nullptr;
}

Object cast_to_object(Ref &&v, Object const &t, Object const &root) {
    Object out = try_python_cast(v, t, root);
    if (!out) return type_error("cannot convert value to type %R from %R", +t, +as_object(v.index()));
    return out;
}

/******************************************************************************/

}
