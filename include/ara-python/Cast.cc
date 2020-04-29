#include <ara-python/Cast.h>
#include <ara-python/Variable.h>
#include <numeric>

namespace ara::py {

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

/******************************************************************************/

Object getattr(PyObject *obj, char const *name) {
    if (PyObject_HasAttrString(obj, name))
        return {PyObject_GetAttrString(obj, name), false};
    return {};
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



Object cast_to_object(Ref &&v, Object const &t, Object const &root) {
    Object out = try_python_cast(v, t, root);
    if (!out) return type_error("cannot convert value to type %R from %R", +t, +as_object(v.index()));
    return out;
}

/******************************************************************************/

}
