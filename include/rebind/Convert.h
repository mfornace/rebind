#pragma once
#include "Type.h"
#include "Value.h"
#include <stdexcept>


namespace rebind {
aaaa
/******************************************************************************/

/// Tags for method of Dump or Load specialization
struct Default {};
struct Specialized {};
struct ADL {};

/******************************************************************************/

/// The default behavior has no custom conversions
template <class T, class SFINAE=void>
struct Load {
    static_assert(std::is_same_v<unqualified<T>, T>);
    using method = Default;

    std::optional<T> operator()(Value const &r, Scope &msg) const {
        return msg.error("mismatched class type", Index::of<T>());
    }

    // constexpr bool operator()(Value const &r) const {return false;}
};

/******************************************************************************/

/// Unused overload just to register the from_ref identifier
// void from_ref(int, int, int);

/// ADL version of Load
// template <class T>
// struct Load<T, std::void_t<decltype(from_ref(Type<T>(), std::declval<Value const &>(), std::declval<Scope &>()))>> {
//     using method = ADL;

//     std::optional<T> operator()(Value const &r, Scope &msg) const {
//         return static_cast<std::optional<T>>(from_ref(Type<T>(), r, msg));
//     }
// };

/******************************************************************************/

/// Default response just tries implicit conversions
template <class T, class SFINAE=void>
struct Dump {
    using method = Default;
    static_assert(std::is_same_v<unqualified<T>, T>);

    bool operator()(Value &v, T const &) const {
        DUMP("no conversion found from source ", Index::of<T>(), " to value of type ", v.name());
        return false;
    }
};

/******************************************************************************/

// template <class T>
// struct Dump<T, std::void_t<decltype(to_value(std::declval<Output &>(), std::declval<T>()))>> {
//     static_assert(std::is_same_v<unqualified<T>, T>);
//     using method = ADL;

//     bool operator()(Output &v, T &t) const {return to_value(v, t);}
//     bool operator()(Output &v, T const &t) const {return to_value(v, t);}
//     bool operator()(Output &v, T &&t) const {return to_value(v, std::move(t));}
// };

/******************************************************************************/

// template <class T, class SFINAE=void> // T is unqualified
// struct ToRef {
//     using method = Default;
//     static_assert(std::is_same_v<unqualified<T>, T>);

//     bool operator()(Value const &p, T const &) const {
//         DUMP("no conversion found from source ", raw::name(Index::of<T>()), " to reference of type ", p.name());
//         return false;
//     }
// };

/******************************************************************************/

// template <class T, class=void>
// struct RequestMethod {using method = Specialized;};

// template <class T>
// struct RequestMethod<T, std::void_t<typename Load<T>::method>> {using type = typename Load<T>::method;};

// template <class T>
// using request_method = typename RequestMethod<T>::type;

/******************************************************************************/

// template <class T, class=void>
// struct ResponseMethod {using type = Specialized;};

// template <class T>
// struct ResponseMethod<T, std::void_t<typename Dump<T>::method>> {using type = typename Dump<T>::method;};

// template <class T>
// using response_method = typename ResponseMethod<T>::type;

/******************************************************************************/

// void lvalue_fails(Variable const &, Scope &, Index);
// void rvalue_fails(Variable const &, Scope &, Index);

/******************************************************************************/

namespace raw {

// template <class T, std::enable_if_t<std::is_reference_v<T>, int>>
// std::remove_reference_t<T> * request(Index i, void *p, Scope &s, Type<T>, Qualifier q) {
//     using U = unqualified<T>;
//     assert_usable<U>();

//     if (!p) return nullptr;

//     DUMP("raw::request reference ", raw::name(Index::of<U>()), " ", qualifier_of<T>, " from ", raw::name(i), " ", q);
//     if (compatible_qualifier(q, qualifier_of<T>)) {
//         if (auto o = target<U>(i, p)) return o;
//         // if (i->has_base(Index::of<U>())) return static_cast<std::remove_reference_t<T> *>(p);
//     }

//     Value out(Index::of<U>(), nullptr, q);
//     raw::request_to(out, i, p, q);
//     return out.target<T>();
// }

/******************************************************************************/

// template <class T, std::enable_if_t<!std::is_reference_v<T>, int>>
// std::optional<T> request(Index i, void *p, Scope &s, Type<T>, Qualifier q) {
//     assert_usable<T>();
//     std::optional<T> out;

//     if (!p) return out;

//     if (auto o = request(i, p, s, Type<T &&>(), q)) {
//         out.emplace(std::move(*o));
//         return out;
//     }

//     if constexpr(std::is_copy_constructible_v<T>) {
//         if (auto o = request(i, p, s, Type<T const &>(), q)) {
//             out.emplace(*o);
//             return out;
//         }
//     }

//     // Dump
//     Output v{Type<T>()};
//     DUMP("requesting via to_value ", v.name());
//     if (stat::request::ok == raw::request_to(v, i, p, q)) {
//         if (v.index() == Index::of<T>()) // target not supported, so just use raw version
//             out.emplace(std::move(*static_cast<T *>(v.address())));
//         DUMP("request via to_value succeeded");
//         return out;
//     }

//     // Load
//     out = Load<T>()(Value(i, p, q), s);

//     return out;
// }

/******************************************************************************/

// inline bool call_to(Value &v, Index i, Qualifier q, Storage const &s, ArgView args) noexcept {
//     DUMP("raw::call_to value: name=", i.name(), " size=", args.size());
//     return stat::call::in_place == stat::call(i(tag::call, &v, const_cast<Storage *>(&s), args));
// }

// template <class ...Args>
// inline Value call_value(Index i, Qualifier q, Storage const &s, Caller &&c, Args &&...args) {
//     Value v;
//     if (!call_to(v, i, q, s, to_arguments(c, static_cast<Args &&>(args)...)))
//         throw std::runtime_error("function could not yield Value");
//     return v;
// }

/******************************************************************************/

}

/******************************************************************************/

// template <class ...Args>
// Value Value::operator()(Args &&...args) const {
//     return raw::call_value(index(), qualifier(), storage, Caller(), static_cast<Args &&>(args)...);
// }

inline bool Value::call_to(Value &r, ArgView args) const {
    return has_value() && raw::call_to(r, index(), qualifier(), storage, std::move(args));
}

/******************************************************************************/

}
