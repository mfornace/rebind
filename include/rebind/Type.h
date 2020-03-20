#pragma once
#include <type_traits>
#include <utility>
#include <typeindex>
#include <string_view>
#include <iostream>
#include <functional>

namespace rebind {

template <class T>
using unqualified = std::remove_cv_t<std::remove_reference_t<T>>;

/******************************************************************************************/

enum Qualifier : unsigned char {Const, Lvalue, Rvalue};

static std::string_view QualifierNames[3] = {"const", "lvalue", "rvalue"};
static std::string_view QualifierSuffixes[3] = {" const &", " &", " &&"};

inline std::ostream & operator<<(std::ostream &os, Qualifier q) {
    return os << QualifierNames[static_cast<unsigned char>(q)];
}

template <class T, Qualifier Q>
using qualified = std::conditional_t<Q == Const, T const &,
    std::conditional_t<Q == Lvalue, T &, T &&>>;

template <class T>
static constexpr Qualifier qualifier_of =
    std::is_rvalue_reference_v<T> ? Rvalue : (std::is_const_v<std::remove_reference_t<T>> ? Const : Lvalue);

inline constexpr bool compatible_qualifier(Qualifier from, Qualifier to) {
    // from = const: {const yes, rvalue no, lvalue no}
    // from = rvalue: {const yes, rvalue yes, lvalue no}
    // from = lvalue: {const yes, rvalue no, lvalue yes}
    return to == Const || from == to;
}

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

using Demangler = std::function<std::string(char const *)>;

std::string demangle(char const *);

void set_demangler(Demangler fun) noexcept;

/******************************************************************************************/

class Index {
    std::type_info const *p = nullptr;

public:

    constexpr Index() noexcept = default;
    constexpr Index(std::type_info const &t) noexcept : p(&t) {}

    template <class T>
    Index(Type<T>) noexcept : p(&typeid(T)) {}

    /**************************************************************************************/

    std::type_info const & info() const noexcept {return p ? *p : typeid(void);}

    std::string name() const noexcept {return demangle(info().name());}

    /// For now, hash code does not incorporate the qualifier
    std::size_t hash_code() const noexcept {return info().hash_code();}

    /// Return if the index is not empty
    constexpr explicit operator bool() const noexcept {return p;}

    /// Test if this type equals another one, but ignoring all qualifiers
    template <class T>
    bool matches(Type<T> t={}) const noexcept {return p && typeid(T) == *p;}

    /// Test if this type equals another one, but ignoring all qualifiers
    bool matches(Index const &t) const noexcept {return p == t.p;}

    /// Test if this type equals a type, including qualifiers
    template <class T>
    bool equals(Type<T> t={}) const noexcept {
        return std::is_same_v<T, unqualified<T>> && p && typeid(T) == *p;
    }

    /**************************************************************************************/

    constexpr bool operator==(Index const &t) const {return p == t.p;}
    constexpr bool operator!=(Index const &t) const {return p != t.p;}
    constexpr bool operator<(Index const &t) const {return p < t.p;}
    constexpr bool operator>(Index const &t) const {return p > t.p;}
    constexpr bool operator<=(Index const &t) const {return p <= t.p;}
    constexpr bool operator>=(Index const &t) const {return p >= t.p;}
};

inline std::ostream & operator<<(std::ostream &os, Index t) {return os << t.name();}

template <class T>
constexpr Index type_index(Type<T> t={}) {return t;}

/******************************************************************************************/

}

namespace std {

template <>
struct hash<rebind::Index> {
    size_t operator()(rebind::Index const &t) const {return t.hash_code();}
};

}
