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
template <class T>
T cast_index(ArgPack &v, IndexedType<T> i, unsigned int offset) {
    return std::visit(FromValue<T>(), std::move(v[i.index - offset].var));
}

/// Check that element i of v can be cast to type T
template <class T>
bool check_cast_index(ArgPack &v, IndexedType<T> i, unsigned int offset) {
    return std::visit([](auto const &x) {return FromValue<T>().check(x);}, v[i.index - offset].var);
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
    Value operator()(CallingContext const &, ArgPack &args) const {
        Signature<F>::apply([](auto return_type, auto ...ts) {
            (NoMutable<decltype(*ts)>(), ...);
        });
        return Signature<F>::apply([&](auto return_type, auto ...ts) {
            if (args.size() != sizeof...(ts))
                throw WrongNumber(sizeof...(ts), args.size());
            if ((... && check_cast_index(args, ts, 1)))
                return value_invoke(function, cast_index(args, ts, 1)...);
            throw wrong_types(args);
        });
    }
};

template <class F>
struct ContextAdaptor {
    F function;

    /// Run C++ functor; logs non-ClientError and rethrows all exceptions
    Value operator()(CallingContext &ct, ArgPack &args) const {
        Signature<F>::apply([](auto return_type, auto context_type, auto ...ts) {
            (NoMutable<decltype(*ts)>(), ...);
        });
        return Signature<F>::apply([&](auto return_type, auto context_type, auto ...ts) {
            if (args.size() != sizeof...(ts))
                throw WrongNumber(sizeof...(ts), args.size());
            if ((... && check_cast_index(args, ts, 2)))
                return value_invoke(function, ct, cast_index(args, ts, 2)...);
            throw wrong_types(args);
        });
    }
};

template <class F, class R, class T, class ...Ts>
Function make_function(F f, Pack<R, T, Ts...>) {
    return std::conditional_t<std::is_convertible_v<CallingContext &, T>,
        ContextAdaptor<F>, FunctionAdaptor<F>>{std::move(f)};
}

template <class F, class R>
Function make_function(F f, Pack<R>) {return FunctionAdaptor<F>{std::move(f)};}

template <class F>
Function make_function(F f) {return make_function(std::move(f), Signature<F>());}

/******************************************************************************/

}
