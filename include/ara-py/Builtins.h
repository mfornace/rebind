#pragma once
#include "Wrap.h"
#include <ara/Structs.h>
// #include "Variable.h"

namespace ara::py {

/******************************************************************************/

struct pyNone : Wrap<pyNone> {
    static Always<pyType> def() {return *Py_None->ob_type;}

    static bool matches(Always<pyType> p) {return +p == Py_None->ob_type;}

    static bool check(Always<> o) {return ~o == Py_None;}

    static Value<pyNone> load(Ignore, Ignore, Ignore) {return Always<pyNone>(*Py_None);}
};

/******************************************************************************/

struct pyBool : Wrap<pyBool> {
    static Always<pyType> def() {return PyBool_Type;}

    static bool check(Always<> o) {return PyBool_Check(+o);}
    static bool matches(Always<pyType> p) {return +p == &PyBool_Type;}

    static Value<pyBool> from(bool b) {return {b ? Py_True : Py_False, true};}

    static Value<pyBool> load(Ref &ref, Ignore, Ignore) {
        DUMP("load_bool");
        if (auto p = ref.get<Bool>()) return from(bool(*p));
        return {};
    }
};


/******************************************************************************/

struct pyInt : Wrap<pyInt> {
    static Always<pyType> def() {return PyLong_Type;}

    static bool check(Always<> o) {return PyLong_Check(+o);}
    static bool matches(Always<pyType> p) {return +p == &PyLong_Type;}

    static Value<pyInt> from(Integer i) {return Value<pyInt>::take(PyLong_FromLongLong(static_cast<long long>(i)));}

    static Value<pyInt> load(Ref &ref, Ignore, Ignore) {
        DUMP("loading int");
        if (auto p = ref.get<Integer>()) return from(*p);
        DUMP("cannot load int");
        return {};
    }
};

inline Integer view_underlying(Always<pyInt> i) noexcept {return PyLong_AsLongLong(~i);}

/******************************************************************************/

struct pyFloat : Wrap<pyFloat> {

    static Always<pyType> def() {return PyFloat_Type;}

    static bool check(Always<> o) {return PyFloat_Check(+o);}
    static bool matches(Always<pyType> p) {return +p == &PyFloat_Type;}

    static Value<pyFloat> from(Float x) {return Value<pyFloat>::take(PyFloat_FromDouble(x));}

    static Value<pyFloat> load(Ref &ref, Ignore, Ignore) {
        if (auto p = ref.get<Float>()) return from(*p);
        if (auto p = ref.get<Integer>()) return from(static_cast<double>(*p));
        DUMP("bad float");
        return {};
    }
};

/******************************************************************************/

struct pyStr : Wrap<pyStr> {

    static Always<pyType> def() {return PyUnicode_Type;}

    static bool check(Always<> o) {return PyUnicode_Check(+o);}
    static bool matches(Always<pyType> p) {return +p == &PyUnicode_Type;}

    static Value<pyStr> from(Str s) {return Value<pyStr>::take(PyUnicode_FromStringAndSize(s.data(), s.size()));}

    static Value<pyStr> load(Ref &ref, Ignore, Ignore) {
        DUMP("converting", ref.name(), " to str");
        if (auto p = ref.get<Str>()) return from(std::move(*p));
        if (auto p = ref.get<String>()) return from(std::move(*p));

        // if (auto p = ref.get<std::wstring_view>())
        //     return {PyUnicode_FromWideChar(p->data(), static_cast<Py_ssize_t>(p->size())), false};
        // if (auto p = ref.get<std::wstring>())
        //     return {PyUnicode_FromWideChar(p->data(), static_cast<Py_ssize_t>(p->size())), false};
        return {};
    }
};

/******************************************************************************/

inline Str view_underlying(Always<pyStr> o) {
    Py_ssize_t size;
#   if PY_MAJOR_VERSION > 2
        char const *c = PyUnicode_AsUTF8AndSize(~o, &size);
#   else
        char *c;
        if (PyString_AsStringAndSize(~o, &c, &size)) throw PythonError();
#   endif
    if (!c) throw PythonError();
    return Str(c, size);
}

/******************************************************************************/

struct pyBytes : Wrap<pyBytes> {

