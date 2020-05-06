#pragma once
// #include "Call.h"
#include "Target.h"
// #include "API.h"
// #include "Type.h"
// #include "Common.h"
// #include "Parts.h"
// #include "Index.h"

// #include <map>
// #include <stdexcept>
// #include <array>
// #include <type_traits>

namespace ara {

/******************************************************************************************/

template <class T>
struct Alias : std::false_type {};

template <class T>
struct Alias<T &> : std::true_type {};

template <class T>
struct Alias<T const &> : std::true_type {};

template <class T>
struct Alias<T &&> : std::true_type {};

template <class T>
static constexpr bool is_alias = Alias<T>::value;

/******************************************************************************************/

// Insert a new copy of the object into a buffer of size n
// -- If the buffer is too small, allocate the object on the Heap
// -- Out of memory errors are handled specifically
// -- Other exceptions are currently not output and are just returned as the Exception code
struct Copy {
    enum stat : Stat {Stack, Heap, Impossible, Exception, OutOfMemory};

    template <class T>
    struct impl {
        static stat put(void* out, Code n, T const &t) noexcept {
            if constexpr(is_copy_constructible_v<T>) {
                try {
                    if (is_stackable<T>(n)) {
                        new(out) T(t);
                        return Stack;
                    } else {
                        *static_cast<void**>(out) = new T(t);
                        return Heap;
                    }
                } catch (std::bad_alloc) {
                    return OutOfMemory;
                } catch (...) {
                    return Exception;
                }
            } else {
                return Impossible;
            }
        }
    };

    static stat call(Idx f, void* out, Code n, Pointer source) noexcept {
        return static_cast<stat>(f({code::copy, n}, out, source.base, {}));
    }
};

/******************************************************************************************/

// Relocate an object (construct T in new location and destruct the old one)
// -- If t was on the Heap, it could have been trivially relocated; thus we can assume it is on the Stack
// -- We also assume that the type is nothrow move constructible currently
struct Relocate {
    enum stat : Stat {OK, Impossible};

    template <class T>
    struct impl {
        static stat put(void* out, T&& t) noexcept {
            if constexpr(std::is_nothrow_move_constructible_v<T> && std::is_destructible_v<T>) {
                new(out) T(std::move(t));
                t.~T();
                return OK;
            } else {
                return Impossible;
            }
        }
    };

    static stat call(Idx f, void* out, void*t) noexcept {
        return static_cast<stat>(f({code::relocate, {}}, out, t, {}));
    }
};

/******************************************************************************/

/// Delete the held object, on Stack or on Heap.
// -- Always natively noexcept. If Impossible is returned, likely a programmer error.
struct Destruct {
    enum storage : Code {Stack, Heap};
    enum stat : Stat {OK, Impossible};

    template <class T>
    struct impl {
        static stat put(T& t, storage s) noexcept {
            if constexpr(std::is_destructible_v<T>) {
                if (s == Heap) delete std::addressof(t);
                else t.~T();
                return OK;
            } else {
                return Impossible;
            }
        }
    };

    static stat call(Idx f, Pointer t, storage s) noexcept {
        return static_cast<stat>(f({code::destruct, s}, t.base, {}, {}));
    }
};

/******************************************************************************/

template <class T, bool Heap>
struct DestructGuard {
    T &held;
    ~DestructGuard() noexcept {Destruct::impl<T>::put(held, Heap ? Destruct::Heap : Destruct::Stack);}
};

/******************************************************************************/

// Return a handle to std::type_info or some language-specific equivalent
// -- Should be done in a noexcept way and should always succeed
struct Info {
    enum stat : Stat {OK};

    template <class T>
    struct impl {
        static stat put(Idx& out, void const*& t) noexcept {
            t = &typeid(T);
            out = fetch(Type<std::type_info>());
            return OK;
        }
    };

    static stat call(Idx f, Idx& out, void const*& t) noexcept {
        return static_cast<stat>(f({code::info, {}}, &out, &t, {}));
    }
};

/******************************************************************************/

// Return a reasonable *type* name
// -- Should always succeed and return a ara_str (basically like std::string_view)
struct Name {
    enum stat : Stat {OK};

    template <class T>
    struct impl {
        static stat put(ara_str &s) noexcept {
            s.pointer = TypeName<T>::name.data();
            s.len = TypeName<T>::name.size();
            return OK;
        }
    };

