#pragma once
#include "Index.h"
#include "Frame.h"
#include <optional>

/******************************************************************************************/

namespace rebind {

/******************************************************************************************/

template <class T>
using Maybe = std::conditional_t<std::is_void_v<T>, void,
    std::conditional_t<std::is_reference_v<T>, std::remove_reference_t<T> *, std::optional<T>>>;

template <class T, class Arg>
void maybe_emplace(Maybe<T> &m, Arg &&t) {
    if constexpr(std::is_reference_v<T>) m = std::addressof(t);
    else m.emplace(static_cast<Arg &&>(t));
}

/******************************************************************************************/

namespace parts {

template <class T, class ...Ts>
Maybe<T> call(Index i, Tag t, void *self, Caller &c, Ts &&...ts);


template <class T, class ...Args>
T * alloc(Args &&...args) {
    assert_usable<T>();
    if constexpr(std::is_constructible_v<T, Args &&...>) {
        return new T(static_cast<Args &&>(args)...);
    } else {
        return new T{static_cast<Args &&>(args)...};
    }
}

template <class T, class ...Args>
T * alloc_to(void *p, Args &&...args) {
    assert_usable<T>();
    if constexpr(std::is_constructible_v<T, Args &&...>) {
        return new(p) T(static_cast<Args &&>(args)...);
    } else {
        return new(p) T{static_cast<Args &&>(args)...};
    }
}

}

/******************************************************************************************/

}