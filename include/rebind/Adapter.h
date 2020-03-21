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
    if constexpr(std::is_same_v<void, O>) {
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

inline Type<void> simplify_argument(Type<void>) {return {};}

/// Check type and remove cv qualifiers on arguments that are not lvalues
template <class T>
auto simplify_argument(Type<T>) {
    using U = std::remove_reference_t<T>;
    static_assert(!std::is_volatile_v<U> || !std::is_reference_v<T>, "volatile references are not supported");
    using V = std::conditional_t<std::is_lvalue_reference_v<T>, U &, std::remove_cv_t<U>>;
    using Out = std::conditional_t<std::is_rvalue_reference_v<T>, V &&, V>;
    static_assert(std::is_convertible_v<Out, T>, "simplified type should be compatible with original");
    return Type<Out>();
}

template <class T>
auto simplify_argument(IndexedType<T> t) {
    return IndexedType<typename decltype(simplify_argument(Type<T>()))::type>{t.index};
}

template <class ...Ts>
Pack<decltype(*simplify_argument(Type<Ts>()))...> simplify_signature(Pack<Ts...>) {return {};}

template <class F>
using SimpleSignature = decltype(simplify_signature(Signature<F>()));

/******************************************************************************/

// template <class F>
// using OpaqueOutput = std::conditional_t<
//     std::is_reference_v<decltype(*SimpleSignature<F>::template at<0>())>, Ref, Value>;


// enum class Tag {call, call_except, match};

using FunctionImpl = std::function<bool(Caller *, Value *, Ref *, Arguments const &)>;
// if caller, call function and store in value or ref
// if not caller, match and return if match
// call_except?: call function, store output in the ref or value, return if success

template <std::size_t N, class F, class SFINAE=void>
struct Adapter;

/******************************************************************************/

template <class F, class SFINAE>
struct Adapter<0, F, SFINAE> {
    F function;
    using Signature = SimpleSignature<F>;
    using Return = decltype(first_type(Signature()));
    using UseCaller = decltype(second_is_convertible<Caller>(Signature()));
    using Args = decltype(without_first_types<1 + int(UseCaller::value)>(Signature()));

    template <class Out>
    bool call(Caller &&caller, Out &out, Arguments const &args) const {
        auto frame = caller();
        Caller handle(frame);
        Scope s(handle);

        return Args::indexed([&](auto ...ts) {
            caller_invoke(out, UseCaller(), function, std::move(handle), cast_index(args, s, ts)...);
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
    bool operator()(Caller *c, Value *v, Ref *p, Arguments const &args) const {
        DUMP("Adapter<", fetch<F>(), ">::()");

        if (args.size() != Args::size)
            throw WrongNumber(Args::size, args.size());

        if constexpr (std::is_reference_v<Return>) {
            if (p) return call(std::move(c), *p, args);
        } else {
            if (p) throw std::runtime_error("Requested reference from a function returning a value");
        }

        return call(std::move(*c), *v, args);
    }

};

// N is the number of trailing optional arguments
template <std::size_t N, class F, class SFINAE>
struct Adapter {
    F function;
    using Signature = SimpleSignature<F>;
    using Return = decltype(first_type(Signature()));
    using UsesCaller = decltype(second_is_convertible<Caller>(Signature()));
    using Args = decltype(without_first_types<1 + int(UsesCaller::value)>(Signature()));

    template <class P, class Out>
    bool call_one(P, Out &out, Caller &c, Scope &s, Arguments const &args) const {
        return P::indexed([&](auto ...ts) {
            caller_invoke(out, UsesCaller(), function, std::move(c), cast_index(args, s, simplify_argument(ts))...);
            return true;
        });
    }

    template <class Out, std::size_t ...Is>
    bool call_indexed(Out &out, Arguments const &args, Caller &c, Scope &s, std::index_sequence<Is...>) const {
        constexpr std::size_t const M = Args::size - 1; // number of total arguments minus 1
        // check the number of arguments given and call with the under-specified arguments
        return ((args.size() == M - Is ? call_one(Args::template slice<0, M - Is>(), out, c, s, args) : false) || ...);
    }

    template <class Out>
    bool call(Caller &c, Out &out, Arguments const &args) const {
        auto frame = c();
        Caller handle(frame);
        Scope s(handle);
        if (args.size() == Args::size) { // handle fully specified arguments
            return Args::indexed([&](auto ...ts) {
                caller_invoke(out, UsesCaller(), function,
                    std::move(handle), cast_index(args, s, simplify_argument(ts))...);
                return true;
            });
        } else if (args.size() < Args::size - N) {
            throw WrongNumber(Args::size - N, args.size());
        } else if (args.size() > Args::size) {
            throw WrongNumber(Args::size, args.size()); // try under-specified arguments
        } else {
            return call_indexed(out, args, handle, s, std::make_index_sequence<N>());
        }
    }

    bool operator()(Caller *c, Value *v, Ref *p, Arguments const &args) const {
        if (args.size() != 1) throw WrongNumber(1, args.size());

        if constexpr (std::is_reference_v<Return>) {
            if (p) return call(c, *p, args);
        } else {
            if (p) throw std::runtime_error("Requested reference from a function returning a value");
        }

        return call(*c, *v, args);
    }
};

/******************************************************************************/

template <class R, class C>
struct Adapter<0, R C::*, std::enable_if_t<std::is_member_object_pointer_v<R C::*>>> {
    R C::* function;

    template <class Out>
    bool call(Caller &&caller, Out &out, Ref const &self) const {
        auto frame = caller();
        DUMP("Adapter<", fetch<R>(), ", ", fetch<C>(), ">::()");
        Caller handle(frame);
        Scope s(handle);

        if (auto p = self.request<C &&>()) {
            return out.set(std::move(*p).*function), true;
        } else if (auto p = self.request<C &>()) {
            return out.set((*p).*function), true;
        } else if (auto p = self.request<C const &>()) {
            return out.set((*p).*function), true;
        }
        // value conversions not allowed currently
        // else if (auto p = self.request_value<C>()) { }
        throw std::move(s.set_error("invalid argument"));
    }

    bool operator()(Caller *c, Value *v, Ref *p, Arguments const &args) const {
        if (args.size() != 1) throw WrongNumber(1, args.size());

        if (v) return call(std::move(*c), *v, args[0]);
        if (p) return call(std::move(*c), *p, args[0]);
        return false;
    }
};

/******************************************************************************/

}
