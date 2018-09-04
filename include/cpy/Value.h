/**
 * @brief C++ type-erased Value object
 * @file Value.h
 */

#pragma once
#include "Error.h"
#include "Signature.h"
#include "Common.h"

#include <iostream>
#include <string>
#include <vector>
#include <type_traits>
#include <string_view>
#include <any>
#include <memory>
#include <typeindex>

#define CPY_CAT_IMPL(s1, s2) s1##s2
#define CPY_CAT(s1, s2) CPY_CAT_IMPL(s1, s2)

#define CPY_STRING_IMPL(x) #x
#define CPY_STRING(x) CPY_STRING_IMPL(x)

namespace cpy {

/******************************************************************************/

using Binary = std::basic_string<unsigned char>;

using BinaryView = std::basic_string_view<unsigned char>;

#ifdef INTPTR_MAX
using Integer = std::intptr_t;
#else
using Integer = std::ptrdiff_t;
#endif

using Real = double;


template <class T, class=void>
struct ToValue;


struct Value {
    std::any any;
    constexpr Value() = default;

    template <class T>
    Value(std::in_place_t, T &&t) : any(static_cast<T &&>(t)) {}

    Value(std::in_place_t, Value &&t) noexcept : Value(std::move(t)) {}

    Value(std::in_place_t, Value const &t) : Value(t) {}

    template <class T, std::enable_if_t<!std::is_same_v<no_qualifier<T>, Value>, int> = 0>
    Value(T &&t) : Value(std::in_place_t(), ToValue<no_qualifier<T>>()(static_cast<T &&>(t))) {}

    std::type_index type() const {return any.type();}
    bool has_value() const {return any.has_value();}
};

/******************************************************************************/

template <class T>
T cast(Value const &v) {return std::any_cast<T>(v.any);}

template <class T>
T cast(Value &v) {return std::any_cast<T>(v.any);}

template <class T>
T const * cast(Value const *v) {return std::any_cast<no_qualifier<T>>(&v->any);}

template <class T>
T * cast(Value *v) {return std::any_cast<no_qualifier<T>>(&v->any);}

/******************************************************************************/

struct Sequence {
    Vector<Value> contents;
    Vector<std::size_t> shape;

    Sequence() = default;

    Sequence(std::initializer_list<Value> const &v) : contents(v) {}

    template <class ...Ts>
    static Sequence from_values(Ts &&...);

    template <class V>
    explicit Sequence(V &&);
};

using ArgPack = Vector<Value>;

/******************************************************************************/

template <class T>
class Reference {
    static_assert(std::is_reference_v<T>, "Only reference types can be wrapped");
    std::remove_reference_t<T> *self;
public:
    Reference(T t) noexcept : self(std::addressof(t)) {}
    T &&get() const noexcept {return static_cast<T &&>(*self);}
};

/******************************************************************************/

static_assert(32 == sizeof(Value));              // 8 + 24 buffer I think

/******************************************************************************/

struct KeyPair {
    std::string_view key;
    Value value;
};

/******************************************************************************/

/// The default implementation is to serialize to Value
template <class T, class>
struct ToValue {
    static_assert(!std::is_same_v<T, Value>);
    std::any operator()(T &&t) const {return {static_cast<T &&>(t)};}
    std::any operator()(T const &t) const {return {t};}
};

inline std::any to_value(std::nullptr_t) {return {};}

template <class T>
struct ToValue<T, std::void_t<decltype(to_value(Type<T>(), std::declval<T>()))>> {
    decltype(auto) operator()(T &&t) const {return to_value(Type<T>(), static_cast<T &&>(t));}
    decltype(auto) operator()(T const &t) const {return to_value(Type<T>(), t);}
};

template <class T>
struct ToValue<T, std::enable_if_t<(std::is_floating_point_v<T>)>> {
    Real operator()(T t) const {return static_cast<Real>(t);}
};

template <class T>
struct ToValue<T, std::enable_if_t<(std::is_integral_v<T>)>> {
    Integer operator()(T t) const {return static_cast<Integer>(t);}
};

template <>
struct ToValue<char const *> {
    std::string_view operator()(char const *t) const {return t;}
};

template <class T, class Alloc>
struct ToValue<std::vector<T, Alloc>> {
    Sequence operator()(std::vector<T, Alloc> t) const {return Sequence(std::move(t));}
};

/******************************************************************************/

/// The default implementation is to accept convertible arguments or Value of the exact typeid match
template <class T, class=void>
struct FromValue {
    // static_assert(!std::is_reference_v<T>);

