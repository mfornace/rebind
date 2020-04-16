#pragma once
// #include "Call.h"
#include "API.h"
#include "Type.h"
#include "Common.h"
#include "Parts.h"
#include "Index.h"

#include <map>
#include <stdexcept>
#include <array>
#include <type_traits>

namespace rebind {

/******************************************************************************/

struct Target {
    // Kinds of return values that can be requested
    enum Tag : rebind_tag {
        Mutable,     // Request &
        Reference,   // Request & or const &
        Const,       // Request const &
        Stack,       // Request stack storage as long as type is movable
        Heap,        // Always request heap allocated value
        Relocatable, // Request stack storage as long as type is trivially_relocatable
        Movable,     // Request stack storage as long as type is noexcept movable
        Trivial      // Request stack storage as long as type is trivial
    };

    // Output storage address. Must satisfy at least void* alignment and size requirements.
    void *out;
    // Requested type index. May be null if no type is requested
    Index idx;
    // Output storage capacity in bytes (sizeof)
    Len len;
    // Requested qualifier (roughly T, T &, or T const &)
    Tag tag;

    explicit constexpr operator bool() const noexcept {return out;}

    template <class T>
    void set_index() noexcept {idx = Index::of<T>();}

    // Return placement new pointer if it is available for type T
    template <class T>
    constexpr void* placement() const noexcept {
        if (tag == Stack       && is_stackable<T>(len)) return out;
        if (tag == Trivial     && is_stackable<T>(len) && std::is_trivially_copyable_v<T>) return out;
        if (tag == Movable     && is_stackable<T>(len) && std::is_nothrow_move_constructible_v<T>) return out;
        if (tag == Relocatable && is_stackable<T>(len) && is_trivially_relocatable_v<T>) return out;
        return nullptr;
    }

    bool wants_value() const {return tag > 2;}

    template <class T>
    bool accepts() const noexcept {return !idx || idx == Index::of<T>();}

    template <class T, class ...Ts>
    T * emplace_if(Ts &&...ts) {
        if (accepts<T>()) {
            if (auto p = placement<T>()) parts::alloc_to<T>(p, static_cast<Ts &&>(ts)...);
            else out = parts::alloc<T>(static_cast<Ts &&>(ts)...);
            idx = Index::of<T>();
        }
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
};

/******************************************************************************/

/*
void drop      (void*        []            [])
bool copy      (void*,       void const *  [])
bool call      (void*,       void const *, ArgView, Caller &&)
bool method    (void*,       void const *, ArgView, Caller &&, name, tag)
bool name      (void*,       [],           [])
bool to_value  (Value &,      void*,       Qualifier)
bool to_ref    (Ref &,        void*,       Qualifier)
bool assign_if (void*,       [],           Ref const)
*/

/******************************************************************************************/

// Insert a new copy of the object into a buffer of size n
// If the buffer is too small, allocate the object on the heap
struct Copy {
    enum stat : Stat {stack, heap, impossible, exception, out_of_memory};

    template <class T>
    struct impl {
        static stat put(void*out, Len n, T const &t) noexcept {
            if constexpr(is_copy_constructible_v<T>) {
                try {
                    if (is_stackable<T>(n)) {
                        new(aligned_void<T>(out)) T(t);
                        return stack;
                    } else {
                        *static_cast<void**>(out) = new T(t);
                        return heap;
                    }
                } catch (std::bad_alloc) {
                    return out_of_memory;
                } catch (...) {
                    return exception;
                }
            } else {
                return impossible;
            }
        }
    };

    static stat call(Idx f, void* out, Len n, Pointer source) noexcept {
        return static_cast<stat>(f(input::copy, n, out, source.base, {}));
    }
};

/******************************************************************************************/

// Relocate an object (construct T in new location and destruct the old one)
// If t was on the heap, it could have been trivially relocated
// We can assume it was on the stack and that the type is nothrow move constructible
struct Relocate {
    enum stat : Stat {ok, impossible};

