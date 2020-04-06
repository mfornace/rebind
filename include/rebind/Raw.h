#pragma once
#include "API.h"
#include "Type.h"
#include "Index.h"
// #include "Common.h"
#include <optional>
// #include "Scope.h"

/******************************************************************************************/

//     // template <class T>
//     // void set(T &t) {
//     //     ind = Index::of<T>();
//     //     ptr = static_cast<void *>(std::addressof(t));
//     //     return t;
//     // }

//     template <class T>
//     T * reset(T *t) noexcept {ptr = t; ind = Index::of<T>(); return t;}
namespace rebind::raw {

/**************************************************************************************/

// template <class T>
// bool matches(Index ind, Type<T> t={}) noexcept {
//     static_assert(is_usable<T>);
//     return ind == Index::of<unqualified<T>>();
// }

/**************************************************************************************/

template <class T>
T *target(Index i, void *p) noexcept {
    return (p && i == Index::of<T>()) ? static_cast<T *>(p) : nullptr;
}

/**************************************************************************************/

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


// inline stat::drop drop(void *data, Index i, bool heap) noexcept {
//     if (q == Heap) return stat::drop{i.call(tag::dealloc, data, nullptr, {})};
//     if (q == Stack) return stat::drop{i.call(tag::destruct, data, nullptr, {})};
// }

/**************************************************************************************/

// inline stat::copy copy(rebind_value &v, Index i, Qualifier q, Storage const &p) noexcept;
//  {
//     if (i) {
//         return stat::copy{i.call(tag::copy, &v, const_cast<void *>(&p), {})};
//     } else {
//         v.tagged_index = nullptr;
//         return stat::copy::ok;
//     }
// }

/**************************************************************************************/

// inline stat::assign assign(Index i, void *p, Value const &r) noexcept {
//     if (!p) return stat::assign::null;
//     return stat::assign{i.call(tag::assign, p, const_cast<Value *>(&r), {})};
// }

/******************************************************************************/

// inline stat::dump dump(Value &v, Index i, void *p, Qualifier q) noexcept {
//     if (!p) return stat::dump::null;
//     return stat::dump{i.call(tag::dump, &v, p, {})};
// }

/**************************************************************************************/

// template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
// std::remove_reference_t<T> * load(Index i, void *p, Scope &s, Type<T>, Qualifier q);

// template <class T, std::enable_if_t<!std::is_reference_v<T>, int> = 0>
// std::optional<T> load(Index i, void *p, Scope &s, Type<T>, Qualifier q);

/**************************************************************************************/

// bool call_to(Value &v, Index i, void const *p, Caller &&c, ArgView args) noexcept;

// template <class ...Args>
// Value call(Index i, void const *p, Caller &&c, Args &&...args);

/**************************************************************************************/

}