/**
 * @brief C++ type-erased Value object
 * @file Value.h
 */

#pragma once
#include "Error.h"
#include "Signature.h"
#include "Common.h"

#include <iostream>
#include <variant>
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

struct Value;

using Binary = std::basic_string<unsigned char>;

using BinaryView = std::basic_string_view<unsigned char>;

#ifdef INTPTR_MAX
using Integer = std::intptr_t;
#else
using Integer = std::ptrdiff_t;
#endif

using Real = double;

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

using Function = std::function<Value(Caller &, ArgPack)>;

using Any = std::any;

class AnyReference {
    Any *self;
public:
    AnyReference(Any &s) noexcept : self(&s) {}
    Any *get() const noexcept {return self;}
};

/******************************************************************************/

using ValuePack = Pack<
    /* 0 */ std::monostate,    // immutable is fine
    /* 1 */ bool,              // could be mutable but easy if not, no move
    /* 2 */ Integer,           // could be mutable but easy if not, no move
    /* 3 */ Real,              // could be mutable but easy if not, no move
    /* 9 */ Function,          // immutable is fine, move is nice but easy to exclude
    /* 5 */ std::string,       // a bit of usecase since it allows moving strings
    /* 4 */ std::string_view,  // mutable would be better
    /* 6 */ std::type_index,   // immutable
    /* 7 */ Binary,            // ... there is not much usecase because I don't know how to move the allocation
    /* 8 */ BinaryView,        // mutable would be better
    /* 0 */ Any,               // mutable would be better
    /* 1 */ Sequence           // container is immutable, items could be mutated though
>;

// string
// could conceivably want string, string_view, mutable_string_view
// binary
// mostly want binary_view, mutable_binary_view, but binary would be ok
// could conceivably want any, any_view, any_mut_view

using Variant = decltype(variant_type(ValuePack()));

static_assert(1  == sizeof(std::monostate));    // 1
static_assert(1  == sizeof(bool));              // 1
static_assert(8  == sizeof(Integer));           // ptrdiff_t
static_assert(8  == sizeof(Real));              // double
static_assert(16 == sizeof(std::string_view)); // start, stop
static_assert(24 == sizeof(std::string));      // start, stop, buffer
static_assert(8  == sizeof(std::type_index));   // size_t
static_assert(24 == sizeof(Binary));           // start, stop buffer?
static_assert(48 == sizeof(Function));         // 24 buffer + 8 pointer + 8 vtable?
static_assert(32 == sizeof(Any));              // 8 + 24 buffer I think
static_assert(24 == sizeof(Vector<Value>));    //
static_assert(64 == sizeof(Variant));
static_assert(48 == sizeof(Sequence));

/******************************************************************************/

template <class T, class=void>
struct ToValue;

template <class T>
struct InPlace {T value;};

using Value = Any;
// struct Value {
//     Variant var;

//     Value(Value &&v) noexcept : var(std::move(v.var)) {}
//     Value(Value const &v) : var(v.var) {}
//     Value(Value &v) : var(v.var) {}
//     ~Value() = default;

//     Value & operator=(Value const &v) {var = v.var; return *this;}
//     Value & operator=(Value &&v) noexcept {var = std::move(v.var); return *this;}

//     Value(std::monostate v={}) noexcept : var(v) {}
//     Value(bool v)              noexcept : var(v) {}
//     Value(Integer v)           noexcept : var(v) {}
//     Value(Real v)              noexcept : var(v) {}
//     Value(Function v)          noexcept : var(std::move(v)) {}
//     Value(Binary v)            noexcept : var(std::move(v)) {}
//     Value(std::string v)       noexcept : var(std::move(v)) {}
//     Value(std::string_view v)  noexcept : var(std::move(v)) {}
//     Value(std::type_index v)   noexcept : var(std::move(v)) {}
//     Value(Sequence v)          noexcept : var(std::move(v)) {}

//     template <class T>
//     Value(std::in_place_t, T &&t) noexcept : var(std::in_place_type_t<Any>(), static_cast<T &&>(t)) {}

//     template <class T>
//     Value(InPlace<T> &&t) noexcept : var(std::in_place_type_t<Any>(), static_cast<T>(t.value)) {}

//     template <class T, std::enable_if_t<!(std::is_same_v<no_qualifier<T>, Value>), int> = 0>
//     Value(T &&t) : Value(ToValue<no_qualifier<T>>()(static_cast<T &&>(t))) {}

