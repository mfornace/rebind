#pragma once
#include "FunctionAdapter.h"

#include <typeindex>
#include <iostream>
#include <sstream>

namespace cpy {

using ErasedFunction = std::function<Variable(Caller, ArgPack)>;

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
    std::size_t size() const {return e - b;}
};

/******************************************************************************/

struct Function {
    Zip<ErasedSignature, ErasedFunction> overloads;

    Variable operator()(Caller c, ArgPack v) const {
        if (Debug) std::cout << "    - calling type erased Function " << std::endl;
        if (overloads.empty()) throw std::out_of_range("empty Function");
        return overloads[0].second(std::move(c), std::move(v));
    }

    Function() = default;

    explicit operator bool() const {return !overloads.empty();}

    template <class ...Ts>
    Variable operator()(Caller c, Ts &&...ts) const {
        if (Debug) std::cout << "    - calling Function " << std::endl;
        ArgPack v;
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
        overloads.emplace_back(SimpleSignature<decltype(fun)>(),
            FunctionAdaptor<(N == -1 ? 0 : SimpleSignature<decltype(fun)>::size - 1 - N), decltype(fun)>{std::move(fun)});
        return *this;
    }

    template <int N = -1, class F>
    Function && emplace(F f) && {emplace<N>(std::move(f)); return std::move(*this);}
};

/******************************************************************************/

template <class R, class ...Ts>
struct Callback {
    Caller caller;
    Function function;

    Callback(Function f, Caller c) : function(std::move(f)), caller(std::move(c)) {}

    R operator()(Ts ...ts) const {
        ArgPack pack;
        pack.reserve(sizeof...(Ts));
        (pack.emplace_back(static_cast<Ts &&>(ts)), ...);
        return downcast<R>(function(caller, std::move(pack)));
    }
};

/******************************************************************************/

/// Invoke a function and arguments, storing output in a Variable if it doesn't return void
template <class F, class ...Ts>
Variable value_invoke(F const &f, Ts &&... ts) {
    using O = std::remove_cv_t<std::invoke_result_t<F, Ts...>>;
    std::cout << "    -- making output " << typeid(Type<O>).name() << std::endl;
    if constexpr(std::is_same_v<void, O>) {
        std::invoke(f, static_cast<Ts &&>(ts)...);
        return {};
    } else return Variable(Type<O>(), std::invoke(f, static_cast<Ts &&>(ts)...));
}

template <class F, class ...Ts>
Variable caller_invoke(std::true_type, F const &f, Caller &&c, Ts &&...ts) {
    if (Debug) std::cout << "    - invoking with context" << std::endl;
    return value_invoke(f, std::move(c)(), static_cast<Ts &&>(ts)...);
}

template <class F, class ...Ts>
Variable caller_invoke(std::false_type, F const &f, Caller &&c, Ts &&...ts) {
    if (Debug) std::cout << "    - invoking context guard" << std::endl;
    auto new_frame = std::move(c)();
    if (Debug) std::cout << "    - invoked context guard" << std::endl;
    return value_invoke(f, static_cast<Ts &&>(ts)...);
}

/******************************************************************************/

/// Cast element i of v to type T
template <class T>
decltype(auto) cast_index(ArgPack const &v, Dispatch &msg, IndexedType<T> i) {
    msg.index = i.index;
    return downcast<T>(v[i.index], msg);
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

}
