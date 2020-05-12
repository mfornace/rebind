#pragma once
#include "Index.h"
#include "Common.h"
#include "Type.h"

namespace ara {

/******************************************************************************/

template <class T, class ...Args>
T * allocate(Args &&...args) {
    assert_usable<T>();
    if constexpr(std::is_constructible_v<T, Args &&...>) {
        return new T(static_cast<Args &&>(args)...);
    } else {
        return new T{static_cast<Args &&>(args)...};
    }
}

template <class T, class ...Args>
T * allocate_in_place(void *p, Args &&...args) {
    assert_usable<T>();
    if constexpr(std::is_constructible_v<T, Args &&...>) {
        return new(p) T(static_cast<Args &&>(args)...);
    } else {
        return new(p) T{static_cast<Args &&>(args)...};
    }
}

/******************************************************************************/

struct Lifetime {
    std::uint64_t value = 0;

    constexpr Lifetime() noexcept = default;

    constexpr Lifetime(std::initializer_list<std::uint_fast8_t> const &v) noexcept {
        for (auto const i : v) value |= 1 << i;
    }

    static constexpr Lifetime from_mask(std::uint_fast8_t i) noexcept {
        Lifetime out;
        out.value = i;
        return out;
    }
};

/******************************************************************************/

// T2 is an aggregate type or a union type which holds one of the aforementioned
// types as an element or non-static member [...]: this makes
// it safe to cast from the first member of a struct and from an element of a
// union to the struct/union that contains it.
union Target {
    ara_target c;

    constexpr Target(Index i, void* out, Code len, ara_tag tag) : c{i, out, 0, len, tag} {}
    Target(Target &&) noexcept = delete;
    Target(Target const &) = delete;
    ~Target() noexcept {}

    /**************************************************************************/

    // Kinds of values (used as a mask, exclusive with each other)
    static constexpr ara_tag
        None        = 0,
        Mutable     = 1 << 0, // Mutable reference
        Const       = 1 << 1, // Immutable reference
        Heap        = 1 << 2, // Heap allocation
        Trivial     = 1 << 3, // Trivial type
        Relocatable = 1 << 4, // Relocatable but not trivial type
        MoveNoThrow = 1 << 5, // Noexcept move constructible
        MoveThrow   = 1 << 6, // Non-noexcept move constructible
        Unmovable   = 1 << 7; // Not move constructible

    template <class T>
    static constexpr ara_tag constraint = {
        std::is_trivial_v<T> ? Trivial :
            is_trivially_relocatable_v<T> ? Relocatable :
                std::is_nothrow_move_constructible_v<T> ? MoveNoThrow :
                    std::is_move_constructible_v<T> ? MoveThrow : Unmovable};

    /**************************************************************************/

    Tag returned_tag() const {return static_cast<Tag>(c.tag);}
    Index index() const {return c.index;}
    Code length() const {return c.length;}
    void* output() const {return c.output;}
    auto name() const {return index().name();}

    /**************************************************************************/

    template <class T>
    bool accepts() const noexcept {return !index() || index() == Index::of<T>();}

    template <class T, class ...Ts>
    [[nodiscard]] T* emplace_if(Ts &&...ts) {
        if (accepts<T>()) emplace<T>(static_cast<Ts &&>(ts)...);
        return static_cast<T*>(output());
    }

    template <class T>
    [[nodiscard]] bool set_if(T &&t) {return emplace_if<unqualified<T>>(std::forward<T>(t));}

    // Return placement new pointer if it is available for type T
    template <class T>
    constexpr void* placement() const noexcept {
        return ((c.tag & constraint<T>) && is_stackable<T>(length())) ? output() : nullptr;
    }

    template <class T, class ...Ts>
    void emplace(Ts &&...ts) {
        if (auto p = placement<T>()) {
            allocate_in_place<T>(p, static_cast<Ts &&>(ts)...);
            c.tag = static_cast<ara_tag>(Tag::Stack);
        } else {
            c.output = allocate<T>(static_cast<Ts &&>(ts)...);
            c.tag = static_cast<ara_tag>(Tag::Heap);
        }
        set_index<T>();
    }

    /**************************************************************************/

    // Set pointer to a heap allocation
    template <class T>
    void set_reference(T &t) noexcept {
        c.output = std::addressof(t);
        c.tag = static_cast<ara_tag>(Tag::Mutable);
        set_index<T>();
    }

    template <class T>
    void set_reference(T const &t) noexcept {
        c.output = const_cast<T *>(std::addressof(t));
        c.tag = static_cast<ara_tag>(Tag::Const);
        set_index<T>();
    }

    template <class T>
    void set_heap(T *t) noexcept {
        c.output = t;
        c.tag = static_cast<ara_tag>(Tag::Heap);
        set_index<T>();
    }

    template <class T>
    void set_index() noexcept {c.index = Index::of<T>();}

    void set_lifetime(Lifetime l) noexcept {c.lifetime = l.value;}
    Lifetime lifetime() const noexcept {return Lifetime::from_mask(c.lifetime);}

    void set_current_exception() noexcept;

    template <class F>
    auto make_noexcept(F &&f) noexcept {
        using Enum = decltype(f());
        try {return f();}
        catch (std::bad_alloc const &) {return Enum::OutOfMemory;}
        catch (...) {set_current_exception(); return Enum::Exception;}
    }

    [[noreturn]] void rethrow_exception();
};

static_assert(!std::is_copy_constructible_v<Target>);
static_assert(!std::is_move_constructible_v<Target>);
static_assert(!std::is_move_assignable_v<Target>);
static_assert(!std::is_copy_assignable_v<Target>);

/******************************************************************************/

}