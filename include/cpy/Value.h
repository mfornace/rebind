#pragma once
#include <variant>
#include <complex>
#include <string>
#include <vector>
#include <type_traits>
#include <string_view>

namespace cpy {

/******************************************************************************/

using Integer = std::ptrdiff_t;

using Variant = std::variant<
    std::monostate,
    bool,
    Integer,
    double,
    std::complex<double>,
    std::string,
    std::string_view
>;

struct Value {
    Variant var;

    Value(std::monostate={}) noexcept;
    Value(bool) noexcept;
    Value(Integer) noexcept;
    Value(double) noexcept;
    Value(std::complex<double>) noexcept;
    Value(std::string) noexcept;
    Value(std::string_view) noexcept;

    Value & operator=(Value &&v) noexcept;
    Value & operator=(Value const &v) noexcept;

    Value(Value &&) noexcept;
    Value(Value const &) noexcept;
    ~Value();

    bool as_bool() const;
    Integer as_integer() const;
    double as_double() const;
    std::string_view as_view() const;
    std::string as_string() const;
};

struct KeyPair {
    std::string_view key;
    Value value;
};

using ArgPack = std::vector<Value>;

/******************************************************************************/

template <class T, class=void>
struct Valuable; // undefined

template <>
struct Valuable<bool> {
    Value operator()(bool t) const {return t;}
};

template <>
struct Valuable<std::string> {
    Value operator()(std::string t) const {return std::move(t);}
};

template <>
struct Valuable<std::string_view> {
    Value operator()(std::string_view t) const {return std::move(t);}
};

template <class T>
struct Valuable<T, std::enable_if_t<(std::is_floating_point_v<T>)>> {
    Value operator()(T t) const {return static_cast<double>(t);}
};

template <class T>
struct Valuable<T, std::enable_if_t<(std::is_integral_v<T>)>> {
    Value operator()(T t) const {return static_cast<Integer>(t);}
};

template <>
struct Valuable<char const *> {
    Value operator()(char const *t) const {return std::string_view(t);}
};

/******************************************************************************/

template <class T, class=void>
struct is_valuable
    : std::false_type {};

template <class T>
struct is_valuable<T, std::void_t<decltype(Valuable<T>()(std::declval<T>()))>>
    : std::true_type {};

template <class T> static constexpr bool is_valuable_v = is_valuable<T>::value;

template <class T, std::enable_if_t<is_valuable_v<std::decay_t<T>>, int> = 0>
Value make_value(T &&t) {return Valuable<std::decay_t<T>>()(static_cast<T &&>(t));}

/// @todo fix
template <class T, std::enable_if_t<!is_valuable_v<std::decay_t<T>>, int> = 0>
Value make_value(T &&t) {
    std::ostringstream os;
    os << static_cast<T &&>(t);
    return std::move(os).str();
}

/******************************************************************************/

}
