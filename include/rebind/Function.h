#pragma once
#include "Adapter.h"

#include <typeindex>
#include <iostream>
#include <sstream>

namespace rebind {

// struct Overload {
//     bool operator()(Value *, Pointer *, Arguments const &, Tag) const;

//     // Value operator()(Arguments const &v) const noexcept;
//     // Pointer ref(Arguments const &v) const; // optional
//     // bool match(Arguments const &v, Pointer const &tag) const; //optional

//     // Pointer target() const;
//     // Pointer operator()(Arguments const &v, Pointer const &dispatch) const;

// };
//exceptions?

/******************************************************************************/

// A single (non-ambiguous) callable object
struct Overload {
    FunctionImpl impl;
    ErasedSignature signature; // not sure this is useful

    Overload() = default;

    Overload(FunctionImpl f, ErasedSignature const &s={}) : impl(std::move(f)), signature(s) {}

    template <class F>
    Overload(F f) : Overload(
        Adapter<0, decltype(SimplifyFunction<F>()(std::move(f)))>(SimplifyFunction<F>()(std::move(f))),
        SimpleSignature<decltype(SimplifyFunction<F>()(std::move(f)))>()) {}

    template <int N = -1, class F>
    static Overload from(F f) {
        auto fun = SimplifyFunction<F, N>()(std::move(f));
        constexpr std::size_t n = N == -1 ? 0 : SimpleSignature<decltype(fun)>::size - 1 - N;
        return {Adapter<n, decltype(fun)>{std::move(fun)}, SimpleSignature<decltype(fun)>()};
    }

    explicit operator bool() const {return bool(impl);}

    template <class ...Ts>
    Value value(Caller c, Ts &&...ts) const {
        return call_value(std::move(c), to_arguments(static_cast<Ts &&>(ts)...));
    }

    template <class ...Ts>
    Pointer reference(Caller c, Ts &&...ts) const {
        return call_reference(std::move(c), to_arguments(static_cast<Ts &&>(ts)...));
    }

    template <class ...Ts>
    Value operator()(Caller c, Ts &&...ts) const {
        return call_value(std::move(c), static_cast<Ts &&>(ts)...);
    }

    bool call(Caller &c, Value *v, Pointer *p, Arguments const &args) const {
        DUMP("call function: n=", args.size(), ", v=", bool(v), ", p=", bool(p));
        for (auto &&p: args) {DUMP("argument ", p.name(), " ", p.index(), " ", p.qualifier());}
        return impl(&c, v, p, args);
    }

    void call_in_place(Value &out, Caller c, Arguments const &v) const {call(c, &out, nullptr, v);}

    void call_in_place(Pointer &out, Caller c, Arguments const &v) const {call(c, nullptr, &out, v);}

    Value call_value(Caller c, Arguments const &v) const {
        Value out;
        call(c, &out, nullptr, v);
        return out;
    }

    Pointer call_reference(Caller c, Arguments const &v) const {
        Pointer out;
        call(c, nullptr, &out, v);
        return out;
    }
};

/******************************************************************************/

// A list of at least one overload
// A bit of optimization is used since commonly only one overload will be used
// No other API is currently provided here
// The user is expected to disambiguate the overloading if needed
struct Function {
    Overload first;
    Vector<Overload> rest;

    Function() = default;
    Function(Overload fun) : first(std::move(fun)) {}

    template <class F>
    void visit(F &&f) const {
        f(first);
        for (auto const &x : rest) f(x);
    }

    template <class ...Args>
    Overload & emplace(Args &&...args) {
        if (first) {
            rest.emplace_back(std::forward<Args>(args)...);
            return rest.back();
        } else {
            first = Overload{std::forward<Args>(args)...};
            return first;
        }
    }
};

/******************************************************************************/

template <class R, class ...Ts>
struct AnnotatedCallback {
    Caller caller;
    Overload function;

    AnnotatedCallback() = default;
    AnnotatedCallback(Overload f, Caller c) : function(std::move(f)), caller(std::move(c)) {}

    R operator()(Ts ...ts) const {
        return function(caller, to_arguments(static_cast<Ts &&>(ts)...)).cast(Type<R>());
    }
};

template <class R>
struct Callback {
    Caller caller;
    Overload function;

    Callback() = default;
    Callback(Overload f, Caller c) : function(std::move(f)), caller(std::move(c)) {}

    template <class ...Ts>
    R operator()(Ts &&...ts) const {
        return function(caller, to_arguments(static_cast<Ts &&>(ts)...)).cast(Type<R>());
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
// Overload construct(Type<R>) {
//     Overload out;
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
// struct FromPointer<Callback<R>> {
//     std::optional<Callback<R>> operator()(Pointer const &v, Scope &msg) const {
//         if (!msg.caller) msg.error("Calling context expired", typeid(Callback<R>));
//         else if (auto p = v.from_pointer<Overload>(msg)) return Callback<R>{std::move(*p), msg.caller};
//         return {};
//     }
// };


// template <class R, class ...Ts>
// struct FromPointer<AnnotatedCallback<R, Ts...>> {
//     using type = AnnotatedCallback<R, Ts...>;
//     std::optional<type> operator()(Variable const &v, Scope &msg) const {
//         if (!msg.caller) msg.error("Calling context expired", typeid(type));
//         else if (auto p = v.from_pointer<Overload>(msg)) return type{std::move(*p), msg.caller};
//         return {};
//     }
// };

/******************************************************************************/

}
