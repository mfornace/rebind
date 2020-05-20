#pragma once
#include "Load.h"
#include "Variable.h"

namespace ara::py {

template <class T>
struct Output;

struct NoneType;
struct BoolType;
struct IntType;
struct FloatType;
struct StrType;
struct BytesType;
struct FunctionType;
struct ListType;
struct UnionType;
struct OptionType;
struct DictType;
struct TupleType;
struct MemoryViewType;

/******************************************************************************/

template <>
struct Output<NoneType> {
    static bool matches(Instance<PyTypeObject> p) {return +p == Py_None->ob_type;}

    static Shared load(Ignore, Ignore, Ignore) {return {Py_None, true};}
};


template <>
struct Output<BoolType> {
    static bool matches(Instance<PyTypeObject> p) {return +p == &PyBool_Type;}

    static Shared load(Ref &ref, Ignore, Ignore) {
        DUMP("load_bool");
        if (auto p = ref.load<Bool>()) return as_object(*p);
        return {};//return type_error("could not convert to bool");
    }
};


template <>
struct Output<IntType> {
    static bool matches(Instance<PyTypeObject> p) {return +p == &PyLong_Type;}

    static Shared load(Ref &ref, Ignore, Ignore) {
        DUMP("loading int");
        if (auto p = ref.load<Integer>()) return as_object(*p);
        DUMP("cannot load int");
        return {};
    }
};

template <>
struct Output<FloatType> {
    static bool matches(Instance<PyTypeObject> p) {return +p == &PyFloat_Type;}

    static Shared load(Ref &ref, Ignore, Ignore) {
        if (auto p = ref.load<ara_float>()) return as_object(*p);
        if (auto p = ref.load<Integer>()) return as_object(*p);
        DUMP("bad float");
        return {};
    }
};

template <>
struct Output<StrType> {
    static bool matches(Instance<PyTypeObject> p) {return +p == &PyUnicode_Type;}

    static Shared load(Ref &ref, Ignore, Ignore) {
        DUMP("converting", ref.name(), " to str");
        if (auto p = ref.load<Str>()) return as_object(std::move(*p));
        if (auto p = ref.load<String>()) return as_object(std::move(*p));

        if (auto p = ref.load<std::wstring_view>())
            return {PyUnicode_FromWideChar(p->data(), static_cast<Py_ssize_t>(p->size())), false};
        if (auto p = ref.load<std::wstring>())
            return {PyUnicode_FromWideChar(p->data(), static_cast<Py_ssize_t>(p->size())), false};
        return {};
    }
};

template <>
struct Output<BytesType> {
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
struct Output<FunctionType> {
    static bool matches(Instance<PyTypeObject> p) {return +p == &PyFunction_Type;}

    static Shared load(Ref &ref, Ignore, Ignore) {
        // if (auto p = ref.load<FunctionType>()) return as_object(std::move(*p));
        // if (auto p = ref.load<Overload>()) return as_object(FunctionType(std::move(*p)));
        return {};
    }
};

template <>
struct Output<MemoryViewType> {
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

bool is_structured_type(Instance<> type, PyTypeObject *origin) {
    if (+type == reinterpret_cast<PyObject*>(origin)) return true;
    if constexpr(Version >= decltype(Version)(3, 7, 0)) {
        Shared attr(PyObject_GetAttrString(+type, "__origin__"), false);
        return reinterpret_cast<PyObject *>(origin) == +attr;
    } else {
        // case like typing.TupleType: issubclass(typing.TupleType[int, float], tuple)
        // return is_subclass(reinterpret_cast<PyTypeObject *>(type), reinterpret_cast<PyTypeObject *>(origin));
    }
    return false;
}

template <>
struct Output<ListType> {
    static bool matches(Instance<> p) {return is_structured_type(p, &PyList_Type);}

    static Shared load(Ref &ref, Instance<> p, Shared root) {
        // Load Array.
        // For each, load value type.
        return {};
    }
};

template <>
struct Output<DictType> {
    static bool matches(Instance<> p) {return is_structured_type(p, &PyDict_Type);}

