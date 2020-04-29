#include <ara-py/Load.h>
#include <ara-py/Variable.h>

#include <ara/Ref.h>
#include <ara/Core.h>

#include <unordered_map>

namespace ara::py {

/******************************************************************************/

struct NoneType {
    static bool is(PyTypeObject* p) {return p == Py_None->ob_type;}
    static Object load() {return {Py_None, true};}
};


struct BoolType {
    static bool is(PyTypeObject* p) {return p == &PyBool_Type;}
    static Object load(Ref &ref) {
        DUMP("load_bool");
        if (auto p = ref.load<bool>()) return as_object(*p);
        return {};//return type_error("could not convert to bool");
    }
};


struct IntType {
    static bool is(PyTypeObject* p) {return p == &PyLong_Type;}

    static Object load(Ref &ref) {
        if (auto p = ref.load<Integer>()) return as_object(*p);
        DUMP("cannot load int");
        return {};
    }
};

struct FloatType {
    static bool is(PyTypeObject* p) {return p == &PyFloat_Type;}

    static Object load(Ref &ref) {
        if (auto p = ref.load<Float>()) return as_object(*p);
        if (auto p = ref.load<Integer>()) return as_object(*p);
        DUMP("bad float");
        return {};
    }
};

struct StrType {
    static bool is(PyTypeObject* p) {return p == &PyUnicode_Type;}

    static Object load(Ref &ref) {
        DUMP("converting", ref.name(), " to str");
        if (auto p = ref.load<std::string_view>()) return as_object(std::move(*p));
        if (auto p = ref.load<std::string>()) return as_object(std::move(*p));
        if (auto p = ref.load<std::wstring_view>())
            return {PyUnicode_FromWideChar(p->data(), static_cast<Py_ssize_t>(p->size())), false};
        if (auto p = ref.load<std::wstring>())
            return {PyUnicode_FromWideChar(p->data(), static_cast<Py_ssize_t>(p->size())), false};
        return {};
    }
};

struct BytesType {
    static bool is(PyTypeObject* p) {return p == &PyBytes_Type;}

    static Object load(Ref &ref) {
        // if (auto p = ref.load<BinaryView>()) return as_object(std::move(*p));
        // if (auto p = ref.load<Binary>()) return as_object(std::move(*p));
        return {};
    }
};

struct IndexType {
    static bool is(PyTypeObject* p) {return p == TypePtr::from<Index>();}

    static Object load(Ref &ref) {
        if (auto p = ref.load<Index>()) return as_object(std::move(*p));
        else return {};
    }
};

struct FunctionType {
    static bool is(PyTypeObject* p) {return p == &PyFunction_Type;}

    static Object load(Ref &ref) {
        // if (auto p = ref.load<Function>()) return as_object(std::move(*p));
        // if (auto p = ref.load<Overload>()) return as_object(Function(std::move(*p)));
        return {};
    }
};

struct MemoryViewType {
    static bool is(PyTypeObject* p) {return p == &PyMemoryView_Type;}

    static Object load(Ref &ref, Object const &root) {
        // if (auto p = ref.load<ArrayView>()) {
        //     auto x = TypePtr::from<ArrayBuffer>();
        //     auto obj = Object::from(PyObject_CallObject(x, nullptr));
        //     cast_object<ArrayBuffer>(obj) = {std::move(*p), root};
        //     return Object::from(PyMemoryView_FromObject(obj));
        // }
        return {};
    }
};

/******************************************************************************/

// // condition: PyType_CheckExact(type) is false
// bool is_structured_type(PyObject *type, PyObject *origin) {
//     if constexpr(Version >= decltype(Version)(3, 7, 0)) {
//         DUMP("is_structure_type 3.7A");
//         // in this case, origin may or may not be a PyTypeObject *
//         return origin == +getattr(type, "__origin__");
//     } else {
//         // case like typing.Union: type(typing.Union[int, float] == typing.Union)
//         return (+type)->ob_type == reinterpret_cast<PyTypeObject *>(origin);
//     }
// }

bool is_structured_type(PyObject *type, PyTypeObject *origin) {
    if constexpr(Version >= decltype(Version)(3, 7, 0)) {
        DUMP("is_structure_type 3.7B");
        // return reinterpret_cast<PyObject *>(origin) == +getattr(type, "__origin__");
    } else {
        // case like typing.Tuple: issubclass(typing.Tuple[int, float], tuple)
        // return is_subclass(reinterpret_cast<PyTypeObject *>(type), reinterpret_cast<PyTypeObject *>(origin));
    }
    return false;
}


struct ListType {
    static bool is(Ptr p) {return is_structured_type(p, &PyList_Type);}
    static Object load(Ref &ref, Ptr p, Object root) {return {};}
};


struct DictType {
    static bool is(Ptr p) {return is_structured_type(p, &PyDict_Type);}
    static Object load(Ref &ref, Ptr p, Object root) {return {};}
};


struct TupleType {
    static bool is(Ptr p) {return is_structured_type(p, &PyTuple_Type);}
    static Object load(Ref &ref, Ptr p, Object root) {return {};}
};


struct UnionType {
    static bool is(Ptr p) {return false;}// is_structured_type(p, &PyUnion_Type);}
    static Object load(Ref &ref, Ptr p, Object root) {return {};}
};

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

/******************************************************************************/

std::unordered_map<Object, Object> output_conversions;

Object try_load(Ref &r, Ptr t, Object root) {
    DUMP("try_load", r.name(), "to type", repr(t), "with root", repr(+root));
    // || +t == Py_None
    if (auto type = TypePtr::from(t)) {
        if (NoneType::is(type))                        return NoneType::load();              // NoneType
        else if (BoolType::is(type))                       return BoolType::load(r);                // bool
        else if (IntType::is(type))                        return IntType::load(r);                 // int
        else if (FloatType::is(type))                        return FloatType::load(r);                 // int
        else if (StrType::is(type))                        return StrType::load(r);                 // int
        else if (BytesType::is(type))                        return BytesType::load(r);                 // int
        else if (IndexType::is(type))                        return IndexType::load(r);                 // int
// //         else if (type == &PyBaseObject_Type)                  return as_deduced_object(std::move(r));        // object
        else if (type == TypePtr::from<Variable>()) return Variable::from(r, std::move(root));           // Value
    } else {
        DUMP("Not a type");
        if (auto p = cast_if<Index>(t)) { // Index
// #warning "need to add custom conversions"
            // r.load_to()
            auto c1 = r.name();
            auto c2 = p->name();
            return type_error("could not convert object of type %s to type %s", c1.data(), c2.data());
        }
        else if (UnionType::is(t)) return UnionType::load(r, t, root);
        else if (ListType::is(t))  return ListType::load(r, t, root);       // List[T] for some T (compound type)
        else if (TupleType::is(t)) return TupleType::load(r, t, root);      // Tuple[Ts...] for some Ts... (compound type)
        else if (DictType::is(t))  return DictType::load(r, t, root);       // Dict[K, V] for some K, V (compound type)
        DUMP("Not one of the structure types");
    }

    DUMP("custom convert ", output_conversions.size());
    if (auto p = output_conversions.find(Object{t, true}); p != output_conversions.end()) {
        DUMP(" conversion ");
        Object o;// = ref_to_object(std::move(r), {});
        if (!o) return type_error("could not cast value to Python object");
        DUMP("calling function");
//         // auto &obj = cast_object<Variable>(o).ward;
//         // if (!obj) obj = root;
        return Object::from(PyObject_CallFunctionObjArgs(+p->second, +o, nullptr));
    }

    return nullptr;
}

/******************************************************************************/

}