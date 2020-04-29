#pragma once
#include <cstdint>
#include <iostream>
#include <sstream>

#define DUMP(...) ::ara::print_arguments(__FILE__, __LINE__, __VA_ARGS__);

namespace ara {

/******************************************************************************/

extern bool Debug;
void set_debug(bool debug) noexcept;
bool debug() noexcept;

char const * unknown_exception_description() noexcept;

/// For debugging purposes
template <class ...Ts>
void print_arguments(char const *s, int n, Ts const &...ts) {
    if (!Debug) return;
    std::cout << '[' << s << ':' << n << "] ";
    int x[] = {(std::cout << ts << ' ', 0)...};
    std::cout << std::endl;
}

/******************************************************************************/

/// Copy CV and reference qualifier from one type to another
template <class From, class To> struct copy_qualifier_t {using type = To;};
template <class From, class To> struct copy_qualifier_t<From &, To> {using type = To &;};
template <class From, class To> struct copy_qualifier_t<From const &, To> {using type = To const &;};
template <class From, class To> struct copy_qualifier_t<From &&, To> {using type = To &&;};

template <class From, class To> using copy_qualifier = typename copy_qualifier_t<From, To>::type;

/******************************************************************************/

template <class T, class SFINAE=void>
struct is_trivially_relocatable : std::is_trivially_copyable<T> {};

template <class T>
static constexpr bool is_trivially_relocatable_v = is_trivially_relocatable<T>::value;

template <class T>
static constexpr bool is_stack_type = false;

/******************************************************************************/

template <class T, class=void>
struct is_complete : std::false_type {};

template <class T>
struct is_complete<T, std::enable_if_t<(sizeof(T) >= 0)>> : std::true_type {};

template <class T>
static constexpr bool is_complete_v = is_complete<T>::value;

/******************************************************************************/

template <class T, class SFINAE=void>
struct is_copy_constructible : std::is_copy_constructible<T> {};

template <class T, class Alloc, class SFINAE>
struct is_copy_constructible<std::vector<T, Alloc>, SFINAE> : std::is_copy_constructible<T> {};

template <class T>
static constexpr bool is_copy_constructible_v = is_copy_constructible<T>::value;

/******************************************************************************/

// Ignore variable to use a parameter when the argument should be ignored
struct Ignore {
    template <class ...Ts>
    constexpr Ignore(Ts const &...ts) {}
};

/******************************************************************************/

template <class T>
using SizeOf = std::integral_constant<std::size_t, sizeof(T)>;

/******************************************************************************/

// Update for std::bit_cast in the future;
template <class To, class From>
To bit_cast(From const &from) noexcept {
    static_assert(std::is_trivially_copyable_v<From>);
    static_assert(std::is_trivial_v<To>);
    static_assert(sizeof(To) == sizeof(From));
    To to;
    std::memcpy(&to, &from, sizeof(To));
    return to;
}
// template <class T>
// constexpr std::uintptr_t aligned_location(std::uintptr_t i) {
//     static_assert((alignof(T) & (alignof(T) - 1)) == 0, "not power of 2");
//     auto rem = static_cast<std::uintptr_t>(alignof(T) - 1) & i;
//     return rem ? i + alignof(T) - rem: i;
// }

// template <class T>
// constexpr T* aligned_pointer(void const *p) {
//     return reinterpret_cast<T *>(aligned_location<T>(reinterpret_cast<std::uintptr_t>(p)));
// }


// template <class T>
// constexpr T* aligned_void(void *p) {
//     return (alignof(T) <= alignof(void *)) ? static_cast<T*>(p) : aligned_pointer<T>(p);
// }

/******************************************************************************/

template <class T>
using storage_like = std::aligned_storage_t<sizeof(T), alignof(T)>;

template <class T, class S>
T & storage_cast(S &storage) {
    return *std::launder(reinterpret_cast<T *>(&storage));
}

/******************************************************************************/

}