    template <class T>
    struct impl {
        static stat put(void* out, T&& t) noexcept {
            if constexpr(std::is_nothrow_move_constructible_v<T> && std::is_destructible_v<T>) {
                new(aligned_void<T>(out)) T(std::move(t));
                t.~T();
                return ok;
            } else {
                return impossible;
            }
        }
    };

    static stat call(Idx f, void* out, void*t) noexcept {
        return static_cast<stat>(f(input::relocate, {}, out, t, {}));
    }
};

/******************************************************************************/

/// Delete the held object, on stack or on heap
struct Destruct {
    enum storage : Len {stack, heap};
    enum stat : Stat {ok, impossible};

    template <class T>
    struct impl {
        static stat put(T& t, storage s) noexcept {
            if constexpr(std::is_destructible_v<T>) {
                if (s == heap) delete std::addressof(t);
                else t.~T();
                return ok;
            } else {
                return impossible;
            }
        }
    };

    static stat call(Idx f, Pointer t, storage s) noexcept {
        return static_cast<stat>(f(input::destruct, s, t.base, {}, {}));
    }
};

/******************************************************************************/

/// Return a handle to std::type_info or some language-specific equivalent
struct Info {
    enum stat : Stat {ok};

    template <class T>
    struct impl {
        static stat put(Idx& out, void const*& t) noexcept {
            t = &typeid(T);
            out = fetch<T>();
            return ok;
        }
    };

    static stat call(Idx f, Idx& out, void const*& t) noexcept {
        return static_cast<stat>(f(input::info, {}, &out, &t, {}));
    }
};

/******************************************************************************/

struct Name {
    enum stat : Stat {ok};

    template <class T>
    struct impl {
        static stat put(rebind_str &s) noexcept {
            s.pointer = TypeName<T>::name.data();
            s.len = TypeName<T>::name.size();
            return ok;
        }
    };

    static stat call(Idx f, rebind_str &s) noexcept {
        return static_cast<stat>(f(input::name, {}, &s, {}, {}));
    }
};

/******************************************************************************/

template <class T, class SFINAE=void>
struct Dumpable;

// Dump goes from T to a less restrictive type, so it shouldn't have much error reporting to do
struct Dump {
    enum stat : Stat {none, ok};

    template <class T>
    struct impl {
        static stat put(Target &out, Pointer source, Tag qualifier) noexcept {
            if constexpr(is_complete_v<Dumpable<T>>) {
                switch (qualifier) {
                    case Stack:   return static_cast<stat>(Dumpable<T>()(out, source.load<T &&>()));
                    case Heap:    return static_cast<stat>(Dumpable<T>()(out, source.load<T &&>()));
                    case Mutable: return static_cast<stat>(Dumpable<T>()(out, source.load<T &>()));
                    case Const:   return static_cast<stat>(Dumpable<T>()(out, source.load<T const &>()));
                }
            }
            return none;
        }
    };

    static stat call(Idx f, Target &out, Pointer source, Tag qualifier) noexcept {
        return static_cast<stat>(f(input::dump, qualifier, &out, source.base, {}));
    }
};

/******************************************************************************/

template <class T, class SFINAE=void>
struct Loadable;

// Dump goes from a less restrictive type to T, so it could have some errors when compatibility is not achieved
struct Load {
    enum stat : Stat {heap, stack, none};

    template <class T>
    struct impl {
        // std::optional<T> operator()(Ref, Scope &) ?
        // currently not planned that out can be a reference, I think
        static stat put(Target &out, Pointer source, Tag qualifier) noexcept {
            /*
            auto source_index = out.index();
            out.index = Index::of<std::string_view>();
            if (ok == dump(source_index, out, source, qualifier)) { ... }
            */
            return none;
        }
    };

    static stat call(Idx f, Target &out, Pointer source, Tag qualifier) noexcept {
        return static_cast<stat>(f(input::load, qualifier, &out, source.base, {}));
    }
};

/******************************************************************************/

template <class T, class SFINAE=void>
struct Callable;

struct Call {
    enum stat : Stat {none, in_place, heap,
                      impossible, wrong_number, invalid_argument,
                      wrong_type, wrong_qualifier, exception};

