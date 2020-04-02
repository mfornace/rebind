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
//     //     ind = fetch<T>();
//     //     ptr = static_cast<void *>(std::addressof(t));
//     //     return t;
//     // }

//     template <class T>
//     T * reset(T *t) noexcept {ptr = t; ind = fetch<T>(); return t;}
namespace rebind::raw {

/**************************************************************************************/

// template <class T>
// bool matches(Index ind, Type<T> t={}) noexcept {
//     static_assert(is_usable<T>);
//     return ind == fetch<unqualified<T>>();
// }

/**************************************************************************************/

template <class T>
T *target(Index i, void *p) noexcept {
    return (p && i == fetch<unqualified<T>>()) ? static_cast<T *>(p) : nullptr;
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

/**************************************************************************************/

inline stat::copy copy(rebind_value &v, Index i, void const *p) noexcept {
    if (i) {
        stat::copy err{i.call(tag::copy, &v, const_cast<void *>(p), {})};
        if (stat::copy::ok == err) {
            v.ind = i;
        } else {
            v.ptr = nullptr;
            v.ind = nullptr;
        }
        return err;
    } else {
        v.ptr = nullptr;
        v.ind = nullptr;
        return stat::copy::ok;
    }
}

/**************************************************************************************/

inline stat::drop drop(rebind_value &v) noexcept {return stat::drop(v.ind(tag::drop, &v, nullptr, {}));}

/**************************************************************************************/

inline stat::assign_if assign_if(Index i, void *p, Ref const &r) noexcept {
    if (!p) return stat::assign_if::null;
    return stat::assign_if(i.call(tag::assign_to, p, const_cast<Ref *>(&r), {}));
}

/******************************************************************************/

inline stat::request request_to(Output &v, Index i, void *p, Qualifier q) noexcept {
    if (!p) return stat::request::null;
    return stat::request(i.call(tag::request_to_value, &v, p, {}));
}

inline stat::request request_to(Ref &r, Index i, void *p, Qualifier q) noexcept {
    if (!p) return stat::request::null;
    return stat::request(i.call(tag::request_to_ref, &r, p, {}));
}

/**************************************************************************************/

template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
std::remove_reference_t<T> * request(Index i, void *p, Scope &s, Type<T>, Qualifier q);

template <class T, std::enable_if_t<!std::is_reference_v<T>, int> = 0>
std::optional<T> request(Index i, void *p, Scope &s, Type<T>, Qualifier q);

/**************************************************************************************/

bool call_to(Value &v, Index i, void const *p, Caller &&c, ArgView args) noexcept;

bool call_to(Ref &v, Index i, void const *p, Caller &&c, ArgView args) noexcept;

template <class ...Args>
Value call_value(Index i, void const *p, Caller &&c, Args &&...args);

template <class ...Args>
Ref call_ref(Index i, void const *p, Caller &&c, Args &&...args);

/**************************************************************************************/

}