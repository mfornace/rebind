#pragma once
#include "Index.h"
#include "Frame.h"
#include <optional>

/******************************************************************************************/

namespace ara {

/******************************************************************************************/

template <class T>
struct Maybe {
    using type = std::optional<T>;
    static constexpr auto none() {return std::nullopt;}
};

template <>
struct Maybe<void> {
    using type = void;
    static constexpr void none() {}
};

template <class T>
struct Maybe<T &> {
    using type = T *;
    static constexpr auto none() {return nullptr;}
};

template <class T>
struct Maybe<T &&> {
    using type = T *;
    static constexpr auto none() {return nullptr;}
};

template <class T>
using maybe = typename Maybe<T>::type;

/******************************************************************************************/

// template <class T, class Arg>
// void maybe_emplace(Maybe<T> &m, Arg &&t) {
//     if constexpr(std::is_reference_v<T>) m = std::addressof(t);
//     else m.emplace(static_cast<Arg &&>(t));
// }

/******************************************************************************************/

namespace parts {

template <class T, int N, class ...Ts>
T call(Index i, Tag qualifier, void *self, Caller &c, Ts &&...ts);

template <class T, int N, class ...Ts>
maybe<T> get(Index i, Tag qualifier, void *self, Caller &c, Ts &&...ts);

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