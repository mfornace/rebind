#pragma once
#include "API.h"
#include "Type.h"
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
namespace rebind {

class Ref;
class Value;
class Output;
class Scope;
class Caller;
class ArgView;

template <class T>
static constexpr bool is_usable = true
    && !std::is_reference_v<T>
    && !std::is_const_v<T>
    && !std::is_volatile_v<T>
    && !std::is_void_v<T>
    && std::is_nothrow_destructible_v<T>
    && !std::is_same_v<T, Ref>
    && !std::is_same_v<T, Value>
    && !is_type_t<T>::value;
    // && !std::is_null_pointer_v<T>
    // && !std::is_function_v<T>

/******************************************************************************/

template <class T>
constexpr void assert_usable() {
    static_assert(!std::is_reference_v<T>);
    static_assert(!std::is_const_v<T>);
    static_assert(!std::is_volatile_v<T>);
    static_assert(!std::is_void_v<T>);
    static_assert(std::is_nothrow_destructible_v<T>);

    static_assert(!std::is_same_v<T, Ref>);
    static_assert(!std::is_same_v<T, Value>);
    static_assert(!is_type_t<T>::value);

    // static_assert(!std::is_null_pointer_v<T>);
}

/**************************************************************************************/

namespace raw {

using Ptr = rebind_ptr;
static constexpr std::uintptr_t const qualifier_bits = 3; // i.e. binary 11

/**************************************************************************************/

// Tag the pointer's last 2 bits with the qualifier
inline Ptr make_ptr(void *v, Qualifier q) {
    return reinterpret_cast<Ptr>(reinterpret_cast<std::uintptr_t>(v) | static_cast<std::uintptr_t>(q));
}

// Get out the last 2 bits as the qualifier
inline Qualifier qualifier(Ptr p) noexcept {
    return static_cast<Qualifier>(reinterpret_cast<std::uintptr_t>(p) & qualifier_bits);
}

// Get out the pointer minus the qualifier
inline void * address(Ptr p) noexcept {
    return reinterpret_cast<void *>(reinterpret_cast<std::uintptr_t>(p) & ~qualifier_bits);
}

/**************************************************************************************/

template <class T>
bool matches(Index ind, Type<T> t={}) noexcept {
    static_assert(is_usable<T>);
    return ind == fetch<unqualified<T>>();
}

template <class T>
T *target(Index i, void *p) noexcept {
    return (p && i == fetch<unqualified<T>>()) ? static_cast<T *>(p) : nullptr;
}

inline std::string_view name(Index i) noexcept {
    if (i) {
        std::string_view out;
        if (i(tag::name, &out, nullptr, {})) return out;
    }
    return "null";
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
        stat::copy err{i(tag::copy, &v, const_cast<void *>(p), {})};
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
    return stat::assign_if(i(tag::assign_to, p, const_cast<Ref *>(&r), {}));
}

/******************************************************************************/

inline stat::request request_to(Output &v, Index i, void *p, Qualifier q) noexcept {
    if (!p) return stat::request::null;
    return stat::request(i(tag::request_to_value, &v, p, {}));
}

inline stat::request request_to(Ref &r, Index i, void *p, Qualifier q) noexcept {
    if (!p) return stat::request::null;
    return stat::request(i(tag::request_to_ref, &r, p, {}));
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

}