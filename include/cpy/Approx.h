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
struct ApproxType<L, R, std::enable_if_t<(std::is_integral_v<L>)>> {using type = R;};

template <class L, class R>
struct ApproxType<L, R, std::enable_if_t<(std::is_integral_v<R>)>> {using type = L;};

template <class L, class R>
struct ApproxType<L, R, std::enable_if_t<(std::is_floating_point_v<L> && std::is_floating_point_v<R>)>> {
    using type = std::conditional_t<std::numeric_limits<R>::epsilon() < std::numeric_limits<L>::epsilon(), R, L>;
};

/******************************************************************************/

template <class T, class=void>
struct ApproxEquals;

template <class T>
struct ApproxEquals<T, std::enable_if_t<(std::is_floating_point_v<T>)>> {
    static constexpr T scale = 1;
    static constexpr T epsilon = T(1) / (1 << std::numeric_limits<T>::digits / 2);

    bool operator()(T const &l, T const &r) const {
        if (l == r) return true; // i.e. for exact matches including infinite numbers
        return std::abs(l - r) < epsilon * (scale + std::max(std::abs(l), std::abs(r)));
    }
};

template <class T>
T const ApproxEquals<T, std::enable_if_t<(std::is_floating_point_v<T>)>>::scale;

template <class T>
T const ApproxEquals<T, std::enable_if_t<(std::is_floating_point_v<T>)>>::epsilon;

/******************************************************************************/