    static bool check(Always<> o) {return PyBytes_Check(+o);}
    static bool matches(Always<pyType> p) {return +p == &PyBytes_Type;}

    static Value<pyBytes> load(Ref &ref, Ignore, Ignore) {
        // if (auto p = ref.get<BinaryView>()) return as_object(std::move(*p));
        // if (auto p = ref.get<Binary>()) return as_object(std::move(*p));
        return {};
    }
};

/******************************************************************************/

inline Str view_underlying(Always<pyBytes> o) {
    char *c;
    Py_ssize_t size;
    PyBytes_AsStringAndSize(~o, &c, &size);
    if (!c) throw PythonError();
    return Str(c, size);
}

/******************************************************************************/

template <class T>
inline std::string_view as_string_view(T t) {
    if (auto s = Maybe<pyStr>(t)) return view_underlying(*s);
    if (auto s = Maybe<pyBytes>(t)) return view_underlying(*s);
    throw PythonError::type("Expected str or bytes");
}

/******************************************************************************/

template <class T>
std::ostream& operator<<(std::ostream& os, Ptr<T> const& o) {
    if (!o) return os << "null";
    auto obj = Value<pyStr>::take(PyObject_Str(~o));
    return os << as_string_view(*obj);
}

/******************************************************************************/

struct pyFunction : Wrap<pyFunction> {

    static bool matches(Always<pyType> p) {return +p == &PyFunction_Type;}
    static bool check(Always<> p) {return PyFunction_Check(+p);}

    static Value<pyFunction> load(Ref &ref, Ignore, Ignore) {
        // if (auto p = ref.get<Function>()) return as_object(std::move(*p));
        // if (auto p = ref.get<Overload>()) return as_object(Function(std::move(*p)));
        return {};
    }
};

struct pyMemoryView : Wrap<pyMemoryView> {

    static bool matches(Always<pyType> p) {return +p == &PyMemoryView_Type;}
    static bool check(Always<> p) {return PyMemoryView_Check(+p);}

    static Value<pyMemoryView> load(Ref &ref, Value<> const &root) {
        // if (auto p = ref.get<ArrayView>()) {
        //     auto x = TypePtr::from<ArrayBuffer>();
        //     auto obj = Value<>::from(PyObject_CallObject(x, nullptr));
        //     cast_object<ArrayBuffer>(obj) = {std::move(*p), root};
        //     return Value<>::from(PyMemoryView_FromObject(obj));
        // }
        return {};
    }
};

/******************************************************************************/

bool is_structured_type(Always<> def, PyTypeObject *origin) {
    if (+def == reinterpret_cast<PyObject*>(origin)) return true;
    if constexpr(Version >= decltype(Version)(3, 7, 0)) {
        Value<> attr(PyObject_GetAttrString(+def, "__origin__"), false);
        return reinterpret_cast<PyObject *>(origin) == +attr;
    } else {
        // case like typing.pyTuple: issubclass(typing.pyTuple[int, float], tuple)
        // return is_subclass(reinterpret_cast<PyTypeObject *>(def), reinterpret_cast<PyTypeObject *>(origin));
    }
    return false;
}

struct pyList : Wrap<pyList> {
    static bool check(Always<> p) {return PyList_Check(+p);}

    static bool matches(Always<> p) {return is_structured_type(p, &PyList_Type);}

    static Value<> load(Ref &ref, Always<> p, Maybe<> root) {
        // Load Array.
        // For each, load value def.
        return {};
    }
};

struct pyDict : Wrap<pyDict> {
    static bool check(Always<> p) {return PyDict_Check(+p);}

    static bool matches(Always<> p) {return is_structured_type(p, &PyDict_Type);}

