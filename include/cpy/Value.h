/**
 * @brief C++ type-erased Value object
 * @file Value.h
 */

#pragma once
#include "Error.h"

#include <iostream>
#include <variant>
#include <complex>
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

template <class T>
using no_qualifier = std::remove_cv_t<std::remove_reference_t<T>>;

struct Identity {
    template <class T>
    T const & operator()(T const &t) const {return t;}
};

struct BaseContext {
    std::any context;
    void *metadata = nullptr;
};

/******************************************************************************/

struct Value;

using Function = std::function<Value(BaseContext &, std::vector<Value> &)>;

using Binary = std::vector<char>;

using Integer = std::ptrdiff_t;

using Real = double;

using Complex = std::complex<double>;

using Any = std::any;

template <class T>
using Vector = std::vector<T>;

using Variant = std::variant<
    std::monostate,
    bool,
    Integer,
    Real,
    std::string_view,
    std::string,
    std::type_index,
    Binary,       // ?
    Function,
    Any,     // ?
    Vector<Value> // ?
>;

// static_assert( 1 == sizeof(bool));
// static_assert( 1 == sizeof(std::monostate));
// static_assert( 8 == sizeof(Integer));
// static_assert( 8 == sizeof(Real));
// static_assert(16 == sizeof(std::complex<double>));
// static_assert(16 == sizeof(std::string_view));
// static_assert(24 == sizeof(std::string));
// static_assert(24 == sizeof(Vector<bool>));
// static_assert(24 == sizeof(Vector<Value>));
// static_assert(32 == sizeof(Any));
// static_assert(16 == sizeof(std::shared_ptr<void const>));
// static_assert(24 == sizeof(Binary));
// static_assert(40 == sizeof(Variant));

struct Value {
    Variant var;
    Value & operator=(Value &&v) noexcept;
    Value & operator=(Value const &v);

    Value(Value &&) noexcept;
    Value(Value const &);
    ~Value();

    Value(std::monostate={})    noexcept;
    Value(bool)                 noexcept;
    Value(Integer)              noexcept;
    Value(Real)                 noexcept;
    Value(std::type_index)      noexcept;
    Value(Binary)               noexcept;
    Value(std::string)          noexcept;
    Value(std::string_view)     noexcept;
    Value(std::in_place_t, Any) noexcept;
    Value(Function)             noexcept;
    Value(Vector<Value>)        noexcept;

    bool             as_bool()    const &;
    Integer          as_integer() const &;
    Real             as_real()    const &;
    std::string_view as_view()    const &;
    std::string      as_string()  const &;
    Any              as_any()     const &;
    Vector<Value>    as_vector()  const &;
    Binary           as_binary()  const &;
    std::type_index  as_index()   const &;

    Any              as_any()    &&;
    std::string      as_string() &&;
    Vector<Value>    as_vector() &&;
    Binary           as_binary() &&;
};

struct KeyPair {
    std::string_view key;
    Value value;
};

/******************************************************************************/

using ArgPack = Vector<Value>;

WrongTypes wrong_types(ArgPack const &v);

/******************************************************************************/

template <class T, class=void>
struct ToValue {
    Value operator()(T t) const {return {std::in_place_t(), Any(std::move(t))};}
};

template <>
struct ToValue<bool> {
    Value operator()(bool t) const {return t;}
};

template <>
struct ToValue<std::string> {
    Value operator()(std::string t) const {return std::move(t);}
};

template <>
struct ToValue<Any> {
    Value operator()(Any t) const {return {std::in_place_t(), std::move(t)};}
};

template <>
struct ToValue<std::string_view> {
    Value operator()(std::string_view t) const {return std::move(t);}
};

template <>
struct ToValue<std::type_index> {
    Value operator()(std::type_index t) const {return std::move(t);}
};

template <>
struct ToValue<Binary> {
    Value operator()(Binary t) const {return std::move(t);}
};

template <class T>
struct ToValue<T, std::enable_if_t<(std::is_floating_point_v<T>)>> {
    Value operator()(T t) const {return static_cast<Real>(t);}
};

