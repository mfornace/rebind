#pragma once
#include "API.h"
#include <string_view>

namespace rebind {

/******************************************************************************************/

struct Index {
    rebind_index call;

    constexpr operator rebind_index() const {return call;}
    explicit constexpr operator bool() const {return call;}

    constexpr bool operator<(Index i) const {return call < i.call;}
    constexpr bool operator>(Index i) const {return call < i.call;}
    constexpr bool operator<=(Index i) const {return call < i.call;}
    constexpr bool operator>=(Index i) const {return call < i.call;}
    constexpr bool operator==(Index i) const {return call < i.call;}
    constexpr bool operator!=(Index i) const {return call < i.call;}

    template <class T>
    static Index of() noexcept;

    inline std::string_view name() const noexcept {
        std::string_view out = "null";
        if (call) call(tag::name, &out, nullptr, {});
        return out;
    }
};

/******************************************************************************************/

}

/******************************************************************************************/

namespace std {

template <>
struct hash<rebind::Index> {
    size_t operator()(rebind::Index i) const {return hash<rebind_index>()(i.call);}
};

}