    static Value<> load(Ref &ref, Always<> p, Maybe<> root) {
        DUMP("loading pyDict[]", ref.name());
        if (auto a = ref.get<Array>()) {
            Span &s = *a;
            if (s.rank() == 1) {
                auto out = Value<>::take(PyDict_New());
                s.map([&](Ref &r) {
                    if (auto v = r.get<View>()) {
                        if (v->size() != 2) return false;
                        Value<> key, value;
                        PyDict_SetItem(+out, +key, +value);
                        return true;
                    }
                    return false;
                });
            } else if (s.rank() == 2 && s.length(1) == 2) {
                auto out = Value<>::take(PyDict_New());
                Value<> key;
                s.map([&](Ref &r) {
                    if (key) {
                        auto value = Value<>::take(PyDict_New());
                        PyDict_SetItem(+out, +key, +value);
                        key = {};
                    } else {
                        // key = r.get<>
                    }
                    return true;
                });
            }
        }
        // -
        // strategy: load Array --> gives pair<K, V>[n] --> then load each value as View --> then load each element as Key, Value
        // strategy: load pyTuple --> gives Ref[n] --> then load each ref to a View --> then load each element as Key, Value
        // these are similar... main annoyance is the repeated allocation for a 2-length View...
        // strategy: load Array --> gives variant<K, V>[n, 2] --> then load each value. hmm. not great for compile time. // bad
        // hmm, this seems unfortunate. Maybe pyTuple[] should actually have multiple dimensions?:
        // then: load pyTuple --> gives Ref[n, 2] --> load each element as Key, Value
        // problem with this is that load pyTuple, should it return ref(pair)[N] or ref[N, 2] ... depends on the context which is better.
        // other possibility is to just declare different dimension types ... sigh, gets nasty.
        // other alternative is to make a map def which is like pyTuple[N, 2].
        return {};
    }
};

inline Maybe<> item(Always<pyDict> t, char const* s) {return PyDict_GetItemString(~t, s);}

template <class U>
inline Maybe<> item(Always<pyDict> t, Always<U> key) {return PyDict_GetItem(~t, ~key);}

/******************************************************************************/

template <class F>
void iterate(Always<pyDict> o, F &&f) {
    PyObject *key, *value;
    Py_ssize_t pos = 0;

    while (PyDict_Next(~o, &pos, &key, &value))
        f(Always<>(*key), Always<>(*value));
}

/******************************************************************************/

struct pyTuple : Wrap<pyTuple> {
    static bool check(Always<> p) {return PyTuple_Check(+p);}

    static bool matches(Always<> p) {return is_structured_type(p, &PyTuple_Type);}

    static Value<> load(Ref &ref, Always<> p, Maybe<> root) {
        // load pyTuple or View, go through and load each def. straightforward.
        return {};
    }
};

/******************************************************************************/

inline Always<> item(Always<pyTuple> t, Py_ssize_t i) {return *PyTuple_GET_ITEM(~t, i);}
inline auto size(Always<pyTuple> t) {return PyTuple_GET_SIZE(~t);}

Always<> item_at(Always<pyTuple> t, Py_ssize_t i) {
    return (i < size(t)) ? item(t, i) : throw PythonError::type("out of bounds");
}

/******************************************************************************/

struct pyUnion : Wrap<pyUnion> {
    static bool matches(Always<> p) {return false;}// is_structured_type(p, &PyUnion_Type);}

    static Value<> load(Ref &ref, Always<> p, Maybe<> root) {
        // try loading each possibility. straightforward.
        return {};
    }
};

struct pyOption : Wrap<pyOption> {
    static bool matches(Always<> p) {return false;}// is_structured_type(p, &PyUnion_Type);}

    static Value<> load(Ref &ref, Always<> p, Maybe<> root) {
        // if !ref return none
        // else return load .. hmm needs some thinking
        return {};
    }
};

/******************************************************************************/

}