template <class T>
struct ToValue<T, std::enable_if_t<(std::is_integral_v<T>)>> {
    Value operator()(T t) const {return static_cast<Integer>(t);}
};

template <>
struct ToValue<Vector<Value>> {
    Value operator()(Vector<Value> t) const {return t;}
};

template <>
struct ToValue<char const *> {
    Value operator()(char const *t) const {return std::string(t);}
};

template <>
struct ToValue<Value>;

template <class T, class Alloc>
struct ToValue<std::vector<T, Alloc>> {
    Value operator()(std::vector<T, Alloc> t) const {
        Vector<Value> vec;
        vec.reserve(t.size());
        for (auto &&i : t) vec.emplace_back(ToValue<T>()(i));
        return vec;
    }
};

template <>
struct ToValue<Value> {
    Value operator()(Value v) const {return v;}
};

/******************************************************************************/

// template <class T, class=void>
// struct is_valuable
//     : std::false_type {};

// template <class T>
// struct is_valuable<T, std::void_t<decltype(ToValue<T>()(std::declval<T>()))>>
//     : std::true_type {};

// template <class T> static constexpr bool is_valuable_v = is_valuable<T>::value;

template <class T>//, std::enable_if_t<is_valuable_v<std::decay_t<T>>, int> = 0>
Value make_value(T const &t) {return ToValue<T>()(t);}

template <class T>//, std::enable_if_t<is_valuable_v<std::decay_t<T>>, int> = 0>
Value make_value(T &&t) {return ToValue<T>()(static_cast<T &&>(t));}

// /// @todo fix
// template <class T, std::enable_if_t<!is_valuable_v<std::decay_t<T>>, int> = 0>
// Value make_value(T &&t) {
//     std::ostringstream os;
//     os << static_cast<T &&>(t);
//     return std::move(os).str();
// }

/******************************************************************************/

template <class V, class F=Identity>
Vector<Value> vectorize(V const &v, F &&f) {
    Vector<Value> out;
    out.reserve(std::size(v));
    for (auto &&x : v) out.emplace_back(make_value(f(x)));
    return out;
}

/******************************************************************************/

static char const *cast_bug_message = "FromValue().check() returned false but FromValue()() was still called";

/// Default behavior for casting a variant to a desired argument type
template <class T, class=void>
struct FromValue {
    // Return true if type T can be cast from type U
    template <class U>
    constexpr bool check(U const &) const {
        return std::is_convertible_v<U &&, T> ||
            (std::is_same_v<T, std::monostate> && std::is_default_constructible_v<T>);
    }
    // Return casted type T from type U
    template <class U>
    T operator()(U &&u) const {
        if constexpr(std::is_convertible_v<U &&, T>) return static_cast<T>(static_cast<U &&>(u));
        else if constexpr(std::is_default_constructible_v<T>) return T(); // only hit if U == std::monostate
        else throw std::logic_error(cast_bug_message); // never get here
    }

    bool check(Any const &u) const {
        std::cout << "check" << bool(std::any_cast<no_qualifier<T>>(&u)) << std::endl;
        return std::any_cast<no_qualifier<T>>(&u);}

    T operator()(Any &&u) const {
        return static_cast<T>(std::any_cast<T>(u));
    }
    T operator()(Any const &u) const {
        throw std::logic_error("shouldn't be used");
    }
};

/******************************************************************************/

template <class T>
struct FromValue<Vector<T>> {
    template <class U>
    bool check(U const &) const {return false;}

    bool check(Vector<Value> const &u) const {
        for (auto const &x : u) if (!FromValue<T>().check(x)) return false;
        return true;
    }

    Vector<T> operator()(Vector<Value> &&u) const {
        Vector<T> out;
        for (auto &x : u) {
            std::visit([&](auto &x) {out.emplace_back(FromValue<T>()(std::move(x)));}, x.var);
        }
        return out;
    }

    template <class U>
    Vector<T> operator()(U const &) const {
        throw std::logic_error("shouldn't be used");
    }
};

/******************************************************************************/
}
