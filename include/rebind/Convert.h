#pragma once
#include "Type.h"
#include "Value.h"
#include <stdexcept>


namespace rebind {

/******************************************************************************/

/// Tags for method of ToValue or FromRef specialization
struct Default {};
struct Specialized {};
struct ADL {};

/******************************************************************************/

/// The default behavior has no custom conversions
template <class T, class SFINAE=void>
struct FromRef {
    static_assert(std::is_same_v<unqualified<T>, T>);
    using method = Default;

    std::optional<T> operator()(Ref const &r, Scope &msg) const {
        return msg.error("mismatched class type", fetch<T>());
    }

    constexpr bool operator()(Ref const &r) const {return false;}
};

/******************************************************************************/

/// Unused overload just to register the from_ref identifier
// void from_ref(int, int, int);

/// ADL version of FromRef
template <class T>
struct FromRef<T, std::void_t<decltype(from_ref(Type<T>(), std::declval<Ref const &>(), std::declval<Scope &>()))>> {
    using method = ADL;

    std::optional<T> operator()(Ref const &r, Scope &msg) const {
        return static_cast<std::optional<T>>(from_ref(Type<T>(), r, msg));
    }
};

/******************************************************************************/

/// Default response just tries implicit conversions
template <class T, class SFINAE=void>
struct ToValue {
    using method = Default;
    static_assert(std::is_same_v<unqualified<T>, T>);

    bool operator()(Output &v, T const &) const {
        DUMP("no conversion found from source ", fetch<T>(), " to value of type ", v.name());
        return false;
    }
};

/******************************************************************************/

template <class T>
struct ToValue<T, std::void_t<decltype(to_value(std::declval<Output &>(), std::declval<T>()))>> {
    static_assert(std::is_same_v<unqualified<T>, T>);
    using method = ADL;

    bool operator()(Output &v, T &t) const {return to_value(v, t);}
    bool operator()(Output &v, T const &t) const {return to_value(v, t);}
    bool operator()(Output &v, T &&t) const {return to_value(v, std::move(t));}
};

/******************************************************************************/

template <class T, class SFINAE=void> // T is unqualified
struct ToRef {
    using method = Default;
    static_assert(std::is_same_v<unqualified<T>, T>);