    static Shared load(Ref &ref, Instance<> p, Shared root) {
        DUMP("loading Dict[]", ref.name());
        if (auto a = ref.load<Array>()) {
            Span &s = *a;
            if (s.rank() == 1) {
                auto out = Shared::from(PyDict_New());
                s.map([&](Ref &r) {
                    if (auto v = r.load<View>()) {
                        if (v->size() != 2) return false;
                        Shared key, value;
                        PyDict_SetItem(+out, +key, +value);
                        return true;
                    }
                    return false;
                });
            } else if (s.rank() == 2 && s.length(1) == 2) {
                auto out = Shared::from(PyDict_New());
                Shared key;
                s.map([&](Ref &r) {
                    if (key) {
                        auto value = Shared::from(PyDict_New());
                        PyDict_SetItem(+out, +key, +value);
                        key = {};
                    } else {
                        // key = r.load<>
                    }
                    return true;
                });
            }
        }
        // -
        // strategy: load Array --> gives pair<K, V>[n] --> then load each value as View --> then load each element as Key, Value
        // strategy: load Tuple --> gives Ref[n] --> then load each ref to a View --> then load each element as Key, Value
        // these are similar... main annoyance is the repeated allocation for a 2-length View...
        // strategy: load Array --> gives variant<K, V>[n, 2] --> then load each value. hmm. not great for compile time. // bad
        // hmm, this seems unfortunate. Maybe Tuple[] should actually have multiple dimensions?:
        // then: load Tuple --> gives Ref[n, 2] --> load each element as Key, Value
        // problem with this is that load Tuple, should it return ref(pair)[N] or ref[N, 2] ... depends on the context which is better.
        // other possibility is to just declare different dimension types ... sigh, gets nasty.
        // other alternative is to make a map type which is like Tuple[N, 2].
        return {};
    }
};

template <>
struct Output<TupleType> {
    static bool matches(Instance<> p) {return is_structured_type(p, &PyTuple_Type);}

    static Shared load(Ref &ref, Instance<> p, Shared root) {
        // load Tuple or View, go through and load each type. straightforward.
        return {};
    }
};

template <>
struct Output<UnionType> {
    static bool matches(Instance<> p) {return false;}// is_structured_type(p, &PyUnion_Type);}

    static Shared load(Ref &ref, Instance<> p, Shared root) {
        // try loading each possibility. straightforward.
        return {};
    }
};

template <>
struct Output<OptionType> {
    static bool matches(Instance<> p) {return false;}// is_structured_type(p, &PyUnion_Type);}

    static Shared load(Ref &ref, Instance<> p, Shared root) {
        // if !ref return none
        // else return load .. hmm needs some thinking
        return {};
    }
};


template <>
struct Output<Variable> {
    static bool matches(Instance<PyTypeObject> p) {return false;}

    static Shared load(Ref &ref, Ignore, Ignore) {
        return {};
    }
};

/******************************************************************************/

template <class F>
Shared map_output(Instance<> t, F &&f) {
    // Type objects
    if (PyType_CheckExact(+t)) {
        auto type = t.as<PyTypeObject>();
        if (Output<NoneType>::matches(type))  return f(Output<NoneType>());
        if (Output<BoolType>::matches(type))  return f(Output<BoolType>());
        if (Output<IntType>::matches(type))   return f(Output<IntType>());
        if (Output<FloatType>::matches(type)) return f(Output<FloatType>());
        if (Output<StrType>::matches(type))   return f(Output<StrType>());
        if (Output<BytesType>::matches(type)) return f(Output<BytesType>());
        if (Output<Index>::matches(type)) return f(Output<Index>());
// //         else if (type == &PyBaseObject_Type)                  return as_deduced_object(std::move(r));        // object
        if (type == static_type<Variable>()) return f(Output<Variable>());           // Value
    }
    // Non type objects
    DUMP("Not a type");
    if (auto p = cast_if<Index>(+t)) return f(Output<Index>());  // Index

    if (Output<UnionType>::matches(t)) return f(Output<UnionType>());
    if (Output<ListType>::matches(t))  return f(Output<ListType>());       // ListType[T] for some T (compound type)
    if (Output<TupleType>::matches(t)) return f(Output<TupleType>());      // TupleType[Ts...] for some Ts... (compound type)
    if (Output<DictType>::matches(t))  return f(Output<DictType>());       // DictType[K, V] for some K, V (compound type)
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