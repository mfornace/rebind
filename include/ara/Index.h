#pragma once
#include "Raw.h"
#include "Common.h"

#include <string_view>

namespace ara {

/******************************************************************************************/

template <class T, class SFINAE=void>
struct Switch;

template <class T>
struct Lookup {
    static constexpr Idx invoke = Switch<Alias<T>>::invoke;
};

template <class T>
Idx fetch(Type<T>) noexcept {return Lookup<T>::invoke;}

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

    std::uintptr_t integer() const noexcept {return bit_cast<std::uintptr_t>(base);}

    template <class T>
    static Index of() noexcept {return {fetch(Type<T>())};}

    template <class T>
    constexpr bool equals() const {return *this == of<T>();}

    std::string_view name() const noexcept;
};

#warning "I think I added a static call here"

/******************************************************************************************/

template <class Mode>
struct Tagged {
    Idx base = nullptr;

    constexpr Tagged() = default;
    Tagged(Idx i, Mode t) : base(ara_mode_index(i, static_cast<ara_mode>(t))) {}

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

    auto tag() const noexcept {return static_cast<Mode>(ara_get_mode(base));}
    explicit operator Index() const noexcept {return ara_get_index(base);}

    // template <class T>
    // static Tagged of() noexcept {
    //     using U = unqualified<T>;
    //     static_assert(std::is_same_v<T, U> || std::is_same_v<T, U const&> || std::is_same_v<T, U&>);
    //     return Tagged(fetch<unqualified<T>>(),
    //         std::is_same_v<T, U> ? Stack :
    //             (std::is_const_v<std::remove_reference_t<T>> ? Read : Write));
    // }
};

// template <class Mode>
// Tagged(Idx i, Mode t) ->

/******************************************************************************************/

template <class T, class N>
constexpr bool is_stackable(N size) {
    return std::is_destructible_v<T> && (sizeof(T) <= sizeof(void*) || sizeof(T) <= size) && (alignof(T) <= alignof(void*));
}

template <class T>
static constexpr bool is_always_stackable = is_stackable<T>(sizeof(void*));

/******************************************************************************************/

struct Pointer {
    void *base;

    static constexpr Pointer from(void *b=nullptr) {return {b};}

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
struct hash<ara::Index> {
    size_t operator()(ara::Index i) const {return hash<ara::Idx>()(i.base);}
};

template <class T>
struct hash<ara::Tagged<T>> {
    size_t operator()(ara::Tagged<T> i) const {return hash<ara::Idx>()(i.base);}
};

}

/******************************************************************************************/