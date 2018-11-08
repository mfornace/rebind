#include "Signature.h"
#include "Types.h"

namespace cpy {

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

template <std::size_t N, class F>
struct FunctionAdaptor {
    F function;
    using Ctx = decltype(has_head<Caller>(SimpleSignature<F>()));
    using Sig = decltype(skip_head<1 + int(Ctx::value)>(SimpleSignature<F>()));

    template <class P>
    void call_each(P, Variable &out, Caller &&c, Dispatch &msg, ArgPack &args) const {
        P::indexed([&](auto ...ts) {
            out = caller_invoke(Ctx(), function, std::move(c), cast_index(args, msg, simplify_argument_type(ts))...);
        });
    }

    template <std::size_t ...Is>
    Variable call(ArgPack &args, Caller &&c, Dispatch &msg, std::index_sequence<Is...>) const {
        Variable out;
        ((args.size() == N - Is - 1 ? call_each(Sig::template slice<0, N - Is - 1>(), out, std::move(c), msg, args) : void()), ...);
        return out;
    }

    Variable operator()(Caller c, ArgPack args) const {
        Dispatch msg;
        if (args.size() == Sig::size)
            return Sig::indexed([&](auto ...ts) {
                return caller_invoke(Ctx(), function, std::move(c), cast_index(args, msg, simplify_argument_type(ts))...);
            });
        else if (args.size() < Sig::size - N)
            throw WrongNumber(Sig::size - N, args.size());
        else if (args.size() > Sig::size)
            throw WrongNumber(Sig::size, args.size());
        return call(args, std::move(c), msg, std::make_index_sequence<N>());
    }
};

template <class F>
struct FunctionAdaptor<0, F> {
    F function;
    using Ctx = decltype(has_head<Caller>(SimpleSignature<F>()));
    using Sig = decltype(skip_head<1 + int(Ctx::value)>(SimpleSignature<F>()));

    Variable operator()(Caller c, ArgPack args) const {
        if (args.size() != Sig::size)
            throw WrongNumber(Sig::size, args.size());
        return Sig::indexed([&](auto ...ts) {
            Dispatch msg;
            return caller_invoke(Ctx(), function, std::move(c), cast_index(args, msg, ts)...);
        });
    }
};

/******************************************************************************/

}