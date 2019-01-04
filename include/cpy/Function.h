#pragma once
#include "Adapter.h"

#include <typeindex>
#include <iostream>
#include <sstream>

namespace cpy {

using ErasedFunction = std::function<Variable(Caller, Sequence)>;

template <class R, class ...Ts>
static std::type_index const signature_types[] = {typeid(R), typeid(Ts)...};

/******************************************************************************/

struct ErasedSignature {
    std::type_index const *b = nullptr;
    std::type_index const *e = nullptr;
public:
    ErasedSignature() = default;

    template <class ...Ts>
    ErasedSignature(Pack<Ts...>) : b(std::begin(signature_types<Ts...>)), e(std::end(signature_types<Ts...>)) {}

    explicit operator bool() const {return b;}
    auto begin() const {return b;}
    auto end() const {return e;}
    std::type_index operator[](std::size_t i) const {return b[i];}
    std::size_t size() const {return e - b;}
};

/******************************************************************************/

struct Function {
    Zip<ErasedSignature, ErasedFunction> overloads;

    Variable operator()(Caller c, Sequence v) const {
        DUMP("    - calling type erased Function ");
        if (overloads.empty()) return {}; //throw std::out_of_range("empty Function");
        return overloads[0].second(std::move(c), std::move(v));
    }

    Function() = default;

    explicit operator bool() const {return !overloads.empty();}

    template <class ...Ts>
    Variable operator()(Caller c, Ts &&...ts) const {
        DUMP("    - calling Function ");
        Sequence v;
        v.reserve(sizeof...(Ts));
        (v.emplace_back(static_cast<Ts &&>(ts)), ...);
        return (*this)(std::move(c), std::move(v));
    }

    Function & emplace(ErasedFunction f, ErasedSignature const &s) & {
        overloads.emplace_back(s, std::move(f));
        return *this;
    }


    Function && emplace(ErasedFunction f, ErasedSignature const &s) && {
        overloads.emplace_back(s, std::move(f));
        return std::move(*this);
    }

    /******************************************************************************/

    template <int N = -1, class F>
    Function & emplace(F f) & {
        auto fun = SimplifyFunction<F>()(std::move(f));
        constexpr std::size_t n = N == -1 ? 0 : SimpleSignature<decltype(fun)>::size - 1 - N;
        overloads.emplace_back(SimpleSignature<decltype(fun)>(), Adapter<n, decltype(fun)>{std::move(fun)});
        return *this;
    }

    template <int N = -1, class F>
    Function && emplace(F f) && {emplace<N>(std::move(f)); return std::move(*this);}
};

/******************************************************************************/

template <class R, class ...Ts>
struct AnnotatedCallback {
    Caller caller;
    Function function;

    AnnotatedCallback(Function f, Caller c) : function(std::move(f)), caller(std::move(c)) {}

    R operator()(Ts ...ts) const {
        Sequence pack;
        pack.reserve(sizeof...(Ts));
        (pack.emplace_back(static_cast<Ts &&>(ts)), ...);
        return function(caller, std::move(pack)).downcast(Type<R>());
    }
};

template <class R>
struct Callback {
    Caller caller;
    Function function;

    Callback(Function f, Caller c) : function(std::move(f)), caller(std::move(c)) {}

    template <class ...Ts>
    R operator()(Ts &&...ts) const {
        Sequence pack;
        pack.reserve(sizeof...(Ts));
        (pack.emplace_back(static_cast<Ts &&>(ts)), ...);
        return function(caller, std::move(pack)).downcast(Type<R>());
    }
};

/******************************************************************************/

/// Cast element i of v to type T
template <class T>
decltype(auto) cast_index(Sequence const &v, Dispatch &msg, IndexedType<T> i) {
    msg.index = i.index;
    return v[i.index].downcast(msg, Type<T>());
}

/******************************************************************************/

template <class R, class ...Ts>
struct Construct {
    constexpr R operator()(Ts ...ts) const {return R(static_cast<Ts &&>(ts)...);}
};

template <class R, class ...Ts>
struct BraceConstruct {
    constexpr R operator()(Ts ...ts) const {return R{static_cast<Ts &&>(ts)...};}
};

template <class ...Ts, class R>
constexpr auto construct(Type<R>) {
    return std::conditional_t<std::is_constructible_v<R, Ts...>,
        Construct<R, Ts...>, BraceConstruct<R, Ts...>>();
}

template <class T>
struct Streamable {
    std::string operator()(T const &t) const {
        std::ostringstream os;
        os << t;
        return os.str();
    }
};

template <class T>
Streamable<T> streamable(Type<T> t={}) {return {};}

/******************************************************************************/

template <class R>
struct Request<Callback<R>> {
    std::optional<Callback<R>> operator()(Variable const &v, Dispatch &msg) const {
        if (!msg.caller) msg.error("Calling context expired", v.type(), typeid(Callback<R>));
        else if (auto p = v.request<Function>(msg)) return Callback<R>{std::move(*p), msg.caller};
        return {};
    }
};


template <class R, class ...Ts>
struct Request<AnnotatedCallback<R, Ts...>> {
    using type = AnnotatedCallback<R, Ts...>;
    std::optional<type> operator()(Variable const &v, Dispatch &msg) const {
        if (!msg.caller) msg.error("Calling context expired", v.type(), typeid(type));
        else if (auto p = v.request<Function>(msg)) return type{std::move(*p), msg.caller};
        return {};
    }
};

/******************************************************************************/

}
