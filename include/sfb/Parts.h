#pragma once
#include "Index.h"
#include "Impl.h"
#include <optional>

/******************************************************************************************/

namespace sfb {

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

template <class T, bool Check>
struct Output;

namespace parts {

template <int N, class F, class ...Ts>
decltype(auto) with_args(F &&f, Ts &&...ts);


inline void swap(Index i, Pointer l, Pointer r) noexcept {
    Swap::invoke(i, l, r);
}


inline std::size_t hash(Index i, Pointer self) noexcept {
    std::size_t out;
    Hash::invoke(i, out, self);
    return out;
}


inline bool equal(Index i, Pointer l, Pointer r) noexcept {
    return Equal::invoke(i, l, r) == Equal::True;
}


inline bool less(Index i, Pointer l, Pointer r) noexcept {
    return Compare::invoke(i, l, r) == Compare::Less;
}


template <class T>
T attribute(Index i, Pointer self, Mode mode, std::string_view) noexcept;


template <class T>
T element(Index i, Pointer self, Mode mode, std::intptr_t) noexcept;


}

/******************************************************************************************/

}
