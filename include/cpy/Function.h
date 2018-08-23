#pragma once
#include "Signature.h"
#include "Value.h"

#include <typeindex>
#include <iostream>

namespace cpy {

/******************************************************************************/

/// Invoke a function and arguments, storing output in a Value if it doesn't return void
template <class F, class ...Ts>
Value value_invoke(F &&f, Ts &&... ts) {
    if constexpr(std::is_same_v<void, std::invoke_result_t<F, Ts...>>) {
        std::invoke(static_cast<F &&>(f), static_cast<Ts &&>(ts)...);
        return {};
    } else {
        return std::invoke(static_cast<F &&>(f), static_cast<Ts &&>(ts)...);
    }
}

/******************************************************************************/

/// Cast element i of v to type T
template <class T, std::enable_if_t<!(std::is_convertible_v<Value &, T>), int> = 0>
decltype(auto) cast_index(ArgPack &v, DispatchMessage &msg, IndexedType<T> i, unsigned int offset) {
    msg.index = i.index - offset;
    return std::visit(FromValue<T>{msg}, std::move(v[i.index - offset].var));
}

template <class T, std::enable_if_t<(std::is_convertible_v<Value &, T>), int> = 0>
T cast_index(ArgPack &v, DispatchMessage &msg, IndexedType<T> i, unsigned int offset) {
    msg.index = i.index - offset;
    return static_cast<T>(v[i.index - offset]);
}

/******************************************************************************/

template <class T>
struct NoMutable {
    static_assert(
        !std::is_lvalue_reference_v<T> ||
        std::is_const_v<std::remove_reference_t<T>>,
        "Mutable lvalue references not allowed in function signature"
    );
};

// Basic wrapper to make C++ functor into a type erased std::function
// the C++ functor must be callable with (T), (const &) or (&&) parameters
// we need to take the any class by reference...
// if args contains an Any
// the function may move the Any if it takes Value
// or the function may leave the Any alone if it takes const &

template <class F>
struct FunctionAdaptor {
    F function;

    /// Run C++ functor; logs non-ClientError and rethrows all exceptions
    Value operator()(CallingContext const &, ArgPack args) const {
        Signature<F>::apply([](auto return_type, auto ...ts) {
            (NoMutable<decltype(*ts)>(), ...);
        });
        return Signature<F>::apply([&](auto return_type, auto ...ts) {
            if (args.size() != sizeof...(ts))
                throw WrongNumber(sizeof...(ts), args.size());
            DispatchMessage msg("mismatched type");
            return value_invoke(function, cast_index(args, msg, ts, 1)...);
        });
    }
};

template <class F>
struct ContextAdaptor {
    F function;

    /// Run C++ functor; logs non-ClientError and rethrows all exceptions
    Value operator()(CallingContext &ct, ArgPack args) const {
        Signature<F>::apply([](auto return_type, auto context_type, auto ...ts) {
            (NoMutable<decltype(*ts)>(), ...);
        });
        return Signature<F>::apply([&](auto return_type, auto context_type, auto ...ts) {
            if (args.size() != sizeof...(ts))
                throw WrongNumber(sizeof...(ts), args.size());
            DispatchMessage msg("mismatched type");
            return value_invoke(function, ct, cast_index(args, msg, ts, 2)...);
        });
    }
};

template <class F, class R, class T, class ...Ts>
Function function(F f, Pack<R, T, Ts...>) {
    return std::conditional_t<std::is_convertible_v<T, CallingContext &>,
        ContextAdaptor<F>, FunctionAdaptor<F>>{std::move(f)};
}

template <class F, class R>
Function function(F f, Pack<R>) {return FunctionAdaptor<F>{std::move(f)};}

template <class F>
Function function(F f) {return function(std::move(f), Signature<F>());}

/******************************************************************************/

template <class F, class C, class ...Ts>
auto mutate(F &&f, Pack<void, C, Ts...>) {
    return [f] (no_qualifier<C> &&self, Ts ...ts) {
        f(self, static_cast<decltype(ts) &&>(ts)...);
        return Value(std::move(self));
    };
}

template <class F, class R, class C, class ...Ts, std::enable_if_t<!std::is_same_v<R, void>, int> = 0>
auto mutate(F &&f, Pack<R, C, Ts...>) {
    return [f] (no_qualifier<C> &&self, Ts ...ts) {
        auto x = f(self, static_cast<decltype(ts) &&>(ts)...);
        return Sequence::vector(std::move(self), std::move(x));
    };
}

template <class F>
auto mutate(F &&f) {return mutate(static_cast<F &&>(f), Signature<no_qualifier<F>>());}

/******************************************************************************/

template <class R, class ...Ts>
struct Construct {
    constexpr R operator()(Ts &&...ts) const {return R{static_cast<Ts>(ts)...};}
};

}
