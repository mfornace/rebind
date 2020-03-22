#pragma once
#include "Signature.h"
#include "types/Core.h"
#include <functional>
#include <stdexcept>

namespace rebind {

/******************************************************************************/

/// Invoke a function and arguments, storing output in a Variable if it doesn't return void
template <class Out, class F, class ...Ts>
void opaque_invoke(Out &out, F const &f, Ts &&... ts) {
    using O = std::remove_cv_t<std::invoke_result_t<F, Ts...>>;
    static_assert(std::is_same_v<Out, Value> || std::is_reference_v<O>);
    DUMP("invoking function with output type ", typeid(Type<O>).name());
    if constexpr(std::is_void_v<O>) {
        std::invoke(f, static_cast<Ts &&>(ts)...);
    } else {
        out.set(std::invoke(f, static_cast<Ts &&>(ts)...));
    }
}

template <class Out, class F, class ...Ts>
void caller_invoke(Out &out, std::true_type, F const &f, Caller &&c, Ts &&...ts) {
    c.enter();
    opaque_invoke(out, f, std::move(c), static_cast<Ts &&>(ts)...);
}

template <class Out, class F, class ...Ts>
void caller_invoke(Out &out, std::false_type, F const &f, Caller &&c, Ts &&...ts) {
    c.enter();
    opaque_invoke(out, f, static_cast<Ts &&>(ts)...);
}

/******************************************************************************/

// enum class Tag {call, call_except, match};

// struct FunctionImpl {
//     // FunctionImpl(FunctionImpl &&) noexcept;
//     FunctionImpl(FunctionImpl const &);
//     bool match(Value const &, Arguments const &) const;
//     bool value(Caller &&, Value &, Arguments const &) const;
//     bool ref(Caller &&, Ref &, Arguments const &) const;
//     ~FunctionImpl();
// };

// using FunctionImpl = std::function<bool(Caller *, Value *, Ref *, Arguments const &)>;
// if caller, call function and store in value or ref
// if not caller, match and return if match
// call_except?: call function, store output in the ref or value, return if success

template <std::size_t N, class F, class SFINAE=void>
struct Adapter;

/*
    For a callable type we need to get a function pointer of:
    bool(void const *self, void *out, Caller &&, Arguments);
*/
template <int N=-1, class F>
auto declare_function(F f) {
    auto simplified = SimplifyFunction<F, N>()(std::move(f));
    using S = decltype(simplified);
    constexpr std::size_t n = N == -1 ? 0 : SimpleSignature<S>::size - 1 - N;
    const_cast<CTable &>(fetch<S>()->c).call = &Adapter<n, S>::call;
    return simplified;
}

/******************************************************************************/

template <class F, class SFINAE>
struct Adapter<0, F, SFINAE> {
    using Signature = SimpleSignature<F>;
    using Return = decltype(first_type(Signature()));
    using UseCaller = decltype(second_is_convertible<Caller>(Signature()));
    using Args = decltype(without_first_types<1 + int(UseCaller::value)>(Signature()));

    template <class Out>
    static bool impl(F const &f, Out &out, Caller &caller, Arguments const &args) {
        auto frame = caller();
        Caller handle(frame);
        Scope s(handle);

        return Args::indexed([&](auto ...ts) {
            caller_invoke(out, UseCaller(), f, std::move(handle), cast_index(args, s, ts)...);
            return true;
        });
    }

