#pragma once
#include "Load.h"
#include "Variable.h"

namespace ara::py {

template <class T>
struct Output;

struct None;
struct Bool;
struct Int;
struct Float;
struct Str;
struct Bytes;
struct Function;
struct List;
struct Union;
struct Dict;
struct Tuple;
struct MemoryView;

/******************************************************************************/

template <>
struct Output<None> {
    static bool matches(Instance<PyTypeObject> p) {return +p == Py_None->ob_type;}

    static Shared load(Ignore, Ignore, Ignore) {return {Py_None, true};}
};


template <>
struct Output<Bool> {
    static bool matches(Instance<PyTypeObject> p) {return +p == &PyBool_Type;}

    static Shared load(Ref &ref, Ignore, Ignore) {
        DUMP("load_bool");
        if (auto p = ref.load<bool>()) return as_object(*p);
        return {};//return type_error("could not convert to bool");
    }
};


template <>
struct Output<Int> {
    static bool matches(Instance<PyTypeObject> p) {return +p == &PyLong_Type;}

    static Shared load(Ref &ref, Ignore, Ignore) {
        DUMP("loading int");
        if (auto p = ref.load<Integer>()) return as_object(*p);
        DUMP("cannot load int");
        return {};
    }
};

template <>
struct Output<Float> {
    static bool matches(Instance<PyTypeObject> p) {return +p == &PyFloat_Type;}

    static Shared load(Ref &ref, Ignore, Ignore) {
        if (auto p = ref.load<ara_float>()) return as_object(*p);
        if (auto p = ref.load<Integer>()) return as_object(*p);
        DUMP("bad float");
        return {};
    }
};

template <>
struct Output<Str> {
    static bool matches(Instance<PyTypeObject> p) {return +p == &PyUnicode_Type;}

    static Shared load(Ref &ref, Ignore, Ignore) {
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

template <>
struct Output<Bytes> {
    static bool matches(Instance<PyTypeObject> p) {return +p == &PyBytes_Type;}

    static Shared load(Ref &ref, Ignore, Ignore) {
        // if (auto p = ref.load<BinaryView>()) return as_object(std::move(*p));
        // if (auto p = ref.load<Binary>()) return as_object(std::move(*p));
        return {};
    }
};

template <>
struct Output<Index> {
    static bool matches(Instance<PyTypeObject> p) {return p == static_type<Index>();}

    static Shared load(Ref &ref, Ignore, Ignore) {
        if (auto p = ref.load<Index>()) return as_object(std::move(*p));

        // auto c1 = r.name();
        // auto c2 = p->name();
        // throw PythonError(type_error("could not convert object of type %s to type %s", c1.data(), c2.data()));

        else return {};
    }
};

template <>
struct Output<Function> {
    static bool matches(Instance<PyTypeObject> p) {return +p == &PyFunction_Type;}

    static Shared load(Ref &ref, Ignore, Ignore) {
        // if (auto p = ref.load<Function>()) return as_object(std::move(*p));
        // if (auto p = ref.load<Overload>()) return as_object(Function(std::move(*p)));
        return {};
    }
};

template <>
struct Output<MemoryView> {
    static bool matches(Instance<PyTypeObject> p) {return +p == &PyMemoryView_Type;}

    static Shared load(Ref &ref, Shared const &root) {
        // if (auto p = ref.load<ArrayView>()) {
        //     auto x = TypePtr::from<ArrayBuffer>();
        //     auto obj = Shared::from(PyObject_CallObject(x, nullptr));
        //     cast_object<ArrayBuffer>(obj) = {std::move(*p), root};
        //     return Shared::from(PyMemoryView_FromObject(obj));
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

bool is_structured_type(Instance<> type, PyTypeObject *origin) {
    if constexpr(Version >= decltype(Version)(3, 7, 0)) {
        DUMP("is_structure_type 3.7B");
        // return reinterpret_cast<PyObject *>(origin) == +getattr(type, "__origin__");
    } else {
        // case like typing.Tuple: issubclass(typing.Tuple[int, float], tuple)
        // return is_subclass(reinterpret_cast<PyTypeObject *>(type), reinterpret_cast<PyTypeObject *>(origin));
    }
    return false;
}

template <>
struct Output<List> {
    static bool matches(Instance<> p) {return is_structured_type(p, &PyList_Type);}

    static Shared load(Ref &ref, Instance<> p, Shared root) {return {};}
};

template <>
struct Output<Dict> {
    static bool matches(Instance<> p) {return is_structured_type(p, &PyDict_Type);}

    static Shared load(Ref &ref, Instance<> p, Shared root) {return {};}
};

template <>
struct Output<Tuple> {
    static bool matches(Instance<> p) {return is_structured_type(p, &PyTuple_Type);}

    static Shared load(Ref &ref, Instance<> p, Shared root) {return {};}
};

template <>
struct Output<Union> {
    static bool matches(Instance<> p) {return false;}// is_structured_type(p, &PyUnion_Type);}

    static Shared load(Ref &ref, Instance<> p, Shared root) {return {};}
};


template <>
struct Output<Variable> {
    static bool matches(Instance<PyTypeObject> p) {return +p == &PyBool_Type;}

    static Shared load(Ref &ref, Ignore, Ignore) {
        DUMP("load_bool");
        if (auto p = ref.load<bool>()) return as_object(*p);
        return {};//return type_error("could not convert to bool");
    }
};

/******************************************************************************/

template <class F>
Shared map_output(Instance<> t, F &&f) {
    // Type objects
    if (PyType_CheckExact(+t)) {
        auto type = t.reinterpret<PyTypeObject>();
        if (Output<None>::matches(type))  return f(Output<None>());
        if (Output<Bool>::matches(type))  return f(Output<Bool>());
        if (Output<Int>::matches(type))   return f(Output<Int>());
        if (Output<Float>::matches(type)) return f(Output<Float>());
        if (Output<Str>::matches(type))   return f(Output<Str>());
        if (Output<Bytes>::matches(type)) return f(Output<Bytes>());
        if (Output<Index>::matches(type)) return f(Output<Index>());
// //         else if (type == &PyBaseObject_Type)                  return as_deduced_object(std::move(r));        // object
        if (type == static_type<Variable>()) return f(Output<Variable>());           // Value
    }
    // Non type objects
    DUMP("Not a type");
    if (auto p = cast_if<Index>(+t)) return f(Output<Index>());  // Index

    if (Output<Union>::matches(t)) return f(Output<Union>());
    if (Output<List>::matches(t))  return f(Output<List>());       // List[T] for some T (compound type)
    if (Output<Tuple>::matches(t)) return f(Output<Tuple>());      // Tuple[Ts...] for some Ts... (compound type)
    if (Output<Dict>::matches(t))  return f(Output<Dict>());       // Dict[K, V] for some K, V (compound type)
    DUMP("Not one of the structure types");
    return Shared();
//     DUMP("custom convert ", output_conversions.size());
//     if (auto p = output_conversions.find(Shared{t, true}); p != output_conversions.end()) {
//         DUMP(" conversion ");
//         Shared o;// = ref_to_object(std::move(r), {});
//         if (!o) return type_error("could not cast value to Python object");
//         DUMP("calling function");
// //         // auto &obj = cast_object<Variable>(o).ward;
// //         // if (!obj) obj = root;
//         return Shared::from(PyObject_CallFunctionObjArgs(+p->second, +o, nullptr));
//     }
}

}