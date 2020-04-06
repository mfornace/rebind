#pragma once
#include "API.h"
#include <string_view>

namespace rebind {

/******************************************************************************************/

struct Index {
    rebind_index fptr = nullptr;

    constexpr operator rebind_index() const {return fptr;}
    constexpr bool has_value() const {return fptr;}
    explicit constexpr operator bool() const {return has_value();}

    constexpr bool operator< (Index i) const {return fptr <  i.fptr;}
    constexpr bool operator> (Index i) const {return fptr >  i.fptr;}
    constexpr bool operator<=(Index i) const {return fptr <= i.fptr;}
    constexpr bool operator>=(Index i) const {return fptr >= i.fptr;}
    constexpr bool operator==(Index i) const {return fptr == i.fptr;}
    constexpr bool operator!=(Index i) const {return fptr != i.fptr;}

    template <class T>
    static Index of() noexcept;

    template <class T>
    constexpr bool equals() const {return *this == of<T>();}

    template <class T>
    T call(Tag t, std::uint32_t i, void *o, Fptr f={}, void* s={}, rebind_args args={}) const {
        return static_cast<T>(fptr(t, i, o, f, s, args));
    }

    inline std::string_view name() const noexcept {
        if (!has_value()) return "null";
        rebind_str out;
        if (stat::name::ok != call<stat::name>(tag::name, {}, &out)) return "unavailable";
        return std::string_view(out.pointer, out.len);
    }
};

/******************************************************************************************/

}

/******************************************************************************************/

namespace std {

template <>
struct hash<rebind::Index> {
    size_t operator()(rebind::Index i) const {return hash<rebind_index>()(i.fptr);}
};

}