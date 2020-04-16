#pragma once
#include <rebind/Signature.h>
#include <vector>

namespace rebind {

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

template <class T, class V, class F>
Vector<T> mapped(V const &v, F &&f) {
    Vector<T> out;
    out.reserve(std::size(v));
    for (auto &&x : v) out.emplace_back(f(x));
    return out;
}

/******************************************************************************/

template <class ...Ts>
struct ZipType {using type = std::tuple<Ts...>;};

template <class T, class U>
struct ZipType<T, U> {using type = std::pair<T, U>;};

template <class ...Ts>
using Zip = Vector<typename ZipType<Ts...>::type>;


}
