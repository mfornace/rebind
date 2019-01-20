#include <cpy/PythonCast.h>
#include <numeric>

namespace cpy {

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

Object list_cast(Variable &&ref, Object const &o, Object const &root) {
    if (auto args = type_args(o, 1)) {
        DUMP("is list ", PyList_Check(o));
        auto v = ref.cast<Sequence>();
        Object vt{PyTuple_GET_ITEM(+args, 0), true};
        auto list = Object::from(PyList_New(v.size()));
        for (Py_ssize_t i = 0; i != v.size(); ++i) {
            DUMP("list index ", i);
            Object item = python_cast(std::move(v[i]), vt, root);
            if (!item) return {};
            Py_INCREF(+item);
            PyList_SET_ITEM(+list, i, +item);
        }
        return list;
    } else return {};
}

Object tuple_cast(Variable &&ref, Object const &o, Object const &root) {
    if (auto args = type_args(o)) {
        Py_ssize_t const len = PyTuple_GET_SIZE(+args);
        auto v = ref.cast<Sequence>();
        if (len == 2 && PyTuple_GET_ITEM(+args, 1) == Py_Ellipsis) {
            Object vt{PyTuple_GET_ITEM(+args, 0), true};
            auto tup = Object::from(PyTuple_New(v.size()));
            for (Py_ssize_t i = 0; i != v.size(); ++i)
                if (!set_tuple_item(tup, i, python_cast(std::move(v[i]), vt, root))) return {};
            return tup;
        } else if (len == v.size()) {
            auto tup = Object::from(PyTuple_New(len));
            for (Py_ssize_t i = 0; i != len; ++i)
                if (!set_tuple_item(tup, i, python_cast(std::move(v[i]), {PyTuple_GET_ITEM(+args, i), true}, root))) return {};
            return tup;
        }
    }
    return {};
}

Object dict_cast(Variable &&ref, Object const &o, Object const &root) {
    if (auto args = type_args(o, 2)) {
        Object key{PyTuple_GET_ITEM(+args, 0), true};
        Object val{PyTuple_GET_ITEM(+args, 1), true};

        if (+key == TypeObject{&PyUnicode_Type}) {
            if (auto v = ref.request<Dictionary>()) {
                auto out = Object::from(PyDict_New());
                for (auto &x : *v) {
                    Object k = as_object(x.first);
                    Object v = python_cast(std::move(x.second), val, root);
                    if (!k || !v || PyDict_SetItem(out, k, v)) return {};
                }
                return out;
            }
        }

        if (auto v = ref.request<Vector<std::pair<Variable, Variable>>>()) {
            auto out = Object::from(PyDict_New());
            for (auto &x : *v) {
                Object k = python_cast(std::move(x.first), key, root);
                Object v = python_cast(std::move(x.second), val, root);
                if (!k || !v || PyDict_SetItem(out, k, v)) return {};
            }
            return out;
        }
    }
    return {};
}

// Convert Variable to a class which is a subclass of cpy.Variable
Object variable_cast(Variable &&v, Object const &t) {
    PyObject *x;
    if (t) x = +t;
    else if (auto it = python_types.find(v.type()); it != python_types.end()) x = +it->second;
    else x = type_object<Variable>();

    auto o = Object::from((x == type_object<Variable>()) ?
        PyObject_CallObject(x, nullptr) : PyObject_CallMethod(x, "__new__", "O", x));

    DUMP("making variable ", static_cast<int>(v.qualifier()), " ", v.name());
    cast_object<Variable>(o) = std::move(v);
    DUMP("made variable ", static_cast<int>(cast_object<Variable>(o).qualifier()));
    return o;
}

/******************************************************************************/

Object bool_cast(Variable &&ref) {
    if (auto p = ref.request<bool>()) return as_object(*p);
    return {};
}

Object int_cast(Variable &&ref) {
    if (auto p = ref.request<Integer>()) return as_object(*p);
    DUMP("bad int");
    return {};
}

Object float_cast(Variable &&ref) {
    if (auto p = ref.request<double>()) return as_object(*p);
    if (auto p = ref.request<Integer>()) return as_object(*p);
    DUMP("bad float");
    return {};
}

Object str_cast(Variable &&ref) {
    DUMP("converting ", ref.name(), " to str");
    DUMP(static_cast<int>(ref.qualifier()), bool(ref.action()), ref.name());
    if (auto p = ref.request<std::string_view>()) return as_object(std::move(*p));
    if (auto p = ref.request<std::string>()) return as_object(std::move(*p));
    if (auto p = ref.request<std::wstring_view>())
        return {PyUnicode_FromWideChar(p->data(), static_cast<Py_ssize_t>(p->size())), false};
    if (auto p = ref.request<std::wstring>())
        return {PyUnicode_FromWideChar(p->data(), static_cast<Py_ssize_t>(p->size())), false};
    return {};
}

Object bytes_cast(Variable &&ref) {
    if (auto p = ref.request<BinaryView>()) return as_object(std::move(*p));
    if (auto p = ref.request<Binary>()) return as_object(std::move(*p));
    return {};
}

Object type_index_cast(Variable &&ref) {
    if (auto p = ref.request<std::type_index>()) return as_object(std::move(*p));
    else return {};
}

Object function_cast(Variable &&ref) {
    if (auto p = ref.request<Function>()) return as_object(std::move(*p));
    else return {};
}

Object memoryview_cast(Variable &&ref, Object const &root) {
    if (auto p = ref.request<ArrayData>()) {
        auto x = type_object<ArrayBuffer>();
        auto obj = Object::from(PyObject_CallObject(x, nullptr));
        cast_object<ArrayBuffer>(obj) = {std::move(*p), root};
        return Object::from(PyMemoryView_FromObject(obj));
    } else return {};
}

// Convert C++ Variable to a requested Python type
// First explicit types are checked:
// None, object, bool, int, float, str, bytes, TypeIndex, list, tuple, dict, Variable, Function, memoryview
// Then, the type_conversions map is queried for Python function callable with the Variable

bool is_union(Object const &o) {
    static PyTypeObject *union_type{nullptr};
    if (!union_type) {
        auto o = Object::from(PyImport_ImportModule("typing"));
        auto u = Object::from(PyObject_GetAttrString(o, "Union"));
        union_type = (+u)->ob_type;
    }
    return (+o)->ob_type == union_type;
}

Object union_cast(Variable &&v, Object const &t, Object const &root) {
    if (auto args = type_args(t)) {
        auto const n = PyTuple_GET_SIZE(+args);
        for (Py_ssize_t i = 0; i != n; ++i) {
            Object o = python_cast(std::move(v), {PyTuple_GET_ITEM(+args, i), true}, root);
            if (o) return o;
            else PyErr_Clear();
        }
    }
    return type_error("cannot convert value to %R from type %S", +t, +type_index_cast(v.type()));
}

Object python_cast(Variable &&v, Object const &t, Object const &root) {
    if (!PyType_Check(+t)) {
        if (auto p = cast_if<std::type_index>(t)) {
            Dispatch msg;
            if (auto var = std::move(v).request_variable(msg, *p))
                return variable_cast(std::move(var));
            return type_error("could not convert object of type %s to type %s", v.name(), p->name());
        } else if (is_union(t)) {
            return union_cast(std::move(v), t, root);
        } else {
            return type_error("expected type object but got %R", (+t)->ob_type);
        }
    }
    auto type = reinterpret_cast<PyTypeObject *>(+t);
    Object out;
    DUMP("is Variable ", is_subclass(type, type_object<Variable>()));
    if (+type == Py_None->ob_type || +t == Py_None) return {Py_None, true};
    else if (type == &PyBaseObject_Type)                        out = as_deduced_object(std::move(v));
    else if (is_subclass(type, &PyBool_Type))                   out = bool_cast(std::move(v));
    else if (is_subclass(type, &PyLong_Type))                   out = int_cast(std::move(v));
    else if (is_subclass(type, &PyFloat_Type))                  out = float_cast(std::move(v));
    else if (is_subclass(type, &PyUnicode_Type))                out = str_cast(std::move(v));
    else if (is_subclass(type, &PyBytes_Type))                  out = bytes_cast(std::move(v));
    else if (is_subclass(type, type_object<std::type_index>())) out = type_index_cast(std::move(v));
    else if (is_subclass(type, &PyList_Type))                   out = list_cast(std::move(v), t, root);
    else if (is_subclass(type, &PyTuple_Type))                  out = tuple_cast(std::move(v), t, root);
    else if (is_subclass(type, &PyDict_Type))                   out = dict_cast(std::move(v), t, root);
    else if (is_subclass(type, type_object<Variable>()))        out = variable_cast(std::move(v), t);
    else if (is_subclass(type, type_object<Function>()))        out = function_cast(std::move(v));
    else if (is_subclass(type, &PyFunction_Type))               out = function_cast(std::move(v));
    else if (is_subclass(type, &PyMemoryView_Type))             out = memoryview_cast(std::move(v), root);
    else {
        DUMP("custom convert ", type_conversions.size());
        if (auto p = type_conversions.find(t); p != type_conversions.end()) {
            DUMP(" conversion ");
            Object o = variable_cast(std::move(v)); // make this contain root object
            DUMP(" did something ", bool(o));
            if (!o) return type_error("could not cast Variable to Python object");
            DUMP("calling function");
            auto &obj = static_cast<Var &>(cast_object<Variable>(o)).ward;
            if (!obj) obj = root;
            return Object::from(PyObject_CallFunctionObjArgs(+p->second, +o, nullptr));
        }
    }
    if (!out) return type_error("cannot convert value to type %R from type %S", +t, +type_index_cast(v.type()));
    return out;
}

/******************************************************************************/

}
