#pragma once
#include <variant>
#include <complex>
#include <string>
#include <sstream>
#include <vector>
#include <type_traits>
#include <string_view>
#include <iosfwd>

namespace cpy {

/******************************************************************************/

using Variant = std::variant<
    std::monostate,
    bool,
    std::size_t,
    std::ptrdiff_t,
    double,
    std::complex<double>,
    std::string,
    std::string_view

    // std::vector<bool>,
    // std::vector<std::size_t>,
    // std::vector<std::ptrdiff_t>,
    // std::vector<double>,
    // std::vector<std::complex<double>>,
    // std::vector<std::string>,
    // std::vector<std::string_view>
>;

struct Value {
    Variant var;

    Value(std::monostate={});
    Value(bool);
    Value(std::size_t);
    Value(std::ptrdiff_t);
    Value(double);
    Value(std::complex<double>);
    Value(std::string);
    Value(std::string_view);

    std::string_view as_view() const;
    double as_double() const;

    // Value(std::vector<bool>);
    // Value(std::vector<std::size_t>);
    // Value(std::vector<std::ptrdiff_t>);
    // Value(std::vector<double>);
    // Value(std::vector<std::complex<double>>);
    // Value(std::vector<std::string>);
    // Value(std::vector<std::string_view>);

    Value & operator=(Value &&v) noexcept;
    Value & operator=(Value const &v) noexcept;

    Value(Value &&) noexcept;
    Value(Value const &) noexcept;
    ~Value();
};

/*
 std::any,                       any YES. otoh couldn't I just any anyway
 std::function<ArgPack>?,        YEAH POSSIBLY BUT NEED TO FORWARD DECLARE THEN
 std::time_t,                    NO
 std::chrono::duration<double>,  BLEH need to import datetime then.
 wstring?                        NOT MUCH USE CASE I THINK
 maybe optional for parametrized tests?
*/

struct KeyPair {
    std::string_view key;
    Value value;
};

using ArgPack = std::vector<Value>;

/******************************************************************************/

template <class T, class=void>
struct Valuable; // undefined

// option 1: leave the above undefined -- user can define a default.
// option 5: leave undefined -- make_value uses stream if it is undefined
// option 3: define it to be the stream operator. then the user has to override the default based on a void_t
// problem then is, e.g. for std::any -- user would have to use std::is_copyable -- but then ambiguous with all their other overloads.
// option 2: define but static_assert(false) in it -- that's bad I think
// option 4: define a void_t stream operator. but then very hard to override


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
    Value operator()(T t) const {
        if (std::is_signed_v<T>) return static_cast<std::ptrdiff_t>(t);
        else return static_cast<std::size_t>(t);
    }
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

template <class T, std::enable_if_t<!is_valuable_v<std::decay_t<T>>, int> = 0>
Value make_value(T &&t) {
    std::ostringstream os;
    os << static_cast<T &&>(t);
    return std::move(os).str();
}

/******************************************************************************/

}
