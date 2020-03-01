#pragma once
#include "Signature.h"
#include "BasicTypes.h"
#include <functional>

namespace rebind {


/******************************************************************************/

/// Invoke a function and arguments, storing output in a Variable if it doesn't return void
template <class Out, class F, class ...Ts>
Out opaque_invoke(F const &f, Ts &&... ts) {
    using O = std::remove_cv_t<std::invoke_result_t<F, Ts...>>;
    DUMP("invoking function with output type ", typeid(Type<O>).name());
    if constexpr(std::is_same_v<void, O>) {
        std::invoke(f, static_cast<Ts &&>(ts)...);
        return {};
    } else {
        return Out::from(std::invoke(f, static_cast<Ts &&>(ts)...));
    }
}

template <class Out, class F, class ...Ts>
Out caller_invoke(std::true_type, F const &f, Caller &&c, Ts &&...ts) {
    c.enter();
    return opaque_invoke<Out>(f, std::move(c), static_cast<Ts &&>(ts)...);
}

template <class Out, class F, class ...Ts>
Out caller_invoke(std::false_type, F const &f, Caller &&c, Ts &&...ts) {
    c.enter();
    return opaque_invoke<Out>(f, static_cast<Ts &&>(ts)...);
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

template <int N, class R, class ...Ts, std::enable_if_t<N == 1, int> = 0>
Pack<Ts...> skip_head(Pack<R, Ts...>);

template <int N, class R, class C, class ...Ts, std::enable_if_t<N == 2, int> = 0>
Pack<Ts...> skip_head(Pack<R, C, Ts...>);

template <class C, class R>
std::false_type has_head(Pack<R>);

template <class C, class R, class T, class ...Ts>
std::is_convertible<T, C> has_head(Pack<R, T, Ts...>);

/******************************************************************************/

// template <class F>
// using OpaqueOutput = std::conditional_t<
//     std::is_reference_v<decltype(*SimpleSignature<F>::template at<0>())>, Pointer, Value>;


// N is the number of trailing optional arguments
template <class Out, class F, std::size_t N, class SFINAE=void>
struct Adapter {
    F function;
    using UsesCaller = decltype(has_head<Caller>(SimpleSignature<F>()));
    using AllTypes = decltype(skip_head<1 + int(UsesCaller::value)>(SimpleSignature<F>()));

    template <class P>
    void call_one(P, Output &out, Caller &&c, Scope &s, Arguments &args) const {
        P::indexed([&](auto ...ts) {
            out = caller_invoke<Output>(UsesCaller(), function, std::move(c), cast_index(args, s, simplify_argument(ts))...);
        });
    }

    template <std::size_t ...Is>
    Out call(Arguments &args, Caller &&c, Scope &s, std::index_sequence<Is...>) const {
        Out out;
        constexpr std::size_t const M = AllTypes::size - 1; // number of total arguments minus 1
        // check the number of arguments given and call with the under-specified arguments
        ((args.size() == M - Is ? call_one(AllTypes::template slice<0, M - Is>(), out, std::move(c), s, args) : void()), ...);
        return out;
    }

    Out operator()(Caller c, Arguments args) const {
        auto frame = c();
        Caller handle(frame);
        Scope s(handle);
        if (args.size() == AllTypes::size) { // handle fully specified arguments
            return AllTypes::indexed([&](auto ...ts) {
                return caller_invoke<Output>(UsesCaller(), function,
                    std::move(handle), cast_index(args, s, simplify_argument(ts))...);
            });
        } else if (args.size() < AllTypes::size - N) {
            throw WrongNumber(AllTypes::size - N, args.size());
        } else if (args.size() > AllTypes::size) {
            throw WrongNumber(AllTypes::size, args.size()); // try under-specified arguments
        } else {
            return call(args, std::move(handle), s, std::make_index_sequence<N>());
        }
    }
};

/******************************************************************************/

template <class Out, class F, class SFINAE>
struct Adapter<Out, F, 0, SFINAE> {
    F function;
    using Ctx = decltype(has_head<Caller>(SimpleSignature<F>()));
    using Sig = decltype(skip_head<1 + int(Ctx::value)>(SimpleSignature<F>()));

    Out operator()(Caller c, Arguments args) const {
        DUMP("Adapter<", type_index<F>(), ">::()");
        if (args.size() != Sig::size)
            throw WrongNumber(Sig::size, args.size());
        return Sig::indexed([&](auto ...ts) {
            auto frame = c();
            Caller handle(frame);
            Scope s(handle);
            return caller_invoke<Out>(Ctx(), function, std::move(handle), cast_index(args, s, ts)...);
        });
    }
};

/******************************************************************************/

template <class R, class C>
struct Adapter<0, R C::*, std::enable_if_t<std::is_member_object_pointer_v<R C::*>>> {
    R C::* function;

    Pointer operator()(Caller c, Arguments args) const {
        if (args.size() != 1) throw WrongNumber(1, args.size());
        auto &r = args[0];
        auto frame = c();
        DUMP("Adapter<", type_index<R>(), ", ", type_index<C>(), ">::()");
        Caller handle(frame);
        Scope s(handle);

        if (!r.index().matches<C>() || r.qualifier() == Lvalue) {
            DUMP("Adapter<", type_index<R>(), ", ", type_index<C>(), ">::() try &");
            if (auto p = r.request(s, Type<C &>())) {
                frame->enter();
                return {Type<R &>(), std::invoke(function, *p)};
            }
        }

        DUMP("Adapter<", type_index<R>(), ", ", type_index<C>(), ">::() try const &");
        if (auto p = r.request(s, Type<C const &>())) {
            frame->enter();
            return {Type<R const &>(), std::invoke(function, *p)};
        }

        if (auto p = r.request(s, Type<C>())) {
            DUMP("Adapter<", type_index<R>(), ", ", type_index<C>(), ">::() try &&");
            frame->enter();
            return {Type<std::remove_cv_t<R>>(), std::invoke(function, std::move(*p))};
        }

        throw std::move(s.set_error("should not be used"));
    }
};

/******************************************************************************/

}
