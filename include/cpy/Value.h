#pragma once
#include <variant>
#include <complex>
#include <string>
#include <string_view>
#include <vector>
#include <type_traits>

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

    Value & operator=(Value &&v) noexcept;
    Value & operator=(Value const &v) noexcept;

    Value(Value &&) noexcept;
    Value(Value const &) noexcept;
    ~Value();
};

/*
 Add std::vector<>,              YES
 std::any,                       any YES. otoh couldn't I just any anyway
 std::function<ArgPack>?,        YEAH POSSIBLY BUT NEED TO FORWARD DECLARE THEN
 std::time_t,                    NO
 std::chrono::duration<double>,  BLEH need to import datetime then.
 wstring?                        NOT MUCH USE CASE I THINK
*/

struct KeyPair {
    std::string key;
    Value value;
};

using ArgPack = std::vector<Value>;

/******************************************************************************/

template <class T, class=void>
struct Valuable;

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
struct Valuable<T, std::enable_if_t<(std::is_floating_point<T>::value)>> {
    Value operator()(T t) const {return static_cast<double>(t);}
};

template <class T>
struct Valuable<T, std::enable_if_t<(std::is_integral<T>::value)>> {
    Value operator()(T t) const {
        if (std::is_signed<T>::value) return static_cast<std::ptrdiff_t>(t);
        else return static_cast<std::size_t>(t);
    }
};

template <>
struct Valuable<char const *> {
    Value operator()(char const *t) const {return std::string_view(t);}
};

/******************************************************************************/

}
