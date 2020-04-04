#pragma once
#include "Signature.h"
#include "Value.h"
// #include "types/Core.h"
#include <functional>
#include <stdexcept>

namespace rebind {

// call_none: no return
// call_value: return Value or exception
// call_ref:

// struct FnOutput : rebind_value {
//     stat::call none() {
//         return stat::call::none;
//     }

//     stat::call value() {
//         return stat::call::ok;
//     }

//     stat::call ref() {
//         return stat::call::ok;
//     }

//     stat::call exception() {
//         return stat::call::exception;
//     }

//     stat::call invalid_return() {
//         return stat::call::invalid_return;
//     }

//     stat::call wrong_number(int expect, int got) {
//         return stat::call::wrong_number;
//     }

//     stat::call wrong_type() {
//         return stat::call::wrong_type;
//     }
// };


/******************************************************************************************/

struct ArgView : rebind_args {
    // constexpr
    ArgView(Value *r, std::size_t c);// : rebind_args{static_cast<rebind_ref *>(static_cast<void *>(r)), c} {}

    Caller &caller() const;// {return *reinterpret(ptr)->target<Caller &>();}

    std::string_view name() const {
        auto const &s = reinterpret_cast<rebind_str const &>(ptr[1]);
        return std::string_view(s.data, s.size);
    }

    Value tag() const;// {return *reinterpret(ptr + 2);}

    Value * begin() const noexcept {return reinterpret(ptr) + 3;}

    auto size() const noexcept {return len;}

    Value * end() const noexcept {return begin() + size();}

    Value &operator[](std::size_t i) const noexcept {return begin()[i];}

    static Value * reinterpret(rebind_value *r) {return static_cast<Value *>(static_cast<void *>(r));}
};

/******************************************************************************************/

template <class ...Ts>
Vector<Value> to_arguments(Ts &&...ts) {
    Vector<Value> out;
    out.reserve(sizeof...(Ts));
    // (out.emplace_back(std::addressof(ts), qualifier_of<Ts &&>), ...);
    return out;
}

/******************************************************************************/

/// Cast element i of v to type T
template <class T>
decltype(auto) cast_index(ArgView const &v, Scope &s, IndexedType<T> i) {
    // s.index = i.index;
    return v[i.index].cast(s, Type<T>());
}

/******************************************************************************/

/// Invoke a function and arguments, storing output in a Variable if it doesn't return void
template <class F, class ...Ts>
stat::call invoke_to(Value &out, F const &f, Ts &&... ts) {
    using O = std::remove_cv_t<std::invoke_result_t<F, Ts...>>;
    DUMP("invoking function ", type_name<F>(), " with output ", type_name<O>());
    if constexpr(std::is_void_v<O>) {
        std::invoke(f, static_cast<Ts &&>(ts)...);
        return stat::call::ok;
    } else {
        out.emplace(Type<O>(), std::invoke(f, static_cast<Ts &&>(ts)...));
        return stat::call::ok;
    }
}

template <bool UseCaller, class F, class ...Ts>
stat::call caller_invoke(Value &out, F const &f, Caller &&c, Ts &&...ts) {
    c.enter();
    if constexpr(UseCaller) {
        invoke_to(out, f, std::move(c), static_cast<Ts &&>(ts)...);
    } else {
        invoke_to(out, f, static_cast<Ts &&>(ts)...);
    }
}

/******************************************************************************************/

template <class T, class SFINAE=void>
struct Call : std::false_type {};

/******************************************************************************/

template <int N, class F, class SFINAE=void>
struct Adapter;

template <int N, class F>
struct Callable {
    F function;
    Callable(F &&f) : function(std::move(f)) {}

    constexpr operator F const &() const noexcept {
        DUMP("cast into Callable ", &function, " ", reinterpret_cast<void const *>(function), " ", type_name<F>());
        return function;
    }
};

template <int N, class F>
struct Call<Callable<N, F>> : Adapter<N, F>, std::true_type {};

/******************************************************************************/


template <class F, class SFINAE>
struct Adapter<0, F, SFINAE> {
    using Signature = SimpleSignature<F>;
    using Return = decltype(first_type(Signature()));
    using UseCaller = decltype(second_is_convertible<Caller>(Signature()));
    using Args = decltype(without_first_types<1 + int(UseCaller::value)>(Signature()));

