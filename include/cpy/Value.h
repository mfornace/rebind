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
struct ToArg;

template <class T, class=void>
struct ToValue;

// template <class T>
// Value

struct Arg : std::any {
    constexpr Arg() = default;

    template <class T>
    Arg(std::in_place_t, T &&t) : any(static_cast<T &&>(t)) {}

    template <class T>
    static Arg from_any(T &&t) {return {std::in_place_t(), static_cast<T &&>(t)};}

    template <class T, class ...Ts>
    Arg(Type<T> t, Ts &&...ts) : any(std::in_place_type_t<T>(), static_cast<Ts &&>(ts)...) {}

    template <class T, std::enable_if_t<(!std::is_base_of_v<std::any, no_qualifier<T>>), int> = 0,
        std::enable_if_t<(std::is_base_of_v<Arg, decltype(ToArg<no_qualifier<T>>()(std::declval<T &&>()))>), int> = 0>
    Arg(T &&t) : Arg(ToArg<no_qualifier<T>>()(static_cast<T &&>(t))) {}

    std::any && base() && noexcept {return std::move(*this);}
    std::any const & base() const & noexcept {return *this;}
};

static_assert(std::is_copy_constructible_v<Arg>);
static_assert(std::is_move_constructible_v<Arg>);

/******************************************************************************/

struct Value : Arg {
    using Arg::Arg;

    Value(std::any) = delete;

    template <class T>
    static Value from_any(T &&t) {return {std::in_place_t(), static_cast<T &&>(t)};}

    template <class T, std::enable_if_t<(!std::is_base_of_v<std::any, no_qualifier<T>>), int> = 0,
        std::enable_if_t<(std::is_base_of_v<Value, decltype(ToValue<no_qualifier<T>>()(std::declval<T &&>()))>), int> = 0>
    Value(T &&t) : Value(ToValue<no_qualifier<T>>()(static_cast<T &&>(t))) {}
};

static_assert(!std::is_constructible_v<Value, Arg>);
static_assert(std::is_constructible_v<Arg, Value>);

/******************************************************************************/

template <class T>
class Reference {
    static_assert(std::is_reference_v<T>, "Only reference types can be wrapped");
    std::remove_reference_t<T> *self;
public:
    Reference(T t) noexcept : self(std::addressof(t)) {}
    T get() const noexcept {return static_cast<T>(*self);}
};

/******************************************************************************/

static_assert(32 == sizeof(Value));              // 8 + 24 buffer I think

/******************************************************************************/

template <class T, class>
struct ToArg {
    static_assert(!std::is_base_of_v<T, Arg>);

    Arg operator()(T &t) const {
        if (Debug) std::cout << "ref " << typeid(T &).name() << std::endl;
        return {Type<Reference<T &>>(), t};
    }
    Arg operator()(T const &t) const {
        if (Debug) std::cout << "cref " << typeid(T const &).name() << std::endl;
        return {Type<Reference<T const &>>(), t};
    }
    Arg operator()(T &&t) const {
        if (Debug) std::cout << "rref " << typeid(T).name() << std::endl;
        return ToValue<T>()(std::move(t));
    }
};

struct ToArgFromAny {
    template <class T>
    Arg operator()(T t) const {
        if (Debug) std::cout << "convert directly to arg " << typeid(T).name() << std::endl;
        return Arg::from_any(std::move(t));}
};

template <>
struct ToArg<Reference<Value &>> : ToArgFromAny {};

template <>
struct ToArg<Reference<Value const &>> : ToArgFromAny {};


/// The default implementation is to serialize to Value
template <class T, class>
struct ToValue {
    static_assert(!std::is_base_of_v<T, Arg>);

    Value operator()(T &&t) const {return Value::from_any(static_cast<T &&>(t));}
    Value operator()(T const &t) const {return Value::from_any(t);}
};

inline Value to_value(std::nullptr_t) {return {};}

template <class T>
struct ToValue<T, std::void_t<decltype(to_value(std::declval<T>()))>> {
    decltype(auto) operator()(T &&t) const {return to_value(static_cast<T &&>(t));}
    decltype(auto) operator()(T const &t) const {return to_value(t);}
};

/******************************************************************************/

