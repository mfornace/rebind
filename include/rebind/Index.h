#pragma once
#include "API.h"
#include <string_view>

namespace rebind {


struct Index {
    rebind_index call;

    constexpr operator rebind_index() const {return call;}
    explicit constexpr operator bool() const {return call;}
    // constexpr bool operator<(Index i) const {return x < i.x;}


    inline std::string_view name() const noexcept {
        std::string_view out = "null";
        if (call) call(tag::name, &out, nullptr, {});
        return out;
    }
};
// using Index = rebind_index;

/******************************************************************************************/

template <class T>
Index fetch() noexcept;


}