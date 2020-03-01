#pragma once
#include "Adapter.h"

#include <typeindex>
#include <iostream>
#include <sstream>

namespace rebind {

using ErasedFunction = std::function<Value(Caller, Arguments const &)>;

/******************************************************************************/

struct Function {
    ErasedFunction impl;
    ErasedSignature signature;

    Function() = default;

    Function(ErasedFunction f, ErasedSignature const &s={}) : impl(std::move(f)), signature(s) {}

    template <class F>
    Function(F f) : Function(
        Adapter<0, Value, decltype(SimplifyFunction<F>()(std::move(f)))>(SimplifyFunction<F>()(std::move(f))),
        SimpleSignature<decltype(SimplifyFunction<F>()(std::move(f)))>()) {}

    template <int N = -1, class F>
    static Function from(F f) {
        auto fun = SimplifyFunction<F>()(std::move(f));
        constexpr std::size_t n = N == -1 ? 0 : SimpleSignature<decltype(fun)>::size - 1 - N;
        return {Adapter<n, Value, decltype(fun)>{std::move(fun)}, SimpleSignature<decltype(fun)>()};
    }

    Value operator()(Caller c, Arguments const &v) const {
        DUMP("    - calling type erased Function ");
        return impl(std::move(c), v);
    }

    explicit operator bool() const {return bool(impl);}

    template <class ...Ts>
    Value operator()(Caller c, Ts &&...ts) const {
        DUMP("    - calling Function ");
        Arguments v;
        v.reserve(sizeof...(Ts));
        (v.emplace_back(static_cast<Ts &&>(ts)), ...);
        return (*this)(std::move(c), std::move(v));
    }
};

/******************************************************************************/

template <class R, class ...Ts>
struct AnnotatedCallback {
    Caller caller;
    Function function;

    AnnotatedCallback() = default;
    AnnotatedCallback(Function f, Caller c) : function(std::move(f)), caller(std::move(c)) {}

    R operator()(Ts ...ts) const {
        Sequence pack;
        pack.reserve(sizeof...(Ts));
        (pack.emplace_back(static_cast<Ts &&>(ts)), ...);
        return function(caller, std::move(pack)).cast(Type<R>());
    }
};

template <class R>
struct Callback {
    Caller caller;
    Function function;

    Callback() = default;
    Callback(Function f, Caller c) : function(std::move(f)), caller(std::move(c)) {}

    template <class ...Ts>
    R operator()(Ts &&...ts) const {
        Arguments pack;
        pack.reserve(sizeof...(Ts));
        (pack.emplace_back(static_cast<Ts &&>(ts)), ...);
        return function(caller, std::move(pack)).cast(Type<R>());
    }
};

/******************************************************************************/

/// Cast element i of v to type T
template <class T>
decltype(auto) cast_index(Arguments const &v, Scope &s, IndexedType<T> i) {
    // s.index = i.index;
    return v[i.index].cast(s, Type<T>());
}

/******************************************************************************/

template <class R, class ...Args>
struct Construct {
    template <class ...Ts>
    constexpr R operator()(Ts ...ts) const {
        if constexpr(std::is_constructible_v<R, Ts &&...>) {
            return R(static_cast<Ts &&>(ts)...);
        } else {
            return R{static_cast<Ts &&>(ts)...};
        }
    }
};

template <class R, class ...Args>
struct Signature<Construct<R, Args...>> : Pack<R, Args...> {};

/******************************************************************************/

// template <class ...Ts>
// struct VariadicBases : Ts... {};

// template <class R, class ...Ts>
// Construct<R, Ts...> one_construct(Pack<R, Ts...>);

// template <class R, class ...Ts, std::size_t ...Is>
// auto all_constructs(Pack<R, Ts...>, std::index_sequence<Is...>) {
//     return VariadicBases<decltype(one_construct(Pack<R, Ts...>::template slice<0, (1 + sizeof...(Ts) - Is)>()))...>{};
// }

// template <std::size_t N, class R, class ...Ts>
// struct PartialConstruct : decltype(all_constructs(Pack<R, Ts...>(), std::make_index_sequence<1 + sizeof...(Ts) - N>())) {
//     using full_type = Construct<R, Ts...>;
// };

// template <std::size_t N, class R, class ...Ts>
// struct Signature<PartialConstruct<N, R, Ts...>> : Pack<R, Ts...> {};

// template <class R>
// struct PartialConstruct {
//     template <class ...Ts>
//     constexpr R operator()(Ts &&...ts) const {
//         if constexpr(std::is_constructible_v<R, Ts &&...>) {
//             return R(static_cast<Ts &&>(ts)...);
//         } else {
//             return R(static_cast<Ts &&>(ts)...);
//         }
//     }
// };

/******************************************************************************/

template <class ...Ts, class R>
constexpr auto construct(Type<R>) {return Construct<R, Ts...>();}

// template <std::size_t N, class ...Ts, class R>
// Function construct(Type<R>) {
//     Function out;
//     out.emplace<N>(PartialConstruct<N, R, Ts...>());
//     return out;
// }

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

// template <class R>
// struct Request<Callback<R>> {
//     std::optional<Callback<R>> operator()(Pointer const &v, Scope &msg) const {
//         if (!msg.caller) msg.error("Calling context expired", typeid(Callback<R>));
//         else if (auto p = v.request<Function>(msg)) return Callback<R>{std::move(*p), msg.caller};
//         return {};
//     }
// };


// template <class R, class ...Ts>
// struct Request<AnnotatedCallback<R, Ts...>> {
//     using type = AnnotatedCallback<R, Ts...>;
//     std::optional<type> operator()(Variable const &v, Scope &msg) const {
//         if (!msg.caller) msg.error("Calling context expired", typeid(type));
//         else if (auto p = v.request<Function>(msg)) return type{std::move(*p), msg.caller};
//         return {};
//     }
// };

/******************************************************************************/

}