    /*
     Interface implementation for a function with no optional arguments.
     - Caller is assumed non-null
     - One of Value and Ref should be non-null--it will be set to the function output
     - Throws WrongNumber if args is not the right length
     - Always returns true
     */
    static bool call(void const *self, void *out, Caller &&c, Arguments args, Flag flag) {
        DUMP("Adapter<", fetch<F>(), ">::()");

        if (args.size() != Args::size)
            throw WrongNumber(Args::size, args.size());

        auto const &f = *static_cast<F const *>(self);

        if (flag == Flag::ref) {
            if constexpr (std::is_reference_v<Return>)
                return call(f, *static_cast<Ref *>(out), c, args);
            throw std::runtime_error("Requested reference from a function returning a value");
        }

        return impl(f, *static_cast<Value *>(out), c, args);
    }
};

/******************************************************************************/

// N is the number of trailing optional arguments
template <std::size_t N, class F, class SFINAE>
struct Adapter {
    F function;
    using Signature = SimpleSignature<F>;
    using Return = decltype(first_type(Signature()));
    using UsesCaller = decltype(second_is_convertible<Caller>(Signature()));
    using Args = decltype(without_first_types<1 + int(UsesCaller::value)>(Signature()));

    template <class P, class Out>
    static bool call_one(P, F const &f, Out &out, Caller &c, Scope &s, Arguments const &args) {
        return P::indexed([&](auto ...ts) {
            caller_invoke(out, UsesCaller(), f, std::move(c), cast_index(args, s, simplify_argument(ts))...);
            return true;
        });
    }

    template <class Out, std::size_t ...Is>
    static bool call_indexed(F const &f, Out &out, Caller &c, Arguments const &args, Scope &s, std::index_sequence<Is...>) {
        constexpr std::size_t const M = Args::size - 1; // number of total arguments minus 1
        // check the number of arguments given and call with the under-specified arguments
        return ((args.size() == M - Is ? call_one(Args::template slice<0, M - Is>(), f, out, c, s, args) : false) || ...);
    }

    template <class Out>
    static bool impl(F const &f, Out &out, Caller &c, Arguments const &args) {
        auto frame = c();
        Caller handle(frame);
        Scope s(handle);
        if (args.size() == Args::size) { // handle fully specified arguments
            return Args::indexed([&](auto ...ts) {
                caller_invoke(out, UsesCaller(), f,
                    std::move(handle), cast_index(args, s, simplify_argument(ts))...);
                return true;
            });
        } else if (args.size() < Args::size - N) {
            throw WrongNumber(Args::size - N, args.size());
        } else if (args.size() > Args::size) {
            throw WrongNumber(Args::size, args.size()); // try under-specified arguments
        } else {
            return call_indexed(f, out, handle, args, s, std::make_index_sequence<N>());
        }
    }

    static bool call(void const *self, void *out, Caller &&c, Arguments args, Flag flag) {
        auto const &f = *static_cast<F const *>(self);

        if (flag == Flag::ref) {
            if constexpr (std::is_reference_v<Return>) return impl(f, *static_cast<Ref *>(out), c, args);
            throw std::runtime_error("Requested reference from a function returning a value");
        }

        return impl(f, *static_cast<Value *>(out), c, args);
    }
};

/******************************************************************************/

template <class R, class C>
struct Adapter<0, R C::*, std::enable_if_t<std::is_member_object_pointer_v<R C::*>>> {
    using F = R C::*;

    template <class Out>
    static bool impl(F f, Out &out, Caller &caller, Ref const &self) {
        auto frame = caller();
        DUMP("Adapter<", fetch<R>(), ", ", fetch<C>(), ">::()");
        Caller handle(frame);
        Scope s(handle);

        if (auto p = self.request<C &&>()) {
            return out.set(std::move(*p).*f), true;
        } else if (auto p = self.request<C &>()) {
            return out.set((*p).*f), true;
        } else if (auto p = self.request<C const &>()) {
            return out.set((*p).*f), true;
        }
        // value conversions not allowed currently
        // else if (auto p = self.request_value<C>()) { }
        throw std::move(s.set_error("invalid argument"));
    }

    static bool call(void const *f, void *out, Caller &&c, Arguments args, Flag flag) {
        if (args.size() != 1) throw WrongNumber(1, args.size());

        if (flag == Flag::ref) {
            return impl(*static_cast<F const *>(f), *static_cast<Ref *>(out), c, args[0]);
        } else {
            return impl(*static_cast<F const *>(f), *static_cast<Value *>(out), c, args[0]);
        }
    }
};

/******************************************************************************/

}
