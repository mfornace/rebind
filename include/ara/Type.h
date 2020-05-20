#pragma once

#include <type_traits>
#include <utility>
#include <typeindex>
#include <string_view>
#include <iostream>
#include <functional>

namespace ara {

template <class T>
using unqualified = std::remove_cv_t<std::remove_reference_t<T>>;

/******************************************************************************************/

enum class Qualifier {C, L, R};

// template <class T, Qualifier Q>
// using qualified = std::conditional_t<Q == Read, T const &,
//     std::conditional_t<Q == Lvalue, T &, T &&>>;

template <class T>
static constexpr Qualifier qualifier_of =
    std::is_rvalue_reference_v<T> ? Qualifier::R : (std::is_const_v<std::remove_reference_t<T>> ? Qualifier::C : Qualifier::L);

// inline constexpr bool compatible_qualifier(Qualifier from, Qualifier to) {
//     // from = const: {const yes, rvalue no, lvalue no}
//     // from = rvalue: {const yes, rvalue yes, lvalue no}
//     // from = lvalue: {const yes, rvalue no, lvalue yes}
//     return to == Read || from == to;
// }

/******************************************************************************************/

/// Compile-time type a la boost::hana
template <class T>
struct Type  {
    using type = T;
    T operator*() const; // undefined
    constexpr Type<unqualified<T>> operator+() const {return {};}
};

template <class T, class U>
constexpr std::is_same<T, U> is_same(Type<T>, Type<U>) {return {};}

/******************************************************************************************/

template <class T>
struct is_type_t : std::false_type {};

template <class T>
struct is_type_t<Type<T>> : std::true_type {};

template <class T>
static constexpr bool is_type = is_type_t<unqualified<T>>::value;

/******************************************************************************************/

template <class T>
static constexpr Type<T> ctype = {};

template <class T>
struct IndexedType {
    std::size_t index;
    T operator*() const; // undefined
    constexpr Type<unqualified<T>> operator+() const {return {};}
};

/******************************************************************************************/

std::string demangle(char const *);

/******************************************************************************************/

template <class T, class SFINAE=void>
struct TypeName {
    static std::string const name;
};

template <class T, class SFINAE>
std::string const TypeName<T, SFINAE>::name = demangle(typeid(T).name());

template <class T>
auto const & type_name() noexcept {return TypeName<T>::name;}

/******************************************************************************/

}
