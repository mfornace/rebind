#pragma once
#include "Index.h"
#include "Common.h"
#include "Type.h"

namespace ara {

/******************************************************************************/

struct Lifetime {
    std::uint64_t value = 0;

    constexpr Lifetime() noexcept = default;

    constexpr Lifetime(std::initializer_list<std::uint_fast8_t> const &v) noexcept {
        for (auto const i : v) value &= 1 << i;
    }

    constexpr Lifetime(std::uint_fast8_t i) noexcept : value(1 << i) {}
};

/******************************************************************************/

// T2 is an aggregate type or a union type which holds one of the aforementioned
// types as an element or non-static member [...]: this makes
// it safe to cast from the first member of a struct and from an element of a
// union to the struct/union that contains it.
struct Target {
    ara_target c;

    // Kinds of return values that can be requested
    enum Tag : ara_tag {
        None,                // Request no output
        Mutable,             // Request &
        Const,               // Request const &
        Reference,           // Request & or const &
        Stack,               // Request stack storage
        Heap,                // Request heap allocated value
        TrivialRelocate,     // Request stack storage as long as type is trivially_relocatable
        NoThrowMove//,       // Request stack storage as long as type is noexcept movable
        // Trivial           // Request stack storage as long as type is trivial
    };

    Tag tag() const {return static_cast<Tag>(c.tag);}
    Index index() const {return c.index;}
    Code length() const {return c.length;}
    void* output() const {return c.output;}

    template <class T>
    bool accepts() const noexcept {return !index() || index() == Index::of<T>();}

    template <class T, class ...Ts>
    [[nodiscard]] T * emplace_if(Ts &&...ts) {
        if (accepts<T>()) emplace<T>(static_cast<Ts &&>(ts)...);
        return static_cast<T *>(output());
    }

    template <class T>
    [[nodiscard]] bool set_if(T &&t) {return emplace_if<unqualified<T>>(std::forward<T>(t));}

    bool wants_value() const {return 3 < tag();}

    // Return placement new pointer if it is available for type T
    template <class T>
    constexpr void* placement() const noexcept {
        if (tag() == Stack       && is_stackable<T>(length())) return output();
        // if (tag() == Trivial     && is_stackable<T>(length()) && std::is_trivially_copyable_v<T>) return output();
        if (tag() == NoThrowMove     && is_stackable<T>(length()) && std::is_nothrow_move_constructible_v<T>) return output();
        if (tag() == TrivialRelocate && is_stackable<T>(length()) && is_trivially_relocatable_v<T>) return output();
        return nullptr;
    }

    template <class T, class ...Ts>
    void emplace(Ts &&...ts) {
        if (auto p = placement<T>()) parts::alloc_to<T>(p, static_cast<Ts &&>(ts)...);
        else c.output = parts::alloc<T>(static_cast<Ts &&>(ts)...);
        set_index<T>();
    }

    auto name() const {return index().name();}

    template <class T>
    void set_index() noexcept {c.index = Index::of<T>();}

    void set_lifetime(Lifetime l) noexcept {c.lifetime = l.value;}

    // Set pointer to a heap allocation
    template <class T>
    void set_reference(T &t) noexcept {c.output = std::addressof(t); set_index<T>();}

    template <class T>
    void set_reference(T const &t) noexcept {c.output = const_cast<T *>(std::addressof(t)); set_index<T>();}

    void set_current_exception() noexcept;

    template <class F>
    auto make_noexcept(F &&f) noexcept {
        using Enum = decltype(f());
        try {return f();}
        catch (std::bad_alloc const &) {return Enum::OutOfMemory;}
        catch (...) {set_current_exception(); return Enum::Exception;}
    }

    [[noreturn]] void rethrow_exception();

    static Target from(Index i, void* out, Code len, Tag tag) {
        return {ara_target{i, out, 0, len, tag}};
    }
};

static_assert(std::is_aggregate_v<Target>);

/******************************************************************************/

}