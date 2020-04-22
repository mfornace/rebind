#include <ara-py/Load.h>
#include <ara/Ref.h>
#include <ara/Scope.h>
#include <ara-py/Variable.h>

namespace ara::py {

/******************************************************************************/

Object load_bool(Ref &ref, Scope &s) {
    DUMP("load_bool");
    if (auto p = ref.load<bool>(s)) return as_object(*p);
    return {};//return type_error("could not convert to bool");
}


struct Int {
    static bool is(PyTypeObject* p) {return p == &PyLong_Type;}

    static Object load(Ref &ref) {
        if (auto p = ref.load<Integer>(s)) return as_object(*p);
        DUMP("bad int");
        return {};
    }
};

Object load_float(Ref &ref, Scope &s) {
    if (auto p = ref.load<Float>(s)) return as_object(*p);
    if (auto p = ref.load<Integer>(s)) return as_object(*p);
    DUMP("bad float");
    return {};
}

Object load_str(Ref &ref, Scope &s) {
    DUMP("converting ", ref.name(), " to str");
    if (auto p = ref.load<std::string_view>(s)) return as_object(std::move(*p));
    if (auto p = ref.load<std::string>(s)) return as_object(std::move(*p));
    if (auto p = ref.load<std::wstring_view>(s))
        return {PyUnicode_FromWideChar(p->data(), static_cast<Py_ssize_t>(p->size())), false};
    if (auto p = ref.load<std::wstring>(s))
        return {PyUnicode_FromWideChar(p->data(), static_cast<Py_ssize_t>(p->size())), false};
    return {};
}

Object load_bytes(Ref &ref, Scope &s) {
    // if (auto p = ref.load<BinaryView>(s)) return as_object(std::move(*p));
    // if (auto p = ref.load<Binary>(s)) return as_object(std::move(*p));
    return {};
}

Object load_index(Ref &ref, Scope &s) {
    if (auto p = ref.load<Index>(s)) return as_object(std::move(*p));
    else return {};
}

Object load_function(Ref &ref, Scope &s) {
    // if (auto p = ref.load<Function>()) return as_object(std::move(*p));
    // if (auto p = ref.load<Overload>()) return as_object(Function(std::move(*p)));
    return {};
}

Object load_memoryview(Ref &ref, Scope &s, Object const &root) {
    // if (auto p = ref.load<ArrayView>(s)) {
    //     auto x = TypePtr::from<ArrayBuffer>();
    //     auto obj = Object::from(PyObject_CallObject(x, nullptr));
    //     cast_object<ArrayBuffer>(obj) = {std::move(*p), root};
    //     return Object::from(PyMemoryView_FromObject(obj));
    // }
    return {};
}

/******************************************************************************/

std::string repr(Ptr o) {
    if (!o) return "null";
    Object out(PyObject_Repr(o), false);
    if (!out) throw PythonError();
#   if PY_MAJOR_VERSION > 2
        return PyUnicode_AsUTF8(+out); // PyErr_Clear
#   else
        return PyString_AsString(+out);
#   endif
}

struct None {
    static bool is(PyTypeObject* p) {return p == Py_None->ob_type;}
    static Object load(Ref &);
};

struct Bool {
    static bool is(PyTypeObject* p) {return p == &PyBool_Type;}
    static Object load(Ref &);
};

/******************************************************************************/

Object try_load(Ref &r, Ptr t, Ptr root) {
    DUMP("try_load ", r.name(), " to type ", repr(t), " with root ", repr(root));

    Scope s;
    // || +t == Py_None
    if (false) {
//     // if (auto it = type_translations.find(t); it != type_translations.end()) {
//         // DUMP("type_translation found");
//         // return try_python_cast(r, it->second, root);
    } else if (auto type = TypePtr::from(t)) {
        if (None::is(type))                            return {Py_None, true};                // NoneType
        else if (Bool::is(type))                       return Bool::load(r);                // bool
        else if (Int::is(type))                        return Int::load(r);                 // int
//         else if (type == &PyFloat_Type)                          return load_float(r, s);               // float
//         else if (type == &PyUnicode_Type)                        return load_str(r, s);                 // str
//         else if (type == &PyBytes_Type)                          return load_bytes(r, s);               // bytes
// //         else if (type == &PyBaseObject_Type)                  return as_deduced_object(std::move(r));        // object
//         else if (type.derives(type, TypePtr::from<Variable>())) return Variable::from(r, t);           // Value
        // else if (type == TypePtr::from<Index>())                return load_index(r, s);          // type(Index)
//         else if (type.derives(type, &PyFunction_Type))        return load_function(r, s);            // Function
        // else if (type == &PyMemoryView_Type)                  return load_memoryview(r, s, root);    // memory_view
//     } else {
//         DUMP("Not type and not in translations");
//         if (auto p = cast_if<Index>(t)) { // Index
// #warning "need to add custom conversions"
//             // if (auto var = r.request_to(*p))
//             //     return value_to_object(std::move(var));
//             auto c1 = r.name();
//             auto c2 = p->name();
//             return type_error("could not convert object of type %s to type %s", c1.data(), c2.data());
//         }
//         else if (is_structured_type(t, UnionType))     return union_cast(r, t, root);
//         else if (is_structured_type(t, &PyList_Type))  return list_cast(r, t, root);       // List[T] for some T (compound type)
//         else if (is_structured_type(t, &PyTuple_Type)) return tuple_cast(r, t, root);      // Tuple[Ts...] for some Ts... (compound type)
//         else if (is_structured_type(t, &PyDict_Type))  return dict_cast(r, t, s, root);       // Dict[K, V] for some K, V (compound type)
//         DUMP("Not one of the structure types");
//     }

//     DUMP("custom convert ", output_conversions.size());
//     if (auto p = output_conversions.find(t); p != output_conversions.end()) {
//         DUMP(" conversion ");
//         // Object o = ref_to_object(std::move(r), {});
//         // if (!o) return type_error("could not cast Value to Python object");
//         // DUMP("calling function");
//         // auto &obj = cast_object<Variable>(o).ward;
//         // if (!obj) obj = root;
//         // return Object::from(PyObject_CallFunctionObjArgs(+p->second, +o, nullptr));
    }

    return nullptr;
}

/******************************************************************************/

}