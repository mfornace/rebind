#pragma once
#include "API.h"

#include <string_view>

namespace rebind {

/******************************************************************************************/

template <class T, class SFINAE=void>
struct impl;

template <class T>
Idx fetch() noexcept {return &impl<T>::call;}

/******************************************************************************************/

struct Index {
    Idx base;
    constexpr Index(Idx f=nullptr) noexcept : base(f) {}

    constexpr operator Idx() const {return base;}
    constexpr bool has_value() const {return base;}
    explicit constexpr operator bool() const {return has_value();}
    void reset() noexcept {base = nullptr;}

    constexpr bool operator< (Index i) const {return base <  i.base;}
    constexpr bool operator> (Index i) const {return base >  i.base;}
    constexpr bool operator<=(Index i) const {return base <= i.base;}
    constexpr bool operator>=(Index i) const {return base >= i.base;}
    constexpr bool operator==(Index i) const {return base == i.base;}
    constexpr bool operator!=(Index i) const {return base != i.base;}
    constexpr Idx operator+() const {return base;}

    template <class T>
    static Index of() noexcept {return {fetch<T>()};}

    template <class T>
    constexpr bool equals() const {return *this == of<T>();}

    std::string_view name() const noexcept;
};

/******************************************************************************************/

struct TagIndex {
    Idx base = nullptr;

    constexpr TagIndex() = default;
    TagIndex(Idx i, rebind_tag t) : base(rebind_tag_index(i, t)) {}

    // constexpr operator Idx() const {return base;}
    constexpr bool has_value() const {return base;}
    explicit constexpr operator bool() const {return has_value();}
    void reset() noexcept {base = nullptr;}

    constexpr bool operator< (Index i) const {return base <  i.base;}
    constexpr bool operator> (Index i) const {return base >  i.base;}
    constexpr bool operator<=(Index i) const {return base <= i.base;}
    constexpr bool operator>=(Index i) const {return base >= i.base;}
    constexpr bool operator==(Index i) const {return base == i.base;}
    constexpr bool operator!=(Index i) const {return base != i.base;}
    constexpr Idx operator+() const {return base;}

    auto tag() const noexcept {return rebind_get_tag(base);}
    explicit operator Index() const noexcept {return rebind_get_index(base);}

    // template <class T>
    // static TagIndex of() noexcept {
    //     using U = unqualified<T>;
    //     static_assert(std::is_same_v<T, U> || std::is_same_v<T, U const&> || std::is_same_v<T, U&>);
    //     return TagIndex(fetch<unqualified<T>>(),
    //         std::is_same_v<T, U> ? Stack :
    //             (std::is_const_v<std::remove_reference_t<T>> ? Const : Mutable));
    // }
};

/******************************************************************************************/

template <class T>
constexpr bool is_stackable(Len size) {
    return (sizeof(T) <= sizeof(void*) || sizeof(T) <= size)
        && (alignof(T) <= alignof(void*)) && std::is_move_constructible_v<T>;
}

template <class T>
static constexpr bool is_always_stackable = is_stackable<T>(sizeof(void*));

/******************************************************************************************/

struct Pointer {
    void *base;

    constexpr Pointer(void *b=nullptr) : base(b) {}

    template <class T>
    constexpr T load() const {
        static_assert(std::is_reference_v<T>);
        // if constexpr(qualifier_of<T> == Qualifier::R || qualifier_of<T> == Qualifier::C) {

        // }
        return static_cast<T>(*static_cast<std::remove_reference_t<T> *>(base));
    }

    template <class T>
    constexpr T *address() const {
        return static_cast<T *>(base);
    }
};

/******************************************************************************************/

}

/******************************************************************************************/

namespace std {

template <>
struct hash<rebind::Index> {
    size_t operator()(rebind::Index i) const {return hash<rebind::Idx>()(i.base);}
};

template <>
struct hash<rebind::TagIndex> {
    size_t operator()(rebind::TagIndex i) const {return hash<rebind::Idx>()(i.base);}
};

}

/******************************************************************************************/