#pragma once
#include "Type.h"
#include "Value.h"


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

    bool operator()(Value &v, T const &) const {
        DUMP("no conversion found from source ", fetch<T>(), " to value of type ", v.name());
        return false;
    }
};

/******************************************************************************/

template <class T>
struct ToValue<T, std::void_t<decltype(to_value(std::declval<Value &>(), std::declval<T>()))>> {
    static_assert(std::is_same_v<unqualified<T>, T>);
    using method = ADL;

    bool operator()(Value &v, T &t) const {return to_value(v, t);}
    bool operator()(Value &v, T const &t) const {return to_value(v, t);}
    bool operator()(Value &v, T &&t) const {return to_value(v, std::move(t));}
};

/******************************************************************************/

template <class T, class SFINAE=void> // T is unqualified
struct ToRef {
    using method = Default;
    static_assert(std::is_same_v<unqualified<T>, T>);

    bool operator()(Ref const &p, T const &) const {
        DUMP("no conversion found from source ", fetch<T>()->name(), " to reference of type ", p.name());
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

template <class T>
bool default_from_ref(Value &v, Ref const &p, Scope &s) {
    if (auto o = FromRef<T>()(p, s)) return v.place<T>(std::move(*o)), true;
    return false;
}

/******************************************************************************/

template <class T>
bool default_to_value(Value &v, void *p, Qualifier const q) {
    assert_usable<T>();
    if (q == Lvalue) {
        return ToValue<T>()(v, *static_cast<T *>(p));
    } else if (q == Rvalue) {
        return ToValue<T>()(v, std::move(*static_cast<T *>(p)));
    } else {
        return ToValue<T>()(v, *static_cast<T const *>(p));
    }
}

/******************************************************************************/

template <class T>
bool default_to_ref(Ref &v, void *p, Qualifier const q) {
    assert_usable<T>();
    if (q == Lvalue) {
        return ToRef<T>()(v, *static_cast<T *>(p));
    } else if (q == Rvalue) {
        return ToRef<T>()(v, std::move(*static_cast<T *>(p)));
    } else {
        return ToRef<T>()(v, *static_cast<T const *>(p));
    }
}

/******************************************************************************/

template <class T>
bool default_assign_if(void *ptr, Ref const &other) {
    assert_usable<T>();
    DUMP("assign_if: ", typeid(T).name());
    auto &self = *static_cast<T *>(ptr);
    if (auto p = other.request<T &&>()) {
        DUMP("assign_if: got T &&");
        self = std::move(*p);
    } else if (auto p = other.request<T const &>()) {
        DUMP("assign_if: got T const &");
        self = *p;
    } else if (auto p = other.request<T>()) {
        DUMP("assign_if: T succeeded, type=", typeid(*p).name());
        self = std::move(*p);
    } else {
        DUMP("assign_if: failed");
        return false;
    }
    return true;
}

/******************************************************************************/

namespace raw {

template <class T, std::enable_if_t<std::is_reference_v<T>, int>>
std::remove_reference_t<T> * request(Index i, void *p, Scope &s, Type<T>, Qualifier q) {
    using U = unqualified<T>;
    assert_usable<U>();

    if (!p) return nullptr;

    DUMP("raw::request reference ", fetch<U>()->name(qualifier_of<T>), " from ", i->name(), " ", QualifierSuffixes[q]);
    if (compatible_qualifier(q, qualifier_of<T>)) {
        if (auto o = target<U>(i, p)) return o;
        if (i->has_base(fetch<U>())) return static_cast<std::remove_reference_t<T> *>(p);
    }

    Ref out(fetch<U>(), nullptr, q);
    i->m_to_ref(out, p, q);
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
    Value v;
    v.as_raw().ind = fetch<T>();
    DUMP("calling m_to_value ", v.name());
    if (i->m_to_value(v, p, q)) {
        if (auto o = v.target<T>()) out.emplace(std::move(*o));
        DUMP("m_to_value succeeded");
        return out;
    }

    // FromRef
    out = FromRef<T>()(Ref(i, p, q), s);

    return out;
}

/******************************************************************************/

inline bool request_to(Index i, void *p, Value &v, Qualifier q) {
    if (p) return i->m_to_value(v, p, q);
    return false;
}

/******************************************************************************/

}

}
