/**
 * @brief C++ type-erased Value object
 * @file Value.h
 */

#pragma once
#include "Error.h"
#include "Signature.h"
#include "Common.h"

#include <iostream>
#include <vector>
#include <type_traits>
#include <string_view>
#include <any>
#include <typeindex>

#define CPY_CAT_IMPL(s1, s2) s1##s2
#define CPY_CAT(s1, s2) CPY_CAT_IMPL(s1, s2)

#define CPY_STRING_IMPL(x) #x
#define CPY_STRING(x) CPY_STRING_IMPL(x)

namespace cpy {

/******************************************************************************/

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

template <>
struct ToValue<char const *> {
    std::string_view operator()(char const *t) const {return t;}
};

/******************************************************************************/

template <class T, class=void>
struct FromValue {
    T operator()(Value const &v, Dispatch &msg) {
        throw msg.error("mismatched class type", v.type(), typeid(T));
    }
};

template <class T>
struct FromValue<T &> {
    T & operator()(Value const &v, Dispatch &msg) {
        throw msg.error("cannot form lvalue reference", v.type(), typeid(T));
    }
};

template <class T>
struct FromValue<T const &> {
    T const & operator()(Value const &v, Dispatch &msg) {
        throw msg.error("cannot form const lvalue reference", v.type(), typeid(T));
    }
};

template <class T>
struct FromValue<T &&> {
    T && operator()(Value const &v, Dispatch &msg) {
        throw msg.error("cannot form rvalue reference", v.type(), typeid(T));
    }
};

/******************************************************************************/

/// The default implementation is to accept convertible arguments or Value of the exact typeid match
/// castvalue always needs an output Value &, and an input Value
template <class T>
struct CastValue {
    T operator()(Value &out, Value const &in, Dispatch &msg) const {
        if (auto t = std::any_cast<T>(&in.any))
            return *t;
        if (auto p = std::any_cast<Reference<Value const &>>(&in.any))
            return (*this)(out, *p, msg);
        if (auto p = std::any_cast<Reference<Value &>>(&in.any))
            return (*this)(out, *p, msg);
        return FromValue<T>()(in, msg);
    }
    T operator()(Value &out, Value &&in, Dispatch &msg) const {
        if (auto t = std::any_cast<T>(&in.any))
            return std::move(*t);
        if (auto p = std::any_cast<Reference<Value const &>>(&in.any))
            return (*this)(out, *p, msg);
        if (auto p = std::any_cast<Reference<Value &>>(&in.any))
            return (*this)(out, *p, msg);
        return FromValue<T>()(std::move(in), msg);
    }
};

template <class T>
struct CastValue<T &> {
    T & operator()(Value &out, Value &&in, Dispatch &msg) const {
        if (!in.has_value())
            throw msg.error("object was already moved", in.type(), typeid(T));
        if (auto p = std::any_cast<Reference<Value &>>(&in.any)) {
            if (auto t = std::any_cast<T>(&p->get().any)) return *t;
            return FromValue<T &>()(p->get(), msg);
        }
        return FromValue<T &>()(std::move(in), msg);
    }
};

template <class T>
struct CastValue<T const &> {
    T const & operator()(Value &out, Value const &in, Dispatch &msg) const {
        if (auto p = std::any_cast<no_qualifier<T>>(&in.any))
            return *p;
        if (auto p = std::any_cast<Reference<Value &>>(&in.any)) {
            if (auto t = std::any_cast<T>(&p->get().any)) return *t;
            return (*this)(out, p->get(), msg);
        }
        if (auto p = std::any_cast<Reference<Value const &>>(&in.any)) {
            if (auto t = std::any_cast<T>(&p->get().any)) return *t;
            return (*this)(out, p->get(), msg);
        }
        return out.any.emplace<T>(CastValue<T>()(out, std::move(in), msg));
    }
};

template <class T>
struct CastValue<T &&> {
    T && operator()(Value &out, Value &&in, Dispatch &msg) const {
        if (!in.has_value())
            throw msg.error("object was already moved", in.type(), typeid(T));
        return std::move(out.any.emplace<T>(CastValue<no_qualifier<T>>()(out, std::move(in), msg)));
    }
};

template <class T>
T value_cast(Value &&v, Dispatch &msg) {
    return CastValue<T>()(v, std::move(v), msg);
}


template <class T>
T value_cast(Value &&v) {
    Dispatch msg;
    return CastValue<T>()(v, std::move(v), msg);
}

/******************************************************************************/

}