//     /******************************************************************************/

//     Value & no_view();
//     bool as_bool() const {return std::get<bool>(var);}
//     Real as_real() const {return std::get<Real>(var);}
//     Integer as_integer() const {return std::get<Integer>(var);}
//     std::type_index as_type() const {return std::get<std::type_index>(var);}
//     // std::string_view as_view() const & {return std::get<std::string_view>(var);}
//     // Any as_any() const & {return std::get<Any>(var);}
//     // Any as_any() && {return std::get<Any>(std::move(var));}
//     // std::string as_string() const & {
//     //     if (auto s = std::get_if<std::string_view>(&var))
//     //         return std::string(*s);
//     //     return std::get<std::string>(var);
//     // }
//     // Binary as_binary() const & {return std::get<Binary>(var);}
//     // Binary as_binary() && {return std::get<Binary>(std::move(var));}
// };

struct KeyPair {
    std::string_view key;
    Value value;
};

/******************************************************************************/

/// The default implementation is to serialize to Any
template <class T, class>
struct ToValue {
    InPlace<T &&> operator()(T &&t) const {return {static_cast<T &&>(t)};}
    InPlace<T const &> operator()(T const &t) const {return {t};}
};

void to_value(std::nullptr_t);

template <class T>
struct ToValue<T, std::void_t<decltype(to_value(Type<T>(), std::declval<T const &>()))>> {
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
    std::string operator()(char const *t) const {return std::string(t);}
};

template <class T, class Alloc>
struct ToValue<std::vector<T, Alloc>> {
    Sequence operator()(std::vector<T, Alloc> t) const {return Sequence(std::move(t));}
};

/******************************************************************************/

/// The default implementation is to accept convertible arguments or Any of the exact typeid match
template <class T, class=void>
struct FromValue {
    static_assert(!std::is_reference_v<T>);

    Dispatch &message;
    // Return casted type T from type U
    template <class U>
    T && operator()(U &&u) const {
        static_assert(std::is_rvalue_reference_v<U &&>);
        if constexpr(std::is_constructible_v<T &&, U &&>) return static_cast<T &&>(static_cast<U &&>(u));
        else if constexpr(std::is_same_v<T, std::monostate> && std::is_default_constructible_v<T>) return T();
        throw message.error(typeid(U), typeid(T));
    }

    T && operator()(Any &&u) const {
        auto ptr = &u;
        if (auto p = std::any_cast<AnyReference>(&u)) ptr = p->get();
        if (auto p = std::any_cast<no_qualifier<T>>(ptr)) return static_cast<T &&>(*p);
        throw message.error(u.has_value() ? "mismatched class" : "object was already moved", u.type(), typeid(T));
    }
};

template <class T>
struct FromValue<T, std::void_t<decltype(from_value(+Type<T>(), Sequence(), std::declval<Dispatch &>()))>> {
    Dispatch &message;
    // The common return type between the following 2 visitor member functions
    using out_type = std::remove_const_t<decltype(false ? std::declval<T &&>() :
        from_value(Type<T>(), std::declval<Any &&>(), std::declval<Dispatch &>()))>;

    out_type operator()(Any &&u) const {
        auto ptr = &u;
        if (auto p = std::any_cast<AnyReference>(&u)) ptr = p->get();
        auto p = std::any_cast<no_qualifier<T>>(ptr);
        message.source = u.type();
        message.dest = typeid(T);
        return p ? static_cast<T>(*p) : from_value(+Type<T>(), std::move(*ptr), message);
    }

    template <class U>
    out_type operator()(U &&u) const {
        message.source = typeid(U);
        message.dest = typeid(T);
        return from_value(+Type<T>(), static_cast<U &&>(u), message);
    }
};

/******************************************************************************/

template <class V>
struct VectorFromValue {
    using T = typename V::value_type;
    Dispatch &message;

    V operator()(Sequence &&u) const {
        V out;
        out.reserve(u.contents.size());
        message.indices.emplace_back(0);
        for (auto &x : u.contents) {
            std::visit([&](auto &x) {
                out.emplace_back(FromValue<T>{message}(std::move(x)));
            }, x.var);
            ++message.indices.back();
        }
        message.indices.pop_back();
        return out;
    }

    template <class U>
    V operator()(U const &) const {
        throw message.error("expected sequence", typeid(U), typeid(V));
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
