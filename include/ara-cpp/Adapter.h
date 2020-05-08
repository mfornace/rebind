    #pragma once
#include <ara/Call.h>

#include <iostream>
#include <sstream>

namespace ara {


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

template <class ...Ts, class R>
constexpr auto construct(Type<R>) {return Construct<R, Ts...>();}

/******************************************************************************/

template <class T>
struct Streamable {
    std::string operator()(T const &t) const {
        std::ostringstream os;
        os << t;
        return os.str();
    }
};

template <class T>
Streamable<T> streamable(Type<T> = {}) {return {};}

/*
1. bool operator()(Value &, ArgView const &)
2. bool operator()(Ref &, ArgView const &)
3. bool match(ArgView const &v, Ref const &tag)
4.
*/

// struct Overload {
//     bool operator()(Value *, Ref *, ArgView const &, Tag) const;

//     // Value operator()(ArgView const &v) const noexcept;
//     // Ref ref(ArgView const &v) const; // optional
//     // bool match(ArgView const &v, Ref const &tag) const; //optional

//     // Ref target() const;
//     // Ref operator()(ArgView const &v, Ref const &dispatch) const;

// };
//exceptions?

/******************************************************************************/

//     Overload(F f) : Overload(
//         Adapter<0, decltype(SimplifyFunction<F>()(std::move(f)))>(SimplifyFunction<F>()(std::move(f))),
//         SimpleSignature<decltype(SimplifyFunction<F>()(std::move(f)))>()) {}

//     template <int N = -1, class F>
//     static Overload from(F f) {
//         constexpr std::size_t n = N == -1 ? 0 : SimpleSignature<decltype(fun)>::size - 1 - N;
//         return {Adapter<n, decltype(fun)>{std::move(fun)}, SimpleSignature<decltype(fun)>()};
//     }

//     template <class ...Ts>
//     Value value(Caller c, Ts &&...ts) const {
//         return call_value(std::move(c), to_arguments(static_cast<Ts &&>(ts)...));
//     }

//     template <class ...Ts>
//     Ref reference(Caller c, Ts &&...ts) const {
//         return call_reference(std::move(c), to_arguments(static_cast<Ts &&>(ts)...));
//     }

//     template <class ...Ts>
//     Value operator()(Caller c, Ts &&...ts) const {
//         return call_value(std::move(c), static_cast<Ts &&>(ts)...);
//     }

//     bool call(Caller &c, Value *v, Ref *p, ArgView const &args) const {
//         DUMP("call function: n=", args.size(), ", v=", bool(v), ", p=", bool(p));
//         for (auto &&p: args) {DUMP("argument ", p.name(), p.qualifier());}
//         return impl(&c, v, p, args);
//     }

//     void call_in_place(Value &out, Caller c, ArgView const &v) const {call(c, &out, nullptr, v);}

//     void call_in_place(Ref &out, Caller c, ArgView const &v) const {call(c, nullptr, &out, v);}

//     Value call_value(Caller c, ArgView const &v) const {
//         Value out;
//         call(c, &out, nullptr, v);
//         return out;
//     }

//     Ref call_reference(Caller c, ArgView const &v) const {
//         Ref out;
//         call(c, nullptr, &out, v);
//         return out;
//     }
// };

/******************************************************************************/

// A list of at least one overload
// A bit of optimization is used since commonly only one overload will be used
// No other API is currently provided here
// The user is expected to disambiguate the overloading if needed
// struct Function {
//     Overload first;
//     Vector<Overload> rest;

//     Function() = default;
//     Function(Overload fun) : first(std::move(fun)) {}

//     template <class F>
//     void visit(F &&f) const {
//         f(first);
//         for (auto const &x : rest) f(x);
//     }

//     template <class ...Args>
//     Overload & emplace(Args &&...args) {
//         if (first) {
//             rest.emplace_back(std::forward<Args>(args)...);
//             return rest.back();
//         } else {
//             first = Overload{std::forward<Args>(args)...};
//             return first;
//         }
//     }
// };

/******************************************************************************/

// template <class R, class ...Ts>
// struct AnnotatedCallback {
//     Caller caller;
//     Value function;

//     AnnotatedCallback() = default;
//     AnnotatedCallback(Value f, Caller c) : function(std::move(f)), caller(std::move(c)) {}

//     R operator()(Ts ...ts) const;
//     //  {
//     //     return function(caller, to_arguments(static_cast<Ts &&>(ts)...)).cast(Type<R>());
//     // }
// };

// template <class R>
// struct Callback {
//     Caller caller;
//     Value function;

//     Callback() = default;
//     Callback(Value f, Caller c) : function(std::move(f)), caller(std::move(c)) {}

//     template <class ...Ts>
//     R operator()(Ts &&...ts) const;
//     // {
//     //     return function(caller, to_arguments(static_cast<Ts &&>(ts)...)).cast(Type<R>());
//     // }
// };

/******************************************************************************/


// template <std::size_t N, class ...Ts, class R>
// Overload construct(Type<R>) {
//     Overload out;
//     out.emplace<N>(PartialConstruct<N, R, Ts...>());
//     return out;
// }


/******************************************************************************/

// template <class R>
// struct FromRef<Callback<R>> {
//     std::optional<Callback<R>> operator()(Ref const &v, Scope &msg) const {
//         if (!msg.caller) msg.error("Calling context expired", typeid(Callback<R>));
//         else if (auto p = v.from_ref<Overload>(msg)) return Callback<R>{std::move(*p), msg.caller};
//         return {};
//     }
// };


// template <class R, class ...Ts>
// struct FromRef<AnnotatedCallback<R, Ts...>> {
//     using type = AnnotatedCallback<R, Ts...>;
//     std::optional<type> operator()(Variable const &v, Scope &msg) const {
//         if (!msg.caller) msg.error("Calling context expired", typeid(type));
//         else if (auto p = v.from_ref<Overload>(msg)) return type{std::move(*p), msg.caller};
//         return {};
//     }
// };

/******************************************************************************/

}
