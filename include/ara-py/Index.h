#pragma once
#include <ara/Index.h>
#include "Raw.h"
#include "Wrap.h"
#include "Builtins.h"

namespace ara::py {

/******************************************************************************/


template <bool B, class T, std::size_t I>
std::conditional_t<B, Always<T>, Maybe<T>> parse_each(Always<pyTuple> args, Maybe<pyDict> kws, char const* name) {
    if (I < size(args)) return Always<T>::from(item(args, I));
    if (kws) if (auto p = item(*kws, name)) return Always<T>::from(*p);
    if constexpr(B) throw PythonError::type("bad");
    else return {};
}

template <std::size_t N, class ...Ts, std::size_t ...Is>
auto parse(Always<pyTuple> args, Maybe<pyDict> kws, std::array<char const*, sizeof...(Ts)> const &names, std::index_sequence<Is...>) {
    return std::make_tuple(parse_each<(Is < N), Ts, Is>(args, kws, names[Is])...);
}

template <std::size_t N, class ...Ts>
auto parse(Always<pyTuple> args, Maybe<pyDict> kws, std::array<char const*, sizeof...(Ts)> const &names) {
    return parse<N, Ts...>(args, kws, names, std::make_index_sequence<sizeof...(Ts)>());
}

/******************************************************************************/

template <class T, class U>
bool compare_bool(T const& t, U const& u, int op) {
    switch (op) {
        case Py_EQ: return t == u;
        case Py_NE: return t != u;
        case Py_LT: return t < u;
        case Py_GT: return t > u;
        case Py_LE: return t <= u;
        case Py_GE: return t >= u;
        default: return false;
    }
}

template <class T>
Value<> compare(Always<T> self, Always<> other, int op) {
    if (auto o = Maybe<T>(other))
        return pyBool::from(compare_bool(*self, *o, op));
    else return Always<>(*Py_NotImplemented);
}

/******************************************************************************/

struct pyIndex : StaticType<pyIndex> {
    using type = Subtype<Index>;

    static long hash(Always<pyIndex> o) noexcept {
        return static_cast<long>(std::hash<Index>()(*o));
    }

    static Value<> repr(Always<pyIndex> o) noexcept {
        return {PyUnicode_FromFormat("Index('%s')", o->name().data()), false};
    }

    static Value<> str(Always<pyIndex> o) {
        return Value<>::take(PyUnicode_FromString(o->name().data()));//, false};
    }

    static void placement_new(Index &t, Always<pyTuple>, Maybe<pyDict>) noexcept {
        new (&t) Index();
    }

    static void placement_new(Index &t, Index i) noexcept {
        new (&t) Index(i);
    }

    static int as_bool(Always<pyIndex> i) noexcept {return bool(*i);}

    static Value<pyInt> as_int(Always<pyIndex> i) {return pyInt::from(i->integer());}

    static PyNumberMethods number_methods;
    static PyMethodDef methods[];

    static void initialize_type(Always<pyType> o) noexcept;

    static Value<pyIndex> load(Ref &ref, Ignore, Ignore) {return {};}

    static Value<> call(Index, Always<pyTuple>, CallKeywords&&);

    static Value<pyIndex> from_address(Always<> addr) {
        std::uintptr_t i = view_underlying(Always<pyInt>::from(addr));
        return Value<pyIndex>::new_from(reinterpret_cast<ara_index>(i));
    }

    static Value<> forward(Always<pyIndex>, Always<pyTuple> args, Maybe<pyDict> kws) {
        return {};
    }

    static Value<pyIndex> from_library(Always<pyType>, Always<pyTuple> args, Maybe<pyDict> kws) {
        auto [file, name] = parse<2, pyStr, pyStr>(args, kws, {"file", "function"});
        auto ctypes = Value<>::take(PyImport_ImportModule("ctypes"));
        auto cdll = Value<>::take(PyObject_GetAttrString(~ctypes, "CDLL"));
        auto lib = Value<>::take(PyObject_CallFunctionObjArgs(~cdll, ~file, nullptr));
        auto fun = Value<>::take(PyObject_GetAttr(~lib, ~name));
        auto void_p = Value<>::take(PyObject_GetAttrString(~ctypes, "c_void_p"));
        if (PyObject_SetAttrString(~fun, "restype", ~void_p)) throw PythonError();
        auto addr = Value<>::take(PyObject_CallFunctionObjArgs(~fun, nullptr));
        return from_address(*addr);
    }
};

/******************************************************************************/

}