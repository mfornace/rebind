#pragma once
#include "Test.h"
#include <optional>

namespace cpy {

template <class T>
struct CastVariant<std::optional<T>> {
    template <class U>
    bool check(U const &) const {
        return std::is_convertible_v<U &&, T> || std::is_same_v<U, std::monostate>;
    }

    template <class U, std::enable_if_t<(!std::is_convertible_v<U &&, T>), int> = 0>
    std::optional<T> operator()(U &u) const {return T();}

    template <class U, std::enable_if_t<(std::is_convertible_v<U &&, T>), int> = 0>
    std::optional<T> operator()(U &u) const {return static_cast<T>(std::move(u));}
};

}