    // Return casted type T from type U.
    no_qualifier<T> operator()(Value &&u, Dispatch &message) const {
        if (auto p = cast<Reference<Value const &>>(&u)) return cast<no_qualifier<T>>(p->get());
        if (auto p = cast<Reference<Value &>>(&u)) return cast<no_qualifier<T>>(p->get());
        if (auto p = cast<no_qualifier<T>>(&u)) return static_cast<T &&>(*p);
        throw message.error(u.has_value() ? "mismatched class" : "object was already moved", u.type(), typeid(T));
    }
};

template <class T>
struct FromValue<T &> {
    T & operator()(Value &&u, Dispatch &message) const {
        if (!u.has_value())
            throw message.error("object was already moved", u.type(), typeid(T));
        if (auto p = cast<Reference<Value &>>(&u)) {
            if (auto t = cast<T>(&p->get())) return *t;
            throw message.error("mismatched class", p->get().type(), typeid(T));
        }
        throw message.error("cannot form lvalue reference", u.type(), typeid(T));
    }
};


template <class T>
struct FromValue<T const &> {
    T const & operator()(Value &&u, Dispatch &message) const {
        if (!u.has_value())
            throw message.error("object was already moved", u.type(), typeid(T));
        if (auto p = cast<Reference<Value &>>(&u)) {
            if (auto t = cast<T>(&p->get())) return *t;
            throw message.error("mismatched class", u.type(), typeid(T));
        }
        if (auto p = cast<Reference<Value const &>>(&u)) {
            if (auto t = cast<T>(&p->get())) return *t;
            throw message.error("mismatched class", u.type(), typeid(T));
        }
        if (auto p = cast<no_qualifier<T>>(&u)) return *p;
        u.any = FromValue<T>()(std::move(u), message);
        return cast<T const &>(u);
    }
};

template <class T>
struct FromValue<T &&> {
    T && operator()(Value &&u, Dispatch &message) const {
        if (!u.has_value())
            throw message.error("object was already moved", u.type(), typeid(T));
        u.any = FromValue<no_qualifier<T>>()(std::move(u), message);
        return std::move(cast<T &>(u));
    }
};

template <class T>
struct FromValue<T, std::enable_if_t<std::is_arithmetic_v<T>>> {
    T operator()(Value const &u, Dispatch &message) const {
        auto t = u.type();
        if (t == typeid(bool)) return static_cast<T>(cast<bool>(u));
        if (t == typeid(Integer)) return static_cast<T>(cast<Integer>(u));
        if (t == typeid(Real)) return static_cast<T>(cast<Real>(u));
        throw message.error("not convertible to arithmetic value", u.type(), typeid(T));
    }
};

template <class T>
struct FromValue<T, std::void_t<decltype(from_value(+Type<T>(), Sequence(), std::declval<Dispatch &>()))>> {
    // The common return type between the following 2 visitor member functions
    using out_type = std::remove_const_t<decltype(false ? std::declval<T &&>() :
        from_value(Type<T>(), std::declval<Value &&>(), std::declval<Dispatch &>()))>;

    out_type operator()(Value &&u, Dispatch &message) const {
        auto ptr = &u;
        if (auto p = cast<Reference<Value &>>(&u)) ptr = &p->get();
        auto p = cast<no_qualifier<T>>(ptr);
        message.source = u.type();
        message.dest = typeid(T);
        return p ? static_cast<T>(*p) : from_value(+Type<T>(), std::move(*ptr), message);
    }
};

// static_assert(decltype(FromValue<double &>()(Value(), std::declval<Dispatch &>()))::aaa);

/******************************************************************************/

template <class V>
struct VectorFromValue {
    using T = no_qualifier<typename V::value_type>;

    V operator()(Value &&u, Dispatch &message) const {
        auto &&cts = cast<Sequence>(std::move(u)).contents;
        V out;
        out.reserve(cts.size());
        message.indices.emplace_back(0);
        for (auto &x : cts) {
            out.emplace_back(FromValue<T>()(std::move(x), message));
            ++message.indices.back();
        }
        message.indices.pop_back();
        return out;
    }
};

template <class T>
struct FromValue<Vector<T>> : VectorFromValue<Vector<T>> {};

/******************************************************************************/

template <class ...Ts>
Sequence Sequence::from_values(Ts &&...ts) {
    Sequence out;
    out.contents.reserve(sizeof...(Ts));
    (out.contents.emplace_back(static_cast<Ts &&>(ts)), ...);
    return out;
}

/******************************************************************************/

template <class V>
Sequence::Sequence(V &&v) {
    contents.reserve(std::size(v));
    for (auto &&x : v) contents.emplace_back(static_cast<decltype(x) &&>(x));
}

}
