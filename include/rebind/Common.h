#pragma once
#include "Signature.h"
#include <vector>
#include <iostream>
#include <sstream>

#define DUMP(...) ::rebind::dump(__FILE__, __LINE__, __VA_ARGS__);

namespace rebind {

inline std::ostream & operator<<(std::ostream &os, Qualifier q) {
    return os << QualifierNames[static_cast<unsigned char>(q)];
}

/******************************************************************************/

extern bool Debug;
void set_debug(bool debug) noexcept;
bool debug() noexcept;

char const * unknown_exception_description() noexcept;

/******************************************************************************/

/// Copy CV and reference qualifier from one type to another
template <class From, class To> struct copy_qualifier_t {using type = To;};
template <class From, class To> struct copy_qualifier_t<From &, To> {using type = To &;};
template <class From, class To> struct copy_qualifier_t<From const &, To> {using type = To const &;};
template <class From, class To> struct copy_qualifier_t<From &&, To> {using type = To &&;};

template <class From, class To> using copy_qualifier = typename copy_qualifier_t<From, To>::type;

/******************************************************************************/

/// For debugging purposes
template <class ...Ts>
void dump(char const *s, int n, Ts const &...ts) {
    if (!Debug) return;
    std::cout << '[' << s << ':' << n << "] ";
    int x[] = {(std::cout << ts, 0)...};
    std::cout << std::endl;
}

/******************************************************************************/

/// To avoid template type deduction on a given parameter
template <class T>
struct SameType {using type = T;};

/******************************************************************************/

// Ignore variable to use a parameter when the argument should be ignored
struct Ignore {
    template <class ...Ts>
    constexpr Ignore(Ts const &...ts) {}
};

/******************************************************************************/

template <class T>
using SizeOf = std::integral_constant<std::size_t, sizeof(T)>;

/// Binary search of an iterable of std::pair
template <class V>
auto binary_search(V const &v, typename V::value_type::first_type t) {
    auto it = std::lower_bound(v.begin(), v.end(), t,
        [](auto const &x, auto const &t) {return x.first < t;});
    return (it != v.end() && it->first == t) ? it : v.end();
}

/******************************************************************************/

template <class T>
using Vector = std::vector<T>;

template <class T, class ...Ts>
Vector<T> vectorize(Ts &&...ts) {
    Vector<T> out;
    out.reserve(sizeof...(Ts));
    (out.emplace_back(static_cast<Ts &&>(ts)), ...);
    return out;
}

template <class ...Ts>
struct ZipType {using type = std::tuple<Ts...>;};

template <class T, class U>
struct ZipType<T, U> {using type = std::pair<T, U>;};

template <class ...Ts>
using Zip = Vector<typename ZipType<Ts...>::type>;

template <class T, class V, class F>
Vector<T> mapped(V const &v, F &&f) {
    Vector<T> out;
    out.reserve(std::size(v));
    for (auto &&x : v) out.emplace_back(f(x));
    return out;
}

/******************************************************************************/

}