    static stat call(Idx f, ara_str &s) noexcept {
        return static_cast<stat>(f({code::name, {}}, &s, {}, {}));
    }
};

/******************************************************************************/

template <class T, class SFINAE=void>
struct Dumpable;
// bool operator()(Target &target, T source)
// exception -> Exception
// bad_alloc -> OutOfMemory
// false -> None
// true ->

// Dump the held object to a less constrained type that has been requested
// -- Exceptions should be very rare. Prefer to return None.
struct Dump {
    enum stat : Stat {None, Mutable, Const, Stack, Heap, Exception, OutOfMemory};

    template <class T>
    struct impl {
        static stat put(Target &out, Pointer source, Tag qualifier) noexcept {
            if constexpr(is_complete_v<Dumpable<T>>) {
                return out.make_noexcept([&] {
                    switch (qualifier) {
                        case Tag::Stack:   {if (!Dumpable<T>()(out, source.load<T &&>())) return None; break;}
                        case Tag::Heap:    {if (!Dumpable<T>()(out, source.load<T &&>())) return None; break;}
                        case Tag::Mutable: {if (!Dumpable<T>()(out, source.load<T &>())) return None; break;}
                        case Tag::Const:   {if (!Dumpable<T>()(out, source.load<T const &>())) return None; break;}
                    }
                    switch (out.tag()) {
                        case Target::Mutable: return Mutable;
                        case Target::Const: return Const;
                        case Target::Stack: return Stack;
                        case Target::Heap: return Heap;
                        default: return None;
                    }
                });
            } else return None;
        }
    };

    static stat call(Idx f, Target &out, Pointer source, Tag qualifier) noexcept {
        return static_cast<stat>(f({code::dump, static_cast<Code>(qualifier)}, &out, source.base, {}));
    }
};

/******************************************************************************/

template <class T, class SFINAE=void>
struct Loadable;

// Load T from a less constrained type
// -- Prefer to return None
// -- More likely to encounter exceptions when preconditions are not met
struct Load {
    enum stat : Stat {None, Mutable, Const, Stack, Heap, Exception, OutOfMemory};

    template <class T>
    struct impl {
        // currently not planned that out can be a reference, I think
        static stat put(Target &out, Pointer source, Tag qualifier) noexcept {
            if constexpr(is_complete_v<Loadable<T>>) {
                return out.make_noexcept([&] {
                    ara_ref r{ara_tag_index(out.index(), static_cast<ara_tag>(qualifier)), source.base};
                    if (std::optional<T> o = Loadable<T>()(reinterpret_cast<Ref &>(r))) {
                        out.emplace<T>(std::move(*o));
                        switch (out.tag()) {
                            case Target::Mutable: return Mutable;
                            case Target::Const: return Const;
                            case Target::Stack: return Stack;
                            case Target::Heap: return Heap;
                            default: return None;
                        }
                    } else return None;
                });
            } else return None;
        }
    };

    static stat call(Idx f, Target &out, Pointer source, Tag qualifier) noexcept {
        return static_cast<stat>(f({code::load, static_cast<Code>(qualifier)}, &out, source.base, {}));
    }
};

/******************************************************************************/

template <class T, class SFINAE=void>
struct Callable;
// stat operator()(Target *out, T, ArgView &args)

// Call an object
// -- if Target is null, do not return output (change later for Exception actually...)
// -- on success, return one of first 5 qualifiers
// -- Impossible: calling is not implemented
// -- WrongNumber: wrong number of arguments
// -- WrongReturn: wrong type or qualifier of return
// -- WrongType: wrong type of argument
// -- Exception: Exception while executing the function
struct Call {
    enum stat : Stat {Impossible, WrongNumber, WrongType, WrongReturn,
                      None, Stack, Heap, Mutable, Const,
                      Exception, OutOfMemory};
    // enum source :

    static constexpr bool was_invoked(stat s) {return 3 < s;}
#warning "todo: static method"
    template <class T>
    struct impl {
        static stat put(Target &out, Pointer self, ArgView &args, Tag qualifier) noexcept {
            stat s = Impossible;
            if constexpr(is_complete_v<Callable<T>>) {
                switch (qualifier) {
                    case Tag::Stack:   {Callable<T>()({out, args, s}, self.load<T &&>()); break;}
                    case Tag::Heap:    {Callable<T>()({out, args, s}, self.load<T &&>()); break;}
                    case Tag::Mutable: {Callable<T>()({out, args, s}, self.load<T &>()); break;}
                    case Tag::Const:   {Callable<T>()({out, args, s}, self.load<T const &>()); break;}
                }
            }
            return s;
        }
    };

    static stat call(Idx f, Target &out, Pointer self, Tag qualifier, ArgView &args) noexcept {
        return static_cast<stat>(f({code::call, static_cast<Code>(qualifier)}, &out, self.base, reinterpret_cast<ara_args *>(&args)));
    }

