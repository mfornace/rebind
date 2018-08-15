/**
 * @brief C++ type-erased Value object
 * @file Value.h
 */

#pragma once
#include <variant>
#include <complex>
#include <string>
#include <vector>
#include <type_traits>
#include <string_view>
#include <any>
#include <memory>

#define CPY_CAT_IMPL(s1, s2) s1##s2
#define CPY_CAT(s1, s2) CPY_CAT_IMPL(s1, s2)

#define CPY_STRING_IMPL(x) #x
#define CPY_STRING(x) CPY_STRING_IMPL(x)

namespace cpy {

struct BaseContext {};

/******************************************************************************/

struct ClientError : std::exception {
    std::string_view message;
    explicit ClientError(std::string_view const &s) noexcept : message(s) {}
    char const * what() const noexcept override {return message.empty() ? "cpy::ClientError" : message.data();}
};

/******************************************************************************/

struct Value;

using Function = std::function<Value(BaseContext &, std::vector<Value> &)>;

struct Binary {
    std::shared_ptr<char const> data;
    std::size_t size = 0;
};

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
    // Complex,   // ?
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

    Value(std::monostate={}) noexcept;
    Value(bool)              noexcept;
    Value(Integer)           noexcept;
    Value(Real)              noexcept;
    // Value(Complex)           noexcept;
    Value(std::string)       noexcept;
    Value(std::string_view)  noexcept;
    Value(std::in_place_t, Any) noexcept;
    Value(Function) noexcept;
    Value(Vector<Value>)             noexcept;

    bool             as_bool()    const &;
    Integer          as_integer() const &;
    Real             as_real()    const &;
    // Complex          as_complex() const &;
    std::string_view as_view()    const &;
    std::string      as_string()  const &;
    Any              as_any()     const &;
    Vector<Value>    as_vector()  const &;
    Binary           as_binary()  const &;

    Any              as_any()    &&;
    std::string      as_string() &&;
    Vector<Value>    as_vector() &&;
    Binary           as_binary() &&;

};

struct KeyPair {
    std::string_view key;
    Value value;
};

using ArgPack = Vector<Value>;

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

template <class T>
struct ToValue<T, std::enable_if_t<(std::is_floating_point_v<T>)>> {
    Value operator()(T t) const {return static_cast<Real>(t);}
};

template <class T>
struct ToValue<T, std::enable_if_t<(std::is_integral_v<T>)>> {
    Value operator()(T t) const {return static_cast<Integer>(t);}
};

template <>
struct ToValue<char const *> {
    Value operator()(char const *t) const {return std::string_view(t);}
};

template <class T, class Alloc>
struct ToValue<std::vector<T, Alloc>> {
    Value operator()(std::vector<T, Alloc> t) const {
        std::vector<Value> vec;
        for (auto &&i : t) vec.emplace_back(ToValue<T>()(i));
        return vec;
    }
};


/******************************************************************************/

template <class T, class=void>
struct is_valuable
    : std::false_type {};

template <class T>
struct is_valuable<T, std::void_t<decltype(ToValue<T>()(std::declval<T>()))>>
    : std::true_type {};

template <class T> static constexpr bool is_valuable_v = is_valuable<T>::value;

template <class T, std::enable_if_t<is_valuable_v<std::decay_t<T>>, int> = 0>
Value make_value(T &&t) {return ToValue<std::decay_t<T>>()(static_cast<T &&>(t));}

/// @todo fix
template <class T, std::enable_if_t<!is_valuable_v<std::decay_t<T>>, int> = 0>
Value make_value(T &&t) {
    std::ostringstream os;
    os << static_cast<T &&>(t);
    return std::move(os).str();
}

/******************************************************************************/

}
