#pragma once
#include <sfb/Impl.h>

namespace sfb {

/******************************************************************************/

template <class Op, class T>
struct Trait;

template <class T>
struct Trait<Hash, T> {
    std::size_t hash() const {return hash(static_cast<T const&>(*this).as_ref());}
};

/******************************************************************************/

}