    bool operator()(Ref const &p, T const &) const {
        DUMP("no conversion found from source ", raw::name(fetch<T>()), " to reference of type ", p.name());
        return false;
    }
};

/******************************************************************************/

template <class T, class=void>
struct RequestMethod {using method = Specialized;};

template <class T>
struct RequestMethod<T, std::void_t<typename FromRef<T>::method>> {using type = typename FromRef<T>::method;};

template <class T>
using request_method = typename RequestMethod<T>::type;

/******************************************************************************/

template <class T, class=void>
struct ResponseMethod {using type = Specialized;};

template <class T>
struct ResponseMethod<T, std::void_t<typename ToValue<T>::method>> {using type = typename ToValue<T>::method;};

template <class T>
using response_method = typename ResponseMethod<T>::type;

/******************************************************************************/

// void lvalue_fails(Variable const &, Scope &, Index);
// void rvalue_fails(Variable const &, Scope &, Index);

/******************************************************************************/

namespace raw {

template <class T, std::enable_if_t<std::is_reference_v<T>, int>>
std::remove_reference_t<T> * request(Index i, void *p, Scope &s, Type<T>, Qualifier q) {
    using U = unqualified<T>;
    assert_usable<U>();

    if (!p) return nullptr;

    DUMP("raw::request reference ", raw::name(fetch<U>()), " ", qualifier_of<T>, " from ", raw::name(i), " ", q);
    if (compatible_qualifier(q, qualifier_of<T>)) {
        if (auto o = target<U>(i, p)) return o;
        // if (i->has_base(fetch<U>())) return static_cast<std::remove_reference_t<T> *>(p);
    }

    Ref out(fetch<U>(), nullptr, q);
    raw::request_to(out, i, p, q);
    return out.target<T>();
}

/******************************************************************************/

template <class T, std::enable_if_t<!std::is_reference_v<T>, int>>
std::optional<T> request(Index i, void *p, Scope &s, Type<T>, Qualifier q) {
    assert_usable<T>();
    std::optional<T> out;

    if (!p) return out;

    if (auto o = request(i, p, s, Type<T &&>(), q)) {
        out.emplace(std::move(*o));
        return out;
    }

    if constexpr(std::is_copy_constructible_v<T>) {
        if (auto o = request(i, p, s, Type<T const &>(), q)) {
            out.emplace(*o);
            return out;
        }
    }

    // ToValue
    Output v{Type<T>()};
    DUMP("requesting via to_value ", v.name());
    if (raw::request_to(v, i, p, q)) {
        if (v.index() == fetch<T>()) // target not supported, so just use raw version
            out.emplace(std::move(*static_cast<T *>(v.address())));
        DUMP("request via to_value succeeded");
        return out;
    }

    // FromRef
    out = FromRef<T>()(Ref(i, p, q), s);

    return out;
}

/******************************************************************************/

inline bool call_to(Value &v, Index i, void const *p, ArgView args) noexcept {
    DUMP("raw::call_to value: ptr=", p, " name=", raw::name(i), " size=", args.size());
    return p && i(tag::call_to_value, &v, const_cast<void *>(p), args);//, std::move(c), args);
}

inline bool call_to(Ref &r, Index i, void const *p, ArgView args) noexcept {
    DUMP("raw::call_to ref");
// static_assert(std::array<int, sizeof(Caller)>::aaa);
// static_assert(std::array<int, sizeof(std::string_view)>::aaa);
#warning "need to add caller, method name, tag?"
    return p && i(tag::call_to_ref, &r, const_cast<void *>(p), args);//, std::move(c), args);
}

template <class ...Args>
inline Value call_value(Index i, void const *p, Caller &&c, Args &&...args) {
    Value v;
    if (!call_to(v, i, p, to_arguments(c, static_cast<Args &&>(args)...)))
        throw std::runtime_error("function could not yield Value");
    return v;
}

template <class ...Args>
inline Ref call_ref(Index i, void const *p, Caller &&c, Args &&...args) {
    Ref r;
    if (!call_to(r, i, p, to_arguments(c, static_cast<Args &&>(args)...)))
        throw std::runtime_error("function could not yield Ref");
    return r;
}

/******************************************************************************/

}

template <class ...Args>
Value Ref::call_value(Args &&...args) const {return raw::call_value(index(), address(), Caller(), static_cast<Args &&>(args)...);}

template <class ...Args>
Ref Ref::call_ref(Args &&...args) const {return raw::call_ref(index(), address(), Caller(), static_cast<Args &&>(args)...);}

inline bool Ref::call_to(Ref &r, ArgView args) const noexcept {
    DUMP("Ref::call_to ref");
    return raw::call_to(r, index(), address(), std::move(args));
}

inline bool Ref::call_to(Value &r, ArgView args) const noexcept {
    DUMP("Ref::call_to value");
    return raw::call_to(r, index(), address(), std::move(args));
}

/******************************************************************************/

template <class ...Args>
Value Value::call_value(Args &&...args) const {return raw::call_value(index(), address(), Caller(), static_cast<Args &&>(args)...);}

template <class ...Args>
Ref Value::call_ref(Args &&...args) const {return raw::call_ref(index(), address(), Caller(), static_cast<Args &&>(args)...);}

inline bool Value::call_to(Ref &r, ArgView args) const {return raw::call_to(r, index(), address(), std::move(args));}

inline bool Value::call_to(Value &r, ArgView args) const {return raw::call_to(r, index(), address(), std::move(args));}

/******************************************************************************/

}
