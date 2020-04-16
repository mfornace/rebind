#pragma once
#include <cstdint>
#include <iostream>
#include <sstream>

#define DUMP(...) ::rebind::dump(__FILE__, __LINE__, __VA_ARGS__);

namespace rebind {

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
constexpr std::uintptr_t aligned_location(std::uintptr_t i) {
    static_assert((alignof(T) & (alignof(T) - 1)) == 0, "not power of 2");
    auto rem = static_cast<std::uintptr_t>(alignof(T) - 1) & i;
    return rem ? i + alignof(T) - rem: i;
}

template <class T>
constexpr T* aligned_pointer(void const *p) {
    return reinterpret_cast<T *>(aligned_location<T>(reinterpret_cast<std::uintptr_t>(p)));
}


template <class T>
constexpr T* aligned_void(void *p) {
    return (alignof(T) <= alignof(void *)) ? static_cast<T*>(p) : aligned_pointer<T>(p);
}

/******************************************************************************/
}