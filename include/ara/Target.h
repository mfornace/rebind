#pragma once
#include "Index.h"
#include "Common.h"
#include "Type.h"

namespace ara {

/******************************************************************************/

struct Target {
    // Kinds of return values that can be requested
    enum Tag : ara_tag {
        None,
        Mutable,             // Request &
        Reference,           // Request & or const &
        Const,               // Request const &
        Stack,               // Request stack storage
        Heap,                // Request heap allocated value
        TrivialRelocate, // Request stack storage as long as type is trivially_relocatable
        NoThrowMove//,      // Request stack storage as long as type is noexcept movable
        // Trivial              // Request stack storage as long as type is trivial
    };

    // Requested type index. May be null if no type is requested
    Index idx;
    // Output storage address. Must satisfy at least void* alignment and size requirements.
    void* out;
    // void* allocator_data;
    // void* (*allocator)(Code size, Code alignment) noexcept
    // Output storage capacity in bytes (sizeof)
    Code const len;
    // Requested qualifier (roughly T, T &, or T const &)
    Tag const tag;

    explicit constexpr operator bool() const noexcept {return out;}

    template <class T>
    void set_index() noexcept {idx = Index::of<T>();}

    // Return placement new pointer if it is available for type T
    template <class T>
    constexpr void* placement() const noexcept {
        if (tag == Stack       && is_stackable<T>(len)) return out;
        // if (tag == Trivial     && is_stackable<T>(len) && std::is_trivially_copyable_v<T>) return out;
        if (tag == NoThrowMove     && is_stackable<T>(len) && std::is_nothrow_move_constructible_v<T>) return out;
        if (tag == TrivialRelocate && is_stackable<T>(len) && is_trivially_relocatable_v<T>) return out;
        return nullptr;
    }

    bool wants_value() const {return 3 < tag;}

    template <class T>
    bool accepts() const noexcept {return !idx || idx == Index::of<T>();}

    template <class T, class ...Ts>
    void emplace(Ts &&...ts) {
        if (auto p = placement<T>()) parts::alloc_to<T>(p, static_cast<Ts &&>(ts)...);
        else out = parts::alloc<T>(static_cast<Ts &&>(ts)...);
        idx = Index::of<T>();
    }

    template <class T, class ...Ts>
    T * emplace_if(Ts &&...ts) {
        if (accepts<T>()) emplace<T>(static_cast<Ts &&>(ts)...);
        return static_cast<T *>(out);
    }

    auto name() const {return idx.name();}

    template <class T>
    bool set_if(T &&t) {return emplace_if<unqualified<T>>(std::forward<T>(t));}

    // Set pointer to a heap allocation
    template <class T>
    void set_reference(T &t) noexcept {out = std::addressof(t); set_index<T>();}

    template <class T>
    void set_reference(T const &t) noexcept {out = const_cast<T *>(std::addressof(t)); set_index<T>();}

    void set_current_exception() noexcept;

    template <class F>
    auto make_noexcept(F &&f) noexcept {
        using O = decltype(f());
        try {return f();}
        catch (std::bad_alloc const &) {return O::OutOfMemory;}
        catch (...) {set_current_exception(); return O::Exception;}
    }

    [[noreturn]] void rethrow_exception();
};

/******************************************************************************/

}