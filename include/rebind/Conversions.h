#pragma once
#include "Type.h"
#include "Value.h"


namespace rebind {

/******************************************************************************/

/// Tags for method of ToValue or FromPointer specialization
struct Default {};
struct Specialized {};
struct ADL {};

/******************************************************************************/

/// The default behavior has no custom conversions
template <class T, class SFINAE=void>
struct FromPointer {
    static_assert(std::is_same_v<unqualified<T>, T>);
    using method = Default;

    std::optional<T> operator()(Pointer const &r, Scope &msg) const {
        return msg.error("mismatched class type", typeid(T));
    }

    constexpr bool operator()(Pointer const &r) const {return false;}
};

/******************************************************************************/

/// Unused overload just to register the from_pointer identifier
// void from_pointer(int, int, int);

/// ADL version of FromPointer
template <class T>
struct FromPointer<T, std::void_t<decltype(from_pointer(Type<T>(), std::declval<Pointer const &>(), std::declval<Scope &>()))>> {
    using method = ADL;

    std::optional<T> operator()(Pointer const &r, Scope &msg) const {
        return static_cast<std::optional<T>>(from_pointer(Type<T>(), r, msg));
    }
};

/******************************************************************************/

/// Default response just tries implicit conversions
template <class T, class SFINAE=void>
struct ToValue {
    using method = Default;
    static_assert(std::is_same_v<unqualified<T>, T>);

    bool operator()(Value &v, T const &) const {
        DUMP("no conversion found from source ", type_index<T>(), " to value of type ", v.name());
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
struct ToPointer {
    using method = Default;
    static_assert(std::is_same_v<unqualified<T>, T>);

    void operator()(Pointer const &p, T const &) const {
        DUMP("no conversion found from source ", get_table<T>()->name(), " to pointer of type ", p.name());
    }
};

/******************************************************************************/

template <class T, class=void>
struct RequestMethod {using method = Specialized;};

template <class T>
struct RequestMethod<T, std::void_t<typename FromPointer<T>::method>> {using type = typename FromPointer<T>::method;};

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
Value default_from_pointer(Pointer const &p, Scope &s) {
    Value v;
    if (auto o = FromPointer<T>()(p, s)) v.place<T>(std::move(*o));
    return v;
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
void default_to_pointer(Pointer &v, void *p, Qualifier const q) {
    assert_usable<T>();
    if (q == Lvalue) {
        return ToPointer<T>()(v, *static_cast<T *>(p));
    } else if (q == Rvalue) {
        return ToPointer<T>()(v, std::move(*static_cast<T *>(p)));
    } else {
        return ToPointer<T>()(v, *static_cast<T const *>(p));
    }
}

/******************************************************************************/

template <class T>
bool default_assign_if(void *ptr, Pointer const &other) {
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

template <class T, std::enable_if_t<std::is_reference_v<T>, int>>
std::remove_reference_t<T> * Erased::request(Scope &s, Type<T> t, Qualifier q) const {
    using U = unqualified<T>;
    assert_usable<U>();

    if (!has_value()) return nullptr;

    DUMP("Erased::request reference ", get_table<U>()->name(qualifier_of<T>), " from ", tab->name(), " ", QualifierSuffixes[q]);
    if (compatible_qualifier(q, qualifier_of<T>)) {
        if (auto p = target<U>()) return p;
        if (tab->has_base(type_index<U>())) return static_cast<std::remove_reference_t<T> *>(ptr);
    }

    Pointer out(Erased(static_cast<U *>(nullptr)), q);
    tab->m_to_pointer(out, ptr, q);
    return out.target<T>();
}

/******************************************************************************/

template <class T, std::enable_if_t<!std::is_reference_v<T>, int>>
std::optional<T> Erased::request(Scope &s, Type<T> t, Qualifier q) const {
    assert_usable<T>();
    std::optional<T> out;

    if (!has_value()) return out;

    if (auto p = request(s, Type<T &&>(), q)) {
        out.emplace(std::move(*p));
        return out;
    }

    if constexpr(std::is_copy_constructible_v<T>) {
        if (auto p = request(s, Type<T const &>(), q)) {
            out.emplace(*p);
            return out;
        }
    }

    // ToValue
    Value v;
    const_cast<Erased &>(v.as_erased()).tab = get_table<T>();
    DUMP("calling m_to_value ", v.name());
    if (tab->m_to_value(v, ptr, q)) {
        if (auto p = v.target<T>()) out.emplace(std::move(*p));
        DUMP("m_to_value succeeded");
        return out;
    }

    // FromPointer
    out = FromPointer<T>()(Pointer(*this, q), s);

    return out;
}

/******************************************************************************/

inline bool Erased::request_to(Value &v, Qualifier q) const {
    if (has_value()) return tab->m_to_value(v, ptr, q);
    return false;
}

/******************************************************************************/


}
