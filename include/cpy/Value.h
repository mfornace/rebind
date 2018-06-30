#pragma once
#include <variant>
#include <complex>
#include <string>
#include <vector>
#include <type_traits>
#include <string_view>

namespace cpy {

/******************************************************************************/

struct Value;

using Integer = std::ptrdiff_t;

using Real = double;

using Complex = std::complex<double>;

template <class T>
using Vector = std::vector<T>;

using Variant = std::variant<
    std::monostate,
    bool,
    Integer,
    Real,
    Complex,
    std::string_view,
    std::string,

    Vector<bool>,
    Vector<Integer>,
    Vector<Real>,
    Vector<Complex>,
    Vector<std::string>,
    Vector<std::string_view>,
    Vector<Value>
>;

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
    Value(Complex)           noexcept;
    Value(std::string)       noexcept;
    Value(std::string_view)  noexcept;

    Value(Vector<bool>)             noexcept;
    Value(Vector<Integer>)          noexcept;
    Value(Vector<Real>)             noexcept;
    Value(Vector<Complex>)          noexcept;
    Value(Vector<std::string>)      noexcept;
    Value(Vector<std::string_view>) noexcept;
    Value(Vector<Value>)            noexcept;

    bool             as_bool()    const &;
    Integer          as_integer() const &;
    Real             as_real()    const &;
    Complex          as_complex() const &;
    std::string_view as_view()    const &;
    std::string      as_string()  const &;

    Vector<bool>             as_bools()     const &;
    Vector<Integer>          as_integers()  const &;
    Vector<Real>             as_reals()     const &;
    Vector<Complex>          as_complexes() const &;
    Vector<std::string>      as_strings()   const &;
    Vector<std::string_view> as_views()     const &;
    Vector<Value>            as_values()    const &;

    std::string              as_string()    &&;
    Vector<bool>             as_bools()     &&;
    Vector<Integer>          as_integers()  &&;
    Vector<Real>             as_reals()     &&;
    Vector<Complex>          as_complexes() &&;
    Vector<std::string>      as_strings()   &&;
    Vector<std::string_view> as_views()     &&;
    Vector<Value>            as_values()    &&;
};

struct KeyPair {
    std::string_view key;
    Value value;
};

using ArgPack = Vector<Value>;

/******************************************************************************/

template <class T, class=void>
struct ToValue; // undefined

template <>
struct ToValue<bool> {
    Value operator()(bool t) const {return t;}
};

template <>
struct ToValue<std::string> {
    Value operator()(std::string t) const {return std::move(t);}
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
