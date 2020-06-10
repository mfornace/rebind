#pragma once
#include "Index.h"
#include "Common.h"
#include "Type.h"
#include <cstdlib>

namespace ara {

/******************************************************************************/

struct Allocation {
    void* ptr = nullptr;

    template <class T>
    Allocation(Type<T>) {
        using namespace std;
        ptr = aligned_alloc(alignof(T), sizeof(T));
    }

    template <class T>
    T* release() noexcept {return static_cast<T*>(std::exchange(ptr, nullptr));}

    ~Allocation() noexcept {if (ptr) std::free(ptr);}
};

/******************************************************************************/

template <class T>
struct Allocator {

    template <class ...Args>
    static T* stack(void *p, Args &&...args) {
        assert_implementable<T>();
        if constexpr(!std::is_same_v<T, Alias<T>>) {
            new(p) Alias<T>(T(std::forward<Args>(args)...));
        } else if constexpr(std::is_constructible_v<T, Args &&...>) {
            new(p) T(std::forward<Args>(args)...);
        } else {
            new(p) T{std::forward<Args>(args)...};
        }
        return static_cast<T*>(p);
    }

    template <class F, class ...Args>
    static T* invoke_stack(void *p, F &&f, Args &&...args) {
        assert_implementable<T>();
        new(p) Alias<T>(std::invoke(std::forward<F>(f), std::forward<Args>(args)...));
        return static_cast<T*>(p);
    }

    /**************************************************************************/

    template <class F, class ...Args>
    static T* invoke_heap(F &&f, Args &&...args) {
        Allocation alloc{Type<T>()};
        invoke_stack(alloc.ptr, std::forward<F>(f), std::forward<Args>(args)...);
        return alloc.release<T>();
    }

    template <class ...Args>
    static T* heap(Args &&...args) {
        Allocation alloc{Type<T>()};
        stack(alloc.ptr, std::forward<Args>(args)...);
        return alloc.release<T>();
    }

    static void deallocate(T* t) noexcept {std::free(static_cast<void*>(t));}
};

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
    using Constraint = ara_code;

    constexpr Target(Index i, void* out, Code len, Constraint mode) : c{i, out, 0, len, mode} {}
    Target(Target &&) noexcept = delete;
    Target(Target const &) = delete;
    ~Target() noexcept {}

    /**************************************************************************/


    // Kinds of output values (used as a mask, exclusive with each other)
    static constexpr Constraint
        None        = 0,
        Existing    = 1 << 0, // Output is written to rather than created
        Write       = 1 << 1, // Mutable reference
        Read        = 1 << 2, // Immutable reference
        Heap        = 1 << 3, // Heap allocation
        Trivial     = 1 << 4, // Trivial type
        Relocatable = 1 << 5, // Relocatable but not trivial type
        MoveNoThrow = 1 << 6, // Noexcept move constructible
        MoveThrow   = 1 << 7, // Non-noexcept move constructible
        Unmovable   = 1 << 8; // Not move constructible

    template <class T>
    static constexpr Constraint constraint = {
        std::is_trivial_v<T> ? Trivial :
            is_trivially_relocatable_v<T> ? Relocatable :
                std::is_nothrow_move_constructible_v<T> ? MoveNoThrow :
                    std::is_move_constructible_v<T> ? MoveThrow : Unmovable};

    /**************************************************************************/

    Index index() const {return c.index;}
    Code length() const {return c.length;}
    void* output() const {return c.output;}
    auto name() const {return index().name();}
    Mode returned_mode() const {return static_cast<Mode>(c.mode);}

    /**************************************************************************/

    // Return whether a type is accepted
    template <class T>
    constexpr bool accepts(Type<T> t={}) const noexcept {
        static_assert(!std::is_rvalue_reference_v<T>);
        if (!matches<unqualified<T>>()) return false;
        if constexpr(std::is_reference_v<T>) {
            return can_yield(std::is_const_v<std::remove_reference_t<T>> ? Read : Write);
        } else {
            return can_yield(constraint<T> | Heap | Existing);
        }
    }

    // If there is currently held (writable) output of the specified type, return its address
    template <class T>
    [[nodiscard]] T* get(Type<T> t={}) const {
        return can_yield(Existing) && index() == Index::of<T>() ? static_cast<T*>(c.output) : nullptr;
    }

    // Return current output, default constructing it if it is not present
    template <class T>
    [[nodiscard]] T* set_default(Type<T> t={}) {
        static_assert(std::is_default_constructible_v<T>);
        if (auto p = get(t)) return p;
        else if (accepts<T>()) return emplace<T>();
        else return nullptr;
    }

    template <class T, class ...Ts>
    [[nodiscard]] T* emplace(Ts &&...ts) {
        if (!accepts<T>()) return nullptr;
        construct<T>(std::forward<Ts>(ts)...);
        return static_cast<T*>(output());
    }

    // Try to insert a type T which defaults to unqualified<Source>
    // If the output of that type already exists, assign: `static_cast<T &>(dest) = source`
    // If the output does not already exist, construct it like `T(source)`
    // Return whether successful
    template <class T=void, class Source>
    [[nodiscard]] bool assign(Source &&s) {
        using Out = std::conditional_t<std::is_void_v<T>, unqualified<Source>, T>;
        if (!accepts<Out>()) return false;
        if constexpr(std::is_reference_v<Out>) {
            set_reference(s);
            return true;
        } else {
            if (can_yield(Existing)) {
                *static_cast<Out*>(output()) = std::forward<Source>(s);
                return true;
            }
            if (can_yield(constraint<T> | Heap)) {
                construct<Out>(std::forward<Source>(s));
                return true;
            }
            return false;
        }
    }

    /**************************************************************************/

    // Return whether a storage class is accepted
    constexpr bool can_yield(Constraint in) const noexcept {return c.mode & in;}

    // Return whether the unqualified type is allowed by the target
    template <class T>
    bool matches(Type<T> t={}) const noexcept {
        static_assert(std::is_same_v<T, unqualified<T>>);
        return !index() || index() == Index::of<T>();
    }

    template <class T, class ...Ts>
    bool construct(Ts &&...ts) {
        if (auto p = placement<T>()) {
            Allocator<T>::stack(p, static_cast<Ts &&>(ts)...);
            c.mode = static_cast<ara_mode>(Mode::Stack);
            set_index<T>();
            return true;
        } else {
            c.output = Allocator<T>::heap(static_cast<Ts &&>(ts)...);
            c.mode = static_cast<ara_mode>(Mode::Heap);
            set_index<T>();
            return false;
        }
    }

    // Return placement new pointer if it is available for type T
    template <class T>
    constexpr void* placement() const noexcept {
        return can_yield(constraint<T>) && is_stackable<T>(length()) ? output() : nullptr;
    }

    // Set pointer to a heap allocation
    template <class T>
    void set_reference(T &t) noexcept {
        c.output = std::addressof(t);
        c.mode = static_cast<ara_mode>(Mode::Write);
        set_index<T>();
    }

    template <class T>
    void set_reference(T const &t) noexcept {
        c.output = const_cast<T*>(std::addressof(t));
        c.mode = static_cast<ara_mode>(Mode::Read);
        set_index<T>();
    }

    template <class T>
    void set_heap(T* t) noexcept {
        c.output = t;
        c.mode = static_cast<ara_mode>(Mode::Heap);
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