/**
 * @brief Python-related C++ source code for cpy
 * @file Python.cc
 */
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
PyObject * var_assign(PyObject *self, PyObject *args) noexcept {
    if (PyObject *value = one_argument(args)) {
        return raw_object([=] {
            DUMP("- moving variable");
            cast_object<Var>(self).assign(variable_from_object({value, true}));
            return Object(self, true);
        });
    } else return nullptr;
}

/******************************************************************************/

PyObject *type_index_new(PyTypeObject *subtype, PyObject *, PyObject *) noexcept {
    PyObject* o = subtype->tp_alloc(subtype, 0); // 0 unused
    if (o) new (&cast_object<std::type_index>(o)) std::type_index(typeid(void)); // noexcept
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

template <>
PyTypeObject Holder<std::type_index>::type = {
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
    PyObject *value = one_argument(args);
    if (!value) return nullptr;
    return raw_object([=] {
        cast_object<T>(self) = cast_object<T>(value); // not notexcept
        return Object(Py_None, true);
    });
}

int value_bool(PyObject *self) noexcept {
    if (auto v = cast_if<Variable>(self)) return v->has_value();
    else return PyObject_IsTrue(self);
}

PyObject * var_has_value(PyObject *self, PyObject *) noexcept {
    return PyLong_FromLong(value_bool(self));
}

bool is_subclass(PyTypeObject *o, PyTypeObject *t) {
    int x = PyObject_IsSubclass(reinterpret_cast<PyObject *>(o), reinterpret_cast<PyObject *>(t));
    return (x < 0) ? throw python_error() : x;
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
    auto o = Object::from(PyObject_CallObject(type_object<std::type_index>(), nullptr));
    cast_object<std::type_index>(o) = std::move(t);
    return o;
}

Object as_object(Function f) {
    auto o = Object::from(PyObject_CallObject(type_object<Function>(), nullptr));
    cast_object<Function>(o) = std::move(f);
    return o;
}

/// Source driven conversion: guess the correct Python type from the source type
/// I guess this is where automatic class conversions should be done?
Object as_object(Variable &&ref) {
    DUMP("asking for object");
    if (!ref) return {Py_None, true};
    if (auto v = ref.request<Object>())           return std::move(*v);
    if (auto v = ref.request<Real>())             return as_object(std::move(*v));
    if (auto v = ref.request<Integer>())          return as_object(std::move(*v));
    if (auto v = ref.request<bool>())             return as_object(std::move(*v));
    if (auto v = ref.request<std::string_view>()) return as_object(std::move(*v));
    if (auto v = ref.request<std::string>())      return as_object(std::move(*v));
    if (auto v = ref.request<Function>())         return as_object(std::move(*v));
    if (auto v = ref.request<std::type_index>())  return as_object(std::move(*v));
    if (auto v = ref.request<Binary>())           return as_object(std::move(*v));
    if (auto v = ref.request<BinaryView>())       return as_object(std::move(*v));
    if (auto v = ref.request<Sequence>())
        return map_as_tuple(std::move(*v), [](auto &&x) {return as_object(std::move(x));});
    return {};
}

/******************************************************************************/

Object as_python(Variable &&ref, Object const &o);

Object type_args(Object const &o) {
    Object out = {PyObject_GetAttrString(o, "__args__"), true};
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

Object as_list(Variable &&ref, Object const &o) {
    if (auto args = type_args(o, 1)) {
        DUMP("is list ", PyList_Check(o));
        auto v = ref.cast<Sequence>();
        Object vt{PyTuple_GET_ITEM(+args, 0), true};
        auto list = Object::from(PyList_New(v.size()));
        for (Py_ssize_t i = 0; i != v.size(); ++i) {
            DUMP("list index ", i);
            Object item = as_python(std::move(v[i]), vt);
            if (!item) return {};
            Py_INCREF(+item);
            PyList_SET_ITEM(+list, i, +item);
        }
        return list;
    } else return {};
}

Object as_tuple(Variable &&ref, Object const &o) {
    if (auto args = type_args(o)) {
        Py_ssize_t const len = PyTuple_GET_SIZE(+args);
        auto v = ref.cast<Sequence>();
        if (len == 2 && PyTuple_GET_ITEM(+args, 1) == Py_Ellipsis) {
            Object vt{PyTuple_GET_ITEM(+args, 0), true};
            auto tup = Object::from(PyTuple_New(v.size()));
            for (Py_ssize_t i = 0; i != v.size(); ++i)
                if (!set_tuple_item(tup, i, as_python(std::move(v[i]), vt))) return {};
            return tup;
        } else if (len == v.size()) {
            auto tup = Object::from(PyTuple_New(len));
            for (Py_ssize_t i = 0; i != len; ++i)
                if (!set_tuple_item(tup, i, as_python(std::move(v[i]), {PyTuple_GET_ITEM(+args, i), true}))) return {};
            return tup;
        }
    }
    return {};
}

Object as_dict(Variable &&ref, Object const &o) {
    if (auto args = type_args(o, 2)) {
        Object key{PyTuple_GET_ITEM(+args, 0), true};
        Object val{PyTuple_GET_ITEM(+args, 1), true};

        if (+key == TypeObject{&PyUnicode_Type}) {
            if (auto v = ref.request<Dictionary>()) {
                auto out = Object::from(PyDict_New());
                for (auto &x : *v) {
                    Object k = as_object(x.first);
                    Object v = as_python(std::move(x.second), val);
                    if (!k || !v || PyDict_SetItem(out, k, v)) return {};
                }
                return out;
            }
        }

        if (auto v = ref.request<Vector<std::pair<Variable, Variable>>>()) {
            auto out = Object::from(PyDict_New());
            for (auto &x : *v) {
                Object k = as_python(std::move(x.first), key);
                Object v = as_python(std::move(x.second), val);
                if (!k || !v || PyDict_SetItem(out, k, v)) return {};
            }
            return out;
        }
    }
    return {};
}

// Convert Variable to a class which is a subclass of cpy.Variable
Object as_variable(Variable &&v, Object const &t) {
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

Object as_bool(Variable &&ref, Object const &t={}) {
    if (auto p = ref.request<bool>()) return as_object(*p);
    return {};
}

Object as_int(Variable &&ref, Object const &t={}) {
    if (auto p = ref.request<Integer>()) return as_object(*p);
    DUMP("bad int");
    return {};
}

Object as_float(Variable &&ref, Object const &t={}) {
    if (auto p = ref.request<double>()) return as_object(*p);
    if (auto p = ref.request<Integer>()) return as_object(*p);
    DUMP("bad float");
    return {};
}

Object as_str(Variable &&ref, Object const &t={}) {
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

Object as_bytes(Variable &&ref, Object const &t={}) {
    if (auto p = ref.request<BinaryView>()) return as_object(std::move(*p));
    if (auto p = ref.request<Binary>()) return as_object(std::move(*p));
    return {};
}

Object as_type_index(Variable &&ref, Object const &t={}) {
    if (auto p = ref.request<std::type_index>()) return as_object(std::move(*p));
    else return {};
}

Object as_function(Variable &&ref, Object const &t={}) {
    if (auto p = ref.request<Function>()) return as_object(std::move(*p));
    else return {};
}

Object as_memory(Variable &&ref, Object const &t={}) {
    if (auto p = ref.request<ArrayData>()) {
        Py_buffer view;
        view.buf = p->data;
        view.obj = nullptr;
        view.itemsize = Buffer::itemsize(p->type);
        if (p->shape.empty()) view.len = 0;
        else view.len = std::accumulate(p->shape.begin(), p->shape.end(), view.itemsize, std::multiplies<>());
        view.readonly = !p->mutate;
        view.format = const_cast<char *>(Buffer::format(p->type).data());
        view.ndim = p->shape.size();
        std::vector<Py_ssize_t> shape(p->shape.begin(), p->shape.end());
        std::vector<Py_ssize_t> strides(p->strides.begin(), p->strides.end());
        for (auto &s : strides) s *= view.itemsize;
        view.shape = shape.data();
        view.strides = strides.data();
        view.suboffsets = nullptr;
        return {PyMemoryView_FromBuffer(&view), false};
    } else return {};
}

// Convert C++ Variable to a requested Python type
// First explicit types are checked:
// None, object, bool, int, float, str, bytes, TypeIndex, list, tuple, dict, Variable, Function, memoryview
// Then, the type_conversions map is queried for Python function callable with the Variable

Object as_python(Variable &&v, Object const &t={}) {
    auto const name = v.name();
    if (auto p = cast_if<std::type_index>(t)) {
        std::cout << "not done" << std::endl;
        // if (auto var = std::move(v).request(*p)) return as_variable(std::move(var));
        // return type_error("could not convert object of type %s to type %s", v.name(), p->name());
    }
    if (!PyType_Check(+t))
        return type_error("expected type object but got %R", (+t)->ob_type);
    auto type = reinterpret_cast<PyTypeObject *>(+t);
    Object out;
    DUMP("is wrap ", is_subclass(type, type_object<Variable>()));
    if (+type == Py_None->ob_type || +t == Py_None) return {Py_None, true};
    else if (type == &PyBaseObject_Type)                        out = as_object(std::move(v));
    else if (is_subclass(type, &PyBool_Type))                   out = as_bool(std::move(v), t);
    else if (is_subclass(type, &PyLong_Type))                   out = as_int(std::move(v), t);
    else if (is_subclass(type, &PyFloat_Type))                  out = as_float(std::move(v), t);
    else if (is_subclass(type, &PyUnicode_Type))                out = as_str(std::move(v), t);
    else if (is_subclass(type, &PyBytes_Type))                  out = as_bytes(std::move(v), t);
    else if (is_subclass(type, type_object<std::type_index>())) out = as_type_index(std::move(v), t);
    else if (is_subclass(type, &PyList_Type))                   out = as_list(std::move(v), t);
    else if (is_subclass(type, &PyTuple_Type))                  out = as_tuple(std::move(v), t);
    else if (is_subclass(type, &PyDict_Type))                   out = as_dict(std::move(v), t);
    else if (is_subclass(type, type_object<Variable>()))        out = as_variable(std::move(v), t);
    else if (is_subclass(type, type_object<Function>()))        out = as_function(std::move(v), t);
    else if (is_subclass(type, &PyFunction_Type))               out = as_function(std::move(v), t);
    else if (is_subclass(type, &PyMemoryView_Type))             out = as_memory(std::move(v), t);
    else {
        DUMP("custom convert ", type_conversions.size());
        if (auto p = type_conversions.find(t); p != type_conversions.end()) {
            DUMP(" conversion ");
            Object o = as_variable(std::move(v));
            DUMP(" did something ", bool(o));
            if (!o) return type_error("bad");
            DUMP("calling function");
            return Object::from(PyObject_CallFunctionObjArgs(+p->second, +o, nullptr));
        }
    }
    if (!out) return type_error("cannot convert value to type %R from type %S", +t, +as_type_index(v.type()));
    return out;
}

/******************************************************************************/

PyObject * var_cast(PyObject *self, PyObject *args) noexcept {
    PyObject *t = one_argument(args);
    if (!t) return nullptr;
    return raw_object([=]() -> Object {
        auto &v = cast_object<Variable>(self);
        if (t == reinterpret_cast<PyObject *>(&PyBaseObject_Type)) return as_object(std::move(v));
        else return as_python(std::move(v), Object(t, true));
    });
}

PyObject * var_type(PyObject *self, PyObject *) noexcept {
    return raw_object([=] {
        auto o = Object::from(PyObject_CallObject(type_object<std::type_index>(), nullptr));
        cast_object<std::type_index>(o) = cast_object<Variable>(self).type();
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

PyNumberMethods VarNumberMethods = {
    .nb_bool = static_cast<inquiry>(value_bool),
};

PyMethodDef VarMethods[] = {
    {"assign",        static_cast<PyCFunction>(var_assign),        METH_VARARGS, "assign from other using C++ move constructor"},
    {"copy_from",     static_cast<PyCFunction>(copy_from<Var>),    METH_VARARGS, "assign from other using C++ copy constructor"},
    {"address",       static_cast<PyCFunction>(var_address),       METH_NOARGS,  "get C++ pointer address"},
    {"_ward",         static_cast<PyCFunction>(var_ward),          METH_NOARGS,  "get ward object"},
    {"_set_ward",     static_cast<PyCFunction>(var_set_ward),      METH_VARARGS, "set ward object and return self"},
    {"qualifier",     static_cast<PyCFunction>(var_qualifier),     METH_NOARGS,  "return qualifier of self"},
    {"is_stack_type", static_cast<PyCFunction>(var_is_stack_type), METH_NOARGS, "return if object is held in stack storage"},
    {"type",          static_cast<PyCFunction>(var_type),          METH_NOARGS,  "return TypeIndex of the held C++ object"},
    {"has_value",     static_cast<PyCFunction>(var_has_value),     METH_VARARGS, "return if a C++ object is being held"},
    {"cast",          static_cast<PyCFunction>(var_cast),          METH_VARARGS, "cast to a given Python type"},
    {nullptr, nullptr, 0, nullptr}
};

template <>
PyTypeObject Holder<Var>::type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cpy.Variable",
    .tp_as_number = &VarNumberMethods,
    .tp_basicsize = sizeof(Holder<Var>),
    .tp_dealloc = tp_delete<Var>,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = "C++ class object",
    .tp_new = tp_new<Var>, // no init (just use default constructor)
    .tp_methods = VarMethods
};

/******************************************************************************/

Object call_overload(ErasedFunction const &fun, Object const &args, bool gil) {
    if (auto py = fun.target<PythonFunction>())
        return {PyObject_CallObject(+py->function, +args), false};
    auto pack = args_from_python(args);
    DUMP("constructed python args ", pack.size());
    for (auto const &p : pack) DUMP(p.type().name());
    Variable out;
    {
        auto lk = std::make_shared<PythonFrame>(!gil);
        Caller ct(lk);
        DUMP("calling the args: size=", pack.size());
        out = fun(ct, std::move(pack));
    }
    DUMP("got the output ", out.type().name(), int(out.qualifier()));
    if (auto p = out.target<Object &>()) return std::move(*p);
    if (auto p = out.target<PyObject &>()) DUMP("fff");
    // if (auto p = out.target<PyObject * &>()) return {*p, true};
    // Convert the C++ Variable to a cpy.Variable
    return as_variable(std::move(out));
}

PyObject * not_none(PyObject *o) {return o == Py_None ? nullptr : o;}

PyObject * function_call(PyObject *self, PyObject *args, PyObject *kws) noexcept {
    bool gil = true;
    std::optional<std::type_index> t0, t1;
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
        if (r) t0 = cast_object<std::type_index>(r);
        if (f) t1 = cast_object<std::type_index>(f);
    }
    DUMP("gil = ", gil, " ", Py_REFCNT(self), Py_REFCNT(args));
    DUMP("number of signatures ", cast_object<Function>(self).overloads.size());

    return raw_object([=]() -> Object {
        auto const &overloads = cast_object<Function>(self).overloads;
        if (overloads.size() == 1)
            return call_overload(overloads[0].second, {args, true}, gil);

        if (sig && PyLong_Check(sig)) {
            auto i = PyLong_AsLongLong(sig);
            if (i < 0) i += overloads.size();
            if (i <= overloads.size() || i < 0)
                return call_overload(overloads[i].second, {args, true}, gil);
            PyErr_SetString(PyExc_IndexError, "signature index out of bounds");
            return Object();
        }
        auto const n = PyTuple_GET_SIZE(args);
        Variable *first = n ? cast_if<Variable>(PyTuple_GET_ITEM(args, 0)) : nullptr;

        auto list = Object::from(PyList_New(0));

        // maybe we should check for equivalence on the first argument first?
        for (auto const exact : {true, false}) {
            for (auto const &o : overloads) {
                bool const match = (o.first.size() < 2) || (first && first->type() == o.first[1]);
                if (match != exact) continue;
                DUMP("signature ", bool(sig));
                if (sig) {
                    if (PyTuple_Check(sig)) { // beginning list of arguments
                        auto const len = PyObject_Length(sig);
                        if (len > o.first.size())
                            return type_error("C++: too many types given in signature");
                        for (Py_ssize_t i = 0; i != len; ++i) {
                            PyObject *x = PyTuple_GET_ITEM(sig, i);
                            if (x != Py_None && cast_object<std::type_index>(x) != o.first[i]) continue;
                        }
                    } else return type_error("C++: expected 'signature' to be a tuple");
                } else if (t0) {
                    if (o.first.size() > 0 && o.first[0] != t0) continue; // assumed to be return type
                } else if (t1) {
                    if (o.first.size() > 1 && o.first[1] != t1) continue; // assumed to be return type
                }

                DUMP("**** call overload ****");
                try {return call_overload(o.second, {args, true}, gil);}
                catch (WrongType const &e) {
                    if (PyList_Append(+list, +as_object(wrong_type_message(e)))) return {};
                } catch (WrongNumber const &e) {
                    unsigned int n0 = e.expected, n = e.received;
                    auto s = Object::from(PyUnicode_FromFormat("C++: wrong number of arguments (expected %u, got %u)", n0, n));
                    if (PyList_Append(+list, +s)) return {};
                } catch (DispatchError const &e) {
                    if (PyList_Append(+list, +as_object(std::string_view(e.what())))) return {};
                }
            }
        }


        // make a list of the messages
        return type_error("C++: no overloads worked %S", +list); // HERE
    });
}

PyObject * function_signatures(PyObject *self, PyObject *) noexcept {
    return raw_object([=]() -> Object {
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
PyTypeObject Holder<Function>::type = {
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
    return o && PyDict_SetItemString(m, name, o) >= 0;
}

/******************************************************************************/

Object initialize(Document const &doc) {
    auto m = Object::from(PyDict_New());
    for (auto const &p : doc.types)
        if (p.second) type_names.emplace(p.first, p.first.name());//p.second->first);


    bool ok = attach_type(m, "Variable", type_object<Variable>())
        && attach_type(m, "Function", type_object<Function>())
        && attach_type(m, "TypeIndex", type_object<std::type_index>())
            // Tuple[Tuple[int, TypeIndex, int], ...]
        && attach(m, "scalars", map_as_tuple(scalars, [](auto const &x) {
            return args_as_tuple(as_object(static_cast<Integer>(std::get<0>(x))),
                                 as_object(std::get<1>(x)),
                                 as_object(static_cast<Integer>(std::get<2>(x))));
        }))
            // Tuple[Tuple[TypeIndex, Tuple[Tuple[str, function], ...]], ...]
        && attach(m, "contents", map_as_tuple(doc.contents, [](auto const &x) {
            Object o;
            if (auto p = x.second.template target<Function const &>()) o = as_object(*p);
            else if (auto p = x.second.template target<std::type_index const &>()) o = as_object(*p);
            else if (auto p = x.second.template target<TypeData const &>()) o = args_as_tuple(
                map_as_tuple(p->methods, [](auto const &x) {return args_as_tuple(as_object(x.first), as_object(x.second));}),
                map_as_tuple(p->data, [](auto const &x) {return args_as_tuple(as_object(x.first), as_variable(Variable(x.second)));})
            );
            else o = as_variable(Variable(x.second));
            return args_as_tuple(as_object(x.first), std::move(o));
        }))
        && attach(m, "set_conversion", as_object(Function::of([](Object t, Object o) {
            DUMP("insert type conversion ", (t == o), (+t == Py_None));
            type_conversions.insert_or_assign(std::move(t), std::move(o));
            DUMP("inserted type conversion ");
        })))
        && attach(m, "_finalize", as_object(Function::of([] {
            type_conversions.clear();
        })))
        && attach(m, "set_debug", as_object(Function::of([](bool b) {return std::exchange(Debug, b);})))
        && attach(m, "debug", as_object(Function::of([] {return Debug;})))
        && attach(m, "set_type", as_object(Function::of([](std::type_index idx, Object o) {
            python_types.emplace(std::move(idx), std::move(o));
        })))
        && attach(m, "set_type_names", as_object(Function::of(
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