    /*
     Interface implementation for a function with no optional arguments.
     - Returns WrongNumber if args is not the right length
     */
    static stat::call call_to(Value &v, F const &f, ArgView args) noexcept {
        DUMP("call_to function adapter ", type_name<F>(), " ", std::addressof(f), " ", args.size());
        DUMP("method name", args.name(), " ", !args.name().empty());

        if (args.size() != Args::size) {
            // return v.wrong_number(Args::size, args.size());
            return stat::call::wrong_number;
        }

        auto frame = args.caller().new_frame(); // make a new unentered frame, must be noexcept
        Caller handle(frame); // make the Caller with a weak reference to frame
        Scope s(handle);

        return Args::indexed([&](auto ...ts) {
            return caller_invoke<UseCaller::value>(v, f, std::move(handle), cast_index(args, s, ts)...);
        });
        // It is planned to be allowed that the invoked function's C++ exception may propagate
        // in the future, assuming the caller policies allow this.
        // Therefore, resource destruction must be done via the frame going out of scope.
    }
};

/******************************************************************************/

// template <class R, class C>
// struct Adapter<0, R C::*, std::enable_if_t<std::is_member_object_pointer_v<R C::*>>> {
//     using F = R C::*;

//     static stat::call call_to(FnOutput &v, F const &f, Caller &&c, ArgView args) noexcept {
//         if (args.size() != 1) return v.wrong_number(1, args.size());

//         auto frame = caller.new_frame();
//         DUMP("Adapter<", Index::of<R>(), ", ", Index::of<C>(), ">::()");
//         Caller handle(frame);
//         Scope s(handle);

//         if (auto p = self.request<C &&>()) {
//             return out.set(std::move(*p).*f), stat::call::ok;
//         } else if (auto p = self.request<C &>()) {
//             return out.set((*p).*f), stat::call::ok;
//         } else if (auto p = self.request<C const &>()) {
//             return out.set((*p).*f), stat::call::ok;
//         }
//         // value conversions not allowed currently
//         // else if (auto p = self.request_value<C>()) { }
//         throw std::move(s.set_error("invalid argument"));
//     }
// };

/******************************************************************************/

/*
    For a callable type we need to get a function pointer of:
    bool(void const *self, void *out, Caller &&, ArgView);
*/
template <int N=-1, class F>
auto make_function(F f) {
    // First we apply lossless simplifications.
    // i.e. a lambda with specified arguments and no defaults is converted to a function pointer.
    // This makes the type more readable and might reduce the compile time a bit.
    auto simplified = SimplifyFunction<F, N>()(std::move(f));

    using S = decltype(simplified);

    // Now get the number of optional arguments
    constexpr int n = N == -1 ? 0 : SimpleSignature<S>::size - 1 - N;

    // Return the callable object holding the functor
    static_assert(is_manageable<Callable<n, S>>);
    return Callable<n, S>{std::move(simplified)};
}

/******************************************************************************/

// // N is the number of trailing optional arguments
// template <std::size_t N, class F, class SFINAE>
// struct Adapter {
//     F function;
//     using Signature = SimpleSignature<F>;
//     using Return = decltype(first_type(Signature()));
//     using UsesCaller = decltype(second_is_convertible<Caller>(Signature()));
//     using Args = decltype(without_first_types<1 + int(UsesCaller::value)>(Signature()));

//     template <class P, class Out>
//     static bool call_one(P, F const &f, Out &out, Caller &c, Scope &s, ArgView const &args) {
//         return P::indexed([&](auto ...ts) {
//             caller_invoke(out, UsesCaller(), f, std::move(c), cast_index(args, s, simplify_argument(ts))...);
//             return true;
//         });
//     }

//     template <class Out, std::size_t ...Is>
//     static bool call_indexed(F const &f, Out &out, Caller &c, ArgView const &args, Scope &s, std::index_sequence<Is...>) {
//         constexpr std::size_t const M = Args::size - 1; // number of total arguments minus 1
//         // check the number of arguments given and call with the under-specified arguments
//         return ((args.size() == M - Is ? call_one(Args::template slice<0, M - Is>(), f, out, c, s, args) : false) || ...);
//     }

//     template <class Out>
//     static bool impl(F const &f, Out &out, Caller &c, ArgView const &args) {
//         auto frame = c();
//         Caller handle(frame);
//         Scope s(handle);
//         if (args.size() == Args::size) { // handle fully specified arguments
//             return Args::indexed([&](auto ...ts) {
//                 caller_invoke(out, UsesCaller(), f,
//                     std::move(handle), cast_index(args, s, simplify_argument(ts))...);
//                 return true;
//             });
//         } else if (args.size() < Args::size - N) {
//             throw WrongNumber(Args::size - N, args.size());
//         } else if (args.size() > Args::size) {
//             throw WrongNumber(Args::size, args.size()); // try under-specified arguments
//         } else {
//             return call_indexed(f, out, handle, args, s, std::make_index_sequence<N>());
//         }
//     }

//     static bool call(void const *self, void *out, Caller &&c, ArgView args, Flag flag) {
//         auto const &f = *static_cast<F const *>(self);

//         if (flag == Flag::ref) {
//             if constexpr (std::is_reference_v<Return>) return impl(f, *static_cast<Ref *>(out), c, args);
//             throw std::runtime_error("Requested reference from a function returning a value");
//         }

//         return impl(f, *static_cast<Value *>(out), c, args);
//     }
// };

// /******************************************************************************/

/******************************************************************************/

}