template <class T, class=void>
struct FromValue {
    T operator()(Value const &v, Dispatch &msg) {
        if (Debug) std::cout << v.type().name() << " " << typeid(T).name() << std::endl;
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
template <class T, class=void>
struct FromArg {
    T operator()(Arg &out, Arg const &in, Dispatch &msg) const {
        if (auto t = std::any_cast<T>(&in))
            return *t;
        return FromValue<T>()(Value::from_any(in.base()), msg);
    }
    T operator()(Arg &out, Arg &&in, Dispatch &msg) const {
        if (Debug) std::cout << "casting FromArg " << in.type().name() << " to " <<  typeid(T).name() << std::endl;
        if (auto t = std::any_cast<T>(&in))
            return std::move(*t);
        if (auto p = std::any_cast<Reference<T const &>>(&in))
            return p->get();
        if (auto p = std::any_cast<Reference<T &>>(&in))
            return p->get();
        if (auto p = std::any_cast<Reference<Value const &>>(&in))
            return (*this)(out, p->get(), msg);
        if (auto p = std::any_cast<Reference<Value &>>(&in)) {
            if (Debug) std::cout << "casting reference " << p->get().type().name() << std::endl;
            return (*this)(out, p->get(), msg);
        }
        return FromValue<T>()(Value::from_any(std::move(in).base()), msg);
    }
};

template <class T>
struct FromArg<T &> {
    T & operator()(Arg &out, Arg &in, Dispatch &msg) const {
        if (!in.has_value())
            throw msg.error("object was already moved", in.type(), typeid(T));
        /// Must be passes by & wrapper
        if (auto p = std::any_cast<Reference<Value &>>(&in)) {
            if (auto t = std::any_cast<T>(&p->get())) return *t;
            return FromArg<T &>()(out, std::move(p->get()), msg);
        }
        return FromValue<T &>()(Value::from_any(in.base()), msg);
    }

    T & operator()(Arg &out, Arg &&in, Dispatch &msg) const {
        if (!in.has_value())
            throw msg.error("object was already moved", in.type(), typeid(T));
        /// Must be passes by & wrapper
        if (auto p = std::any_cast<Reference<Value &>>(&in)) {
            if (auto t = std::any_cast<T>(&p->get())) return *t;
            return FromArg<T &>()(out, p->get(), msg);
        }
        return FromValue<T &>()(Value::from_any(std::move(in).base()), msg);
    }
};

template <class T>
struct FromArg<T const &> {
    T const & operator()(Arg &out, Arg const &in, Dispatch &msg) const {
        if (auto p = std::any_cast<no_qualifier<T>>(&in))
            return *p;
        /// Check for & and const & wrappers
        if (auto p = std::any_cast<Reference<Value &>>(&in)) {
            if (auto t = std::any_cast<T>(&p->get())) return *t;
            return (*this)(out, p->get(), msg);
        }
        if (auto p = std::any_cast<Reference<Value const &>>(&in)) {
            if (auto t = std::any_cast<T>(&p->get())) return *t;
            return (*this)(out, p->get(), msg);
        }
        /// To bind a temporary to a const &, we store it in the out value
        return out.emplace<T>(FromArg<T>()(out, std::move(in), msg));
    }
};

template <class T>
struct FromArg<T &&> {
    T && operator()(Arg &out, Arg &&in, Dispatch &msg) const {
        /// No reference wrappers are used here, better to just move the Value in
        if (!in.has_value())
            throw msg.error("object was already moved", in.type(), typeid(T));
        /// To bind a temporary to a &&, we store it in the out value
        return std::move(out.emplace<T>(FromArg<no_qualifier<T>>()(out, std::move(in), msg)));
    }
};

template <class T>
T cast_value(Arg &&v, Dispatch &msg) {
    if (Debug) std::cout << "casting " << v.type().name() << " to " << typeid(T).name() << std::endl;
    if constexpr(std::is_convertible_v<Arg &&, T>) return static_cast<T>(std::move(v));
    return FromArg<T>()(v, std::move(v), msg);
}

template <class T>
T cast_value(Arg &&v) {
    Dispatch msg;
    return cast_value<T>(std::move(v), msg);
}

/******************************************************************************/

}
