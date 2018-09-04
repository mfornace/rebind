#pragma once
#include "Signature.h"
#include "Value.h"

#include <typeindex>
#include <iostream>
#include <sstream>

namespace cpy {

using Function = std::function<Value(Caller, ArgPack)>;

/******************************************************************************/

/// Invoke a function and arguments, storing output in a Value if it doesn't return void
template <class F, class ...Ts>
Value value_invoke(F const &f, Ts &&... ts) {
    if constexpr(std::is_same_v<void, std::invoke_result_t<F, Ts...>>) {
        std::invoke(f, static_cast<Ts &&>(ts)...);
        return {};
    } else {
        return std::invoke(f, static_cast<Ts &&>(ts)...);
    }
}

template <class F, class C, class ...Ts>
Value context_invoke(std::true_type, F const &f, C &&c, Ts &&...ts) {
    return value_invoke(f, static_cast<C &&>(c), static_cast<Ts &&>(ts)...);
}

template <class F, class C, class ...Ts>
Value context_invoke(std::false_type, F const &f, C &&c, Ts &&...ts) {
    return value_invoke(f, static_cast<Ts &&>(ts)...);
}

/******************************************************************************/

/// Cast element i of v to type T
template <class T>
decltype(auto) cast_index(ArgPack &v, Dispatch &msg, IndexedType<T> i) {
    msg.index = i.index;
    return value_cast<T>(std::move(v[i.index]), msg);
}

/******************************************************************************/

template <class T>
struct CheckType {
    static_assert(
        true ||
        !std::is_lvalue_reference_v<T> ||
        std::is_const_v<std::remove_reference_t<T>>,
        "Mutable lvalue references are not allowed in function signature"
    );
};

// Basic wrapper to make C++ functor into a type erased std::function
// the C++ functor must be callable with (T), (const &) or (&&) parameters
// we need to take the any class by reference...
// if args contains an Any
// the function may move the Any if it takes Value
// or the function may leave the Any alone if it takes const &

template <int N, class R, class ...Ts, std::enable_if_t<N == 1, int> = 0>
Pack<Ts...> skip_head(Pack<R, Ts...>);

template <int N, class R, class C, class ...Ts, std::enable_if_t<N == 2, int> = 0>
Pack<Ts...> skip_head(Pack<R, C, Ts...>);

template <class C, class R>
std::false_type has_head(Pack<R>);

template <class C, class R, class T, class ...Ts>
std::is_convertible<T, C> has_head(Pack<R, T, Ts...>);

template <std::size_t N, class F>
struct FunctionAdaptor {
    F function;
    using Ctx = decltype(has_head<Caller>(Signature<F>()));
    using Sig = decltype(skip_head<1 + int(Ctx::value)>(Signature<F>()));

    template <class P>
    void call_each(P, Value &out, Caller &c, Dispatch &msg, ArgPack &args) const {
        P::indexed([&](auto ...ts) {
            out = context_invoke(Ctx(), function, c, cast_index(args, msg, ts)...);
        });
    }

    template <std::size_t ...Is>
    Value call(ArgPack &args, Caller &c, Dispatch &msg, std::index_sequence<Is...>) const {
        Value out;
        ((args.size() == N - Is - 1 ? call_each(Sig::template slice<0, N - Is - 1>(), out, c, msg, args) : void()), ...);
        return out;
    }

    Value operator()(Caller c, ArgPack args) const {
        Sig::apply2([](auto ...ts) {
            (CheckType<decltype(*ts)>(), ...);
        });
        Dispatch msg;
        if (args.size() == Sig::size)
            return Sig::indexed([&](auto ...ts) {
                return context_invoke(Ctx(), function, c, cast_index(args, msg, ts)...);
            });
        if (args.size() < Sig::size - N)
            throw WrongNumber(Sig::size - N, args.size());
        if (args.size() > Sig::size)
            throw WrongNumber(Sig::size, args.size());
        return call(args, c, msg, std::make_index_sequence<N>());
    }
};

template <class F>
struct FunctionAdaptor<0, F> {
    F function;
    using Ctx = decltype(has_head<Caller>(Signature<F>()));
    using Sig = decltype(skip_head<1 + int(Ctx::value)>(Signature<F>()));

    /// Run C++ functor; logs non-ClientError and rethrows all exceptions
    Value operator()(Caller c, ArgPack args) const {
        Sig::apply2([](auto ...ts) {
            (CheckType<decltype(*ts)>(), ...);
        });
        if (args.size() != Sig::size)
            throw WrongNumber(Sig::size, args.size());
        return Sig::indexed([&](auto ...ts) {
            Dispatch msg;
            return context_invoke(Ctx(), function, c, cast_index(args, msg, ts)...);
        });
    }
};

/******************************************************************************/

template <class F, class=void>
struct Simplify {
    constexpr std::decay_t<F> operator()(F f) const {return f;}
};

template <class F>
struct Simplify<F, std::void_t<decltype(false ? nullptr : std::declval<F>())>> {
    constexpr auto operator()(F f) const {return false ? nullptr : f;}
};

/******************************************************************************/

template <int N = -1, class F>
Function make_function(F f) {
    auto fun = Simplify<F>()(std::move(f));
    return FunctionAdaptor<(N == -1 ? 0 : Signature<decltype(fun)>::size - 1 - N), decltype(fun)>{std::move(fun)};
}

/******************************************************************************/

template <class R, class ...Ts>
struct Construct {
    constexpr R operator()(Ts &&...ts) const {return R{static_cast<Ts>(ts)...};}
};

template <class ...Ts, class R>
Construct<R, Ts...> construct(Type<R> t) {return {};}


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

template <class R, class ...Ts>
class Callback {
    Function fun;

    Callback(Function f, Caller &c) : fun(std::move(f), caller(&c)) {}

    R operator()(Ts &&...ts) const {
        ArgPack pack;
        pack.reserve(sizeof...(Ts));
        (pack.emplace_back(static_cast<Ts &&>(ts)), ...);
        return value_cast<R>()(fun(caller, std::move(pack)));
    }

private:
    Caller *caller;
};

/******************************************************************************/

}
