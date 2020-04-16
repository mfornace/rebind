#pragma once
#include "Parts.h"
#include "Impl.h"
// #include "Scope.h"

namespace rebind {

/******************************************************************************************/

struct Ref {
    TagIndex idx;
    Pointer ptr;

    constexpr Ref() noexcept = default;

    constexpr Ref(std::nullptr_t) noexcept : Ref() {}

    Ref(TagIndex i, void *p) noexcept : idx(i), ptr(p) {}

    template <class T>
    explicit Ref(T &t) noexcept : idx(Index::of<T>(), Mutable), ptr(std::addressof(t)) {}

    template <class T>
    explicit Ref(T const &t) noexcept : idx(Index::of<T>(), Const), ptr(std::addressof(const_cast<T &>(t))) {}

    /**************************************************************************************/

    Ref(Ref &&r) noexcept : idx(r.idx), ptr(r.ptr) {r.idx.reset();}

    Ref &operator=(Ref &&r) noexcept {
        idx = r.idx;
        ptr = r.ptr;
        r.idx.reset();
        return *this;
    }

    Ref(Ref const &) = delete;
    Ref &operator=(Ref const &) = delete;

    ~Ref() {
        if (!has_value()) return;
        switch (tag()) {
            case Stack: {Destruct::call(index(), ptr, Destruct::stack); return;}
            case Heap:  {Destruct::call(index(), ptr, Destruct::heap); return;}
            default: {return;}
        }
    }

    /**********************************************************************************/

    Tag tag() const noexcept {return static_cast<Tag>(idx.tag());}

    constexpr bool has_value() const noexcept {return idx.has_value();}

    explicit constexpr operator bool() const noexcept {return has_value();}

    Index index() const noexcept {return Index(idx);}

    std::string_view name() const noexcept {return index().name();}

    void reset() noexcept {this->~Ref(); idx.reset();}

    /**********************************************************************************/

    template <class T>
    bool binds_to(Qualifier q) const;

    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> *target() const {
        return binds_to<unqualified<T>>(qualifier_of<T>) ? ptr.address<std::remove_reference_t<T>>() : nullptr;
    }

    template <class T, class ...Ts>
    auto call(Caller c, Ts &&...ts) const {
        DUMP(type_name<T>());
        return parts::call<T>(idx, ptr, c, "", static_cast<Ts &&>(ts)...);
    }

    /**************************************************************************************/

    template <class T, std::enable_if_t<!std::is_reference_v<T>, int> = 0>
    std::optional<T> load(Scope &s, Type<T> t={});

    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> * load(Scope &s, Type<T> t={}) const;

    template <class T, std::enable_if_t<!std::is_reference_v<T>, int> = 0>
    T cast(Type<T> t={});

    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    T cast(Type<T> t={}) const;

    /**************************************************************************************/
};

/******************************************************************************************/

static_assert(std::is_standard_layout_v<Ref>);

template <>
struct is_trivially_relocatable<Ref> : std::true_type {};


template <class T>
bool Ref::binds_to(Qualifier q) const {
    if (!index().equals<T>()) return false;
    if (q == Qualifier::C) return true;
    switch (tag()) {
        case Const: return false;
        case Mutable: return q == Qualifier::L;
        default: return q == Qualifier::R;
    }
}

template <class T, std::enable_if_t<!std::is_reference_v<T>, int>>
std::optional<T> Ref::load(Scope &s, Type<T> t) {
    std::optional<T> out;
    if (index().equals<T>()) {
        switch (tag()) {
            case Stack:   {if constexpr(std::is_constructible_v<T, T&&>) {out.emplace(ptr.load<T &&>()); reset();} break;}
            case Heap:    {if constexpr(std::is_constructible_v<T, T&&>) {out.emplace(ptr.load<T &&>()); reset();} break;}
            case Const:   {if constexpr(std::is_constructible_v<T, T const &>) out.emplace(ptr.load<T const &>()); break;}
            case Mutable: {if constexpr(std::is_constructible_v<T, T &>) out.emplace(ptr.load<T &>()); break;}
        }
    }
    return out;
}

template <class T, std::enable_if_t<std::is_reference_v<T>, int>>
std::remove_reference_t<T> * Ref::load(Scope &s, Type<T> t) const {
    DUMP("load reference ", type_name<T>(), " ", name());
    if (auto t = target<T>()) return t;
    return nullptr;
}

/******************************************************************************************/

}