    template <class T>
    struct impl {
        static stat put(Target *out, Pointer self, ArgView &args, Tag qualifier) noexcept {
            if constexpr(is_complete_v<Callable<T>>) {
                switch (qualifier) {
                    case Stack:   return Callable<T>()(out, self.load<T &&>(), args);
                    case Heap:    return Callable<T>()(out, self.load<T &&>(), args);
                    case Mutable: return Callable<T>()(out, self.load<T &>(), args);
                    case Const:   return Callable<T>()(out, self.load<T const &>(), args);
                }
            }
            return impossible;
        }
    };

    static stat call(Idx f, Target *out, Pointer self, Tag qualifier, ArgView &args) noexcept {
        return static_cast<stat>(f(input::call, qualifier, out, self.base, reinterpret_cast<rebind_args *>(&args)));
    }
};

/******************************************************************************************/

template <class Op, class T, class ...Ts>
Stat impl_put(Ts &&...ts) {
    typename Op::stat out = Op::template impl<T>::put(static_cast<Ts &&>(ts)...);
    return static_cast<Stat>(out);
}

/******************************************************************************************/

template <class T, class SFINAE>
struct impl {
    static_assert(!std::is_void_v<T>);
    static_assert(!std::is_const_v<T>);
    static_assert(!std::is_reference_v<T>);
    static_assert(!std::is_volatile_v<T>);
    static_assert(!std::is_same_v<T, Ref>);

    static Stat call(Input t, Len n, void* o, void* s, rebind_args *args) noexcept __attribute__((noinline)) {
        if (t != input::name) DUMP("Impl::", input_name(t), ": ", type_name<T>());

        switch(t) {
            case input::destruct: {
                return impl_put<Destruct, T>(Pointer(o).load<T &>(), static_cast<Destruct::storage>(n));
            }
            case input::relocate: {
                return impl_put<Relocate, T>(o, Pointer(s).load<T &&>());
            }
            case input::copy: {
                return impl_put<Copy, T>(o, n, Pointer(s).load<T const &>());
            }
            case input::load: {
                return impl_put<Load, T>(*static_cast<Target *>(o), Pointer(s), static_cast<Tag>(n));
            }
            case input::dump: {
                // return impl_put<Dump, T>(o, n, reinterpret_cast<TagIndex &>(f), Pointer(s));
                return impl_put<Dump, T>(*static_cast<Target *>(o), Pointer(s), static_cast<Tag>(n));
            }
            case input::assign: {
                return {};
                // if constexpr(!std::is_move_assignable_v<T>) return stat::put(stat::assign_if::impossible);
                // else return stat::put(default_assign(*static_cast<T *>(b), *static_cast<Ref const *>(o)));
            }
            case input::call: {
                return impl_put<Call, T>(static_cast<Target *>(o), Pointer(s), reinterpret_cast<ArgView &>(*args), static_cast<Tag>(n));
            }
            // case input::emplace: {
            //     if constexpr(!Call<T>::value) return stat::put(stat::call::impossible);

            // }
            case input::name: {
                return impl_put<Name, T>(*static_cast<rebind_str *>(o));
            }
            case input::info: {
                return impl_put<Info, T>(*static_cast<Idx *>(o), *static_cast<void const **>(s));
            }
            case input::check: {
                return n < 12;
            }
        }
        return -1; // ?
    }
};

/******************************************************************************************/

template <class SFINAE>
struct impl<void, SFINAE> {
    static Stat call(Input t, Len n, void* o, void* s, rebind_args *args) {
        switch (t) {
            case input::name: {
                return impl_put<Name, void>(*static_cast<rebind_str *>(o));
            }
            case input::info: {
                return impl_put<Info, void>(*static_cast<Idx *>(o), *static_cast<void const **>(s));
            }
            case input::check: {
                return n == input::info || n == input::name;
            }
            default: {return -1;}
        }
    }
};

/******************************************************************************************/

inline std::string_view Index::name() const noexcept {
    if (!has_value()) return "null";
    rebind_str out;
    Name::call(*this, out);
    return std::string_view(out.pointer, out.len);
}

/******************************************************************************************/

}