    [[nodiscard]] static stat wrong_number(Target &, Code, Code) noexcept;
    static stat wrong_type(Target &, Code, Index, Qualifier) noexcept;
    [[nodiscard]] static stat wrong_return(Target &, Index, Qualifier) noexcept;
};

/******************************************************************************************/

template <class Op, class T, class ...Ts>
Stat impl_put(Ts &&...ts) {
    typename Op::stat out = Op::template impl<T>::put(static_cast<Ts &&>(ts)...);
    DUMP("Impl output", static_cast<Stat>(out));
    return static_cast<Stat>(out);
}

/******************************************************************************************/

template <class T, std::enable_if_t<is_complete_v<Callable<T>> || is_complete_v<Loadable<T>> || is_complete_v<Dumpable<T>> || is_complete_v<Callable<T>>, int> = 0>
void warn_unimplemented() {}

template <class T, std::enable_if_t<!(is_complete_v<Callable<T>> || is_complete_v<Loadable<T>> || is_complete_v<Dumpable<T>> || is_complete_v<Callable<T>>), int> = 0>
// [[deprecated]]
void warn_unimplemented() {}

/******************************************************************************************/

template <class T, class SFINAE>
struct impl {
    static_assert(!std::is_void_v<T>);
    static_assert(!std::is_const_v<T>);
    static_assert(!std::is_reference_v<T>);
    static_assert(!std::is_volatile_v<T>);
    static_assert(!std::is_same_v<T, Ref>);

    static Stat call(ara_input i, void* o, void* s, ara_args *args) noexcept __attribute__((noinline)) {
        return build(i, o, s, args);
    }

    static Stat build(ara_input i, void* o, void* s, ara_args *args) noexcept {
        static_assert(sizeof(T) >= 0, "Type should be complete");
        warn_unimplemented<T>();

        if (i.code != code::name) DUMP("Impl<", type_name<T>(), ">: ", code_name(i.code));

        switch(i.code) {
            case code::destruct: {
                return impl_put<Destruct, T>(Pointer::from(o).load<T &>(), static_cast<Destruct::storage>(i.tag));
            }
            case code::relocate: {
                return impl_put<Relocate, T>(o, Pointer::from(s).load<T &&>());
            }
            case code::copy: {
                return impl_put<Copy, T>(o, i.tag, Pointer::from(s).load<T const &>());
            }
            case code::load: {
                return impl_put<Load, T>(*static_cast<Target *>(o), Pointer::from(s), static_cast<Tag>(i.tag));
            }
            case code::dump: {
                // return impl_put<Dump, T>(o, i.tag, reinterpret_cast<Tagged &>(f), Pointer::from(s));
                return impl_put<Dump, T>(*static_cast<Target *>(o), Pointer::from(s), static_cast<Tag>(i.tag));
            }
            case code::assign: {
                return {};
                // if constexpr(!std::is_move_assignable_v<T>) return stat::put(stat::assign_if::Impossible);
                // else return stat::put(default_assign(*static_cast<T *>(b), *static_cast<Ref const *>(o)));
            }
            case code::call: {
                return impl_put<Call, T>(*static_cast<Target *>(o), Pointer::from(s), reinterpret_cast<ArgView &>(*args), static_cast<Tag>(i.tag));
            }
            case code::name: {
                return impl_put<Name, T>(*static_cast<ara_str *>(o));
            }
            case code::info: {
                return impl_put<Info, T>(*static_cast<Idx *>(o), *static_cast<void const **>(s));
            }
            // repr?
            // traits?
            case code::check: {
                return i.code < 12;
            }
        }
        return -1; // ?
    }
};

/******************************************************************************************/

template <class SFINAE>
struct impl<void, SFINAE> {
    static Stat call(ara_input i, void* o, void* s, ara_args *args) noexcept __attribute__((noinline)) {
        return build(i, o, s, args);
    }

    static Stat build(ara_input i, void* o, void* s, ara_args *args) noexcept {
        switch (i.code) {
            case code::name: {
                return impl_put<Name, void>(*static_cast<ara_str *>(o));
            }
            case code::info: {
                return impl_put<Info, void>(*static_cast<Idx *>(o), *static_cast<void const **>(s));
            }
            case code::check: {
                return i.code == code::info || i.code == code::name;
            }
            default: {return -1;}
        }
    }
};

/******************************************************************************************/

inline std::string_view Index::name() const noexcept {
    if (!has_value()) return "null";
    ara_str out;
    Name::call(*this, out);
    return std::string_view(out.pointer, out.len);
}

/******************************************************************************************/

}
