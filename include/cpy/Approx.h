#pragma once
#include <type_traits>
#include <algorithm>
#include <cmath>

/******************************************************************************/

/// For 2 types, return the smallest floating point type that can be deduced from either
template <class L, class R, class=void>
struct ApproxType;

template <class T>
struct ApproxType<T, T> {using type = T;};

template <class L, class R>
struct ApproxType<L, R, std::enable_if_t<(std::is_integral<L>::value)>> {using type = R;};

template <class L, class R>
struct ApproxType<L, R, std::enable_if_t<(std::is_integral<R>::value)>> {using type = L;};

template <class L, class R>
struct ApproxType<L, R, std::enable_if_t<(std::is_floating_point<L>::value && std::is_floating_point<R>::value)>> {
    using type = std::conditional_t<std::numeric_limits<R>::epsilon() < std::numeric_limits<L>::epsilon(), R, L>;
};

/******************************************************************************/

template <class T, class=void>
struct ApproxEquals;

template <class T>
struct ApproxEquals<T, std::enable_if_t<(std::is_floating_point<T>::value)>> {
    static T const epsilon;
    static T const scale;

    bool operator()(T const &l, T const &r) const {
        if (l == r) return true; // i.e. for exact matches including infinite numbers
        return std::abs(l - r) < epsilon * (scale + std::max(std::abs(l), std::abs(r)));
    }
};

template <class T>
T const ApproxEquals<T, std::enable_if_t<(std::is_floating_point<T>::value)>>::scale = 1;

template <class T>
T const ApproxEquals<T, std::enable_if_t<(std::is_floating_point<T>::value)>>::epsilon = std::sqrt(std::numeric_limits<T>::epsilon());

/******************************************************************************/
