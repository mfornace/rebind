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

template <class T, class SFINAE=void>
struct Impl;

#define ARA_DETECT_TRAIT(NAME, SIGNATURE) \
    template <class T, class SFINAE=void> \
    struct NAME : std::false_type {}; \
    \
    template <class T> \
    struct NAME<T, std::void_t<SIGNATURE>> : std::true_type {}; \
    \
    template <class T> \
    static constexpr bool NAME##_v = NAME<T>::value;

/******************************************************************************************/

// Dont need target here, just:
// void* output
// length
// Pointer source
// bool relocate / assign

struct Copy {
    enum stat : Stat {Stack, Heap, Impossible, Exception, OutOfMemory};

    template <class T>
    struct Put {
        static stat put(void *out, T const&self, std::uintptr_t length) noexcept {
            // return out.make_noexcept([&] {
                if constexpr(std::is_copy_constructible_v<T>) {
                    if (is_stackable<T>(length)) {
                        Allocator<T>::stack(out, self);
                        return Stack;
                    } else {
                        *static_cast<void **>(out) = Allocator<T>::heap(self);
                        return Heap;
                    }
                } else return Impossible;
            // }
        }
    };

    static stat call(Idx f, void* out, Pointer source, std::uintptr_t length) noexcept {
        return static_cast<stat>(f({code::copy, {}}, out, source.base, reinterpret_cast<void*>(length)));
    }
};

/******************************************************************************/

// Relocate an object (construct T in new location and destruct the old one)
// -- If t was on the Heap, it could have been trivially relocated; thus we can assume it is on the Stack
// -- We also assume that the type is nothrow move constructible currently... this can change.

struct Relocate {
    enum stat : Stat {OK, Impossible};

    template <class T>
    struct Put {
        static stat put(void* out, T&& self) noexcept {
            if constexpr(std::is_nothrow_move_constructible_v<T> && std::is_destructible_v<T>) {
                Allocator<T>::stack(out, std::move(self));
                self.~T();
                return OK;
            } else return Impossible;
        }
    };

    static stat call(Idx f, void* out, void* source) noexcept {
        return static_cast<stat>(f({code::relocate, {}}, out, source, {}));
    }
};

/******************************************************************************/

struct Assign {
    enum stat : Stat {OK, Impossible, Exception, OutOfMemory};

    template <class T>
    struct Put {
        static stat put(T&out, Pointer self, Mode mode) noexcept {
            return Impossible;
        }
    };

    static stat call(Idx f, Pointer out, Pointer source, Mode mode) noexcept {
        return static_cast<stat>(f({code::assign, static_cast<Code>(mode)}, out.base, source.base, {}));
    }
};

/******************************************************************************/

struct Swap {
    enum stat : Stat {OK, Impossible, Exception, OutOfMemory};

    template <class T>
    struct Put {
        static stat put(T& first, T& second) noexcept {
            using std::swap;
            if constexpr(std::is_nothrow_swappable_v<T>) {
                swap(first, second);
                return OK;
            } else return Impossible;
        }
    };

    static stat call(Idx f, Pointer first, Pointer second) noexcept {
        return static_cast<stat>(f({code::swap, {}}, first.base, second.base, {}));
    }
};

/******************************************************************************/

/// Delete the held object, on Stack or on Heap.
// -- Always natively noexcept. If Impossible is returned, likely a programmer error.
struct Destruct {
    enum stat : Stat {OK, Impossible};

    template <class T>
    struct Put {
        static stat put(T& t) noexcept {
            if constexpr(std::is_destructible_v<T>) {t.~T(); return OK;}
            else return Impossible;
        }
    };

    static stat call(Idx f, Pointer t) noexcept {
        return static_cast<stat>(f({code::destruct, {}}, t.base, {}, {}));
    }

    template <class T>
    struct RAII {
        T&held;
        ~RAII() noexcept {Put<T>::put(held);}
    };
};

/******************************************************************************/

struct Deallocate {
    enum stat : Stat {OK, Impossible};

    template <class T>
    struct Put {
        static stat put(T& t) noexcept {
            if constexpr(std::is_destructible_v<T>) {
                t.~T();
                Allocator<T>::deallocate(std::addressof(t));
                return OK;
            } else return Impossible;
        }
    };

    static stat call(Idx f, Pointer t) noexcept {
        return static_cast<stat>(f({code::deallocate, {}}, t.base, {}, {}));
    }

    template <class T>
    struct RAII {
        T&held;
        ~RAII() noexcept {Put<T>::put(held);}
    };
};

/******************************************************************************/

// Return a handle to std::type_info or some language-specific equivalent
// -- Should be done in a noexcept way and should always succeed
// -- Should always succeed and return a ara_str (basically like std::string_view)
struct Info {
    enum stat : Stat {OK};

    template <class T>
    struct Put {
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

// Return a handle to std::type_info or some language-specific equivalent
// -- Should be done in a noexcept way and should always succeed
// -- Should always succeed and return a ara_str (basically like std::string_view)
struct Name {
    enum stat : Stat {OK};

    template <class T>
    struct Put {
        static stat put(ara_str& s) noexcept {
            s.data = TypeName<T>::name.data();
            s.size = TypeName<T>::name.size();
            return OK;
        }
    };

    static stat call(Idx f, ara_str &s) noexcept {
        return static_cast<stat>(f({code::name, {}}, &s, {}, {}));
    }
};

/******************************************************************************/

ARA_DETECT_TRAIT(has_element, decltype(Impl<T>::index(std::declval<Target&>(), std::declval<T const&>(), std::intptr_t())));

struct Element {
    enum stat : Stat {OK, Impossible};

    template <class T>
    struct Put {
        static stat put(Target& out, Pointer self, std::intptr_t i, Mode qualifier) noexcept {
            return Impossible;
        }
    };

    static stat call(Idx f, Target& out, Pointer self, std::intptr_t i, Mode qualifier) noexcept {
        return static_cast<stat>(f({code::element, static_cast<Code>(qualifier)},
            &out, self.base, reinterpret_cast<void*>(i)));
    }
};

/******************************************************************************/

ARA_DETECT_TRAIT(has_attribute, decltype(Impl<T>::index(std::declval<Target&>(), std::declval<T const&>(), std::string_view())));

struct Attribute {
    enum stat : Stat {OK, Missing, Impossible};

    template <class T>
    struct Put {
        static stat put(Target &out, Pointer self, ara_str const &name, Mode qualifier) noexcept {
            return Impossible;
        }
    };

    static stat call(Idx f, Target &out, Pointer self, ara_str name, Mode qualifier) noexcept {
        return static_cast<stat>(f({code::attribute, static_cast<Code>(qualifier)}, &out, self.base, &name));
    }
};

/******************************************************************************/

// bool operator()(Target &target, T source)
// exception -> Exception
// bad_alloc -> OutOfMemory
// false -> None
// true ->


ARA_DETECT_TRAIT(has_dump, decltype(Impl<T>::dump(std::declval<Target&>(), std::declval<T&&>())));

// Dump the held object to a less constrained type that has been requested
// -- Exceptions should be very rare. Prefer to return None.
struct Dump {
    enum stat : Stat {None, Write, Read, Stack, Heap, Exception, OutOfMemory};

    template <class T>
    struct Put {
        static stat put(Target &out, Pointer source, Mode qualifier) noexcept {
            if constexpr(has_dump_v<T>) {
                return out.make_noexcept([&] {
                    switch (qualifier) {
                        case Mode::Stack: {if (!Impl<T>::dump(out, source.load<T&&>())) return None; break;}
                        case Mode::Heap:  {if (!Impl<T>::dump(out, source.load<T&&>())) return None; break;}
                        case Mode::Write: {if (!Impl<T>::dump(out, source.load<T&>())) return None; break;}
                        case Mode::Read:  {if (!Impl<T>::dump(out, source.load<T const&>())) return None; break;}
                    }
                    switch (out.returned_mode()) {
                        case Mode::Write: return Write;
                        case Mode::Read: return Read;
                        case Mode::Stack: return Stack;
                        case Mode::Heap: return Heap;
                        default: return None;
                    }
                });
            } else return None;
        }
    };

    static stat call(Idx f, Target &out, Pointer source, Mode qualifier) noexcept {
        return static_cast<stat>(f({code::dump, static_cast<Code>(qualifier)}, &out, source.base, {}));
    }
};

/******************************************************************************/

ARA_DETECT_TRAIT(has_load, decltype(Impl<T>::load(std::declval<Ref&>())));

// Load T from a less constrained type
// -- Prefer to return None
// -- More likely to encounter exceptions when preconditions are not met
// -- Note that Dump is not called by Load
// Seems like could be combined with Assign... just make Target.out non-null on input.
struct Load {
    enum stat : Stat {None, Write, Read, Stack, Heap, Exception, OutOfMemory};

    template <class T>
    struct Put {
        // currently not planned that out can be a reference, I think
        static stat put(Target &out, Pointer source, Mode qualifier) noexcept {
            if constexpr(has_load_v<T>) {
                return out.make_noexcept([&] {
                    ara_ref r{ara_mode_index(out.index(), static_cast<ara_mode>(qualifier)), source.base};
                    if (std::optional<T> o = Impl<T>::load(reinterpret_cast<Ref &>(r))) {
                        out.emplace<T>(std::move(*o)); // <-- here, change if output is already present
                        switch (out.returned_mode()) {
                            case Mode::Write: return Write;
                            case Mode::Read: return Read;
                            case Mode::Stack: return Stack;
                            case Mode::Heap: return Heap;
                            default: return None;
                        }
                    } else return None;
                });
            } else return None;
        }
    };

    static stat call(Idx f, Target &out, Pointer source, Mode qualifier) noexcept {
        return static_cast<stat>(f({code::load, static_cast<Code>(qualifier)}, &out, source.base, {}));
    }
};

/******************************************************************************/

struct DeclAny {
    template <class T>
    operator T() const;
};
ARA_DETECT_TRAIT(has_call, decltype(Impl<T>::call(DeclAny(), std::declval<T&&>())));
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
                      None, Stack, Heap, Write, Read,
                      Exception, OutOfMemory};

    static constexpr bool was_invoked(stat s) {return 3 < s;}

    template <class T>
    struct Put {
        static stat put(Target &out, Pointer self, ArgView &args, Mode qualifier) noexcept {
            stat s = Impossible;
            if constexpr(has_call_v<T>) {
                switch (qualifier) {
                    case Mode::Stack: {Impl<T>::call({out, args, s}, self.load<T&&>()); break;}
                    case Mode::Heap:  {Impl<T>::call({out, args, s}, self.load<T&&>()); break;}
                    case Mode::Write: {Impl<T>::call({out, args, s}, self.load<T&>()); break;}
                    case Mode::Read:  {Impl<T>::call({out, args, s}, self.load<T const&>()); break;}
                }
            }
            return s;
        }
    };

    static stat call(Idx f, Target &out, Pointer self, Mode qualifier, ArgView &args) noexcept {
        return static_cast<stat>(f({code::call, static_cast<Code>(qualifier)}, &out, self.base, reinterpret_cast<ara_args *>(&args)));
    }

    [[nodiscard]] static stat wrong_number(Target &, Code, Code) noexcept;
    static stat wrong_type(Target &, Code, Index, Qualifier) noexcept;
    [[nodiscard]] static stat wrong_return(Target &, Index, Qualifier) noexcept;
};

/******************************************************************************************/

template <class Op, class T, class ...Ts>
Stat impl_put(Ts &&...ts) {
    typename Op::stat out = Op::template Put<T>::put(static_cast<Ts &&>(ts)...);
    DUMP("Put output", static_cast<Stat>(out));
    return static_cast<Stat>(out);
}

/******************************************************************************************/

template <class T, std::enable_if_t<is_complete_v<Impl<T>>, int> = 0>
void warn_unimplemented() {}

template <class T, std::enable_if_t<!is_complete_v<Impl<T>>, int> = 0>
// [[deprecated]]
void warn_unimplemented() {}

/******************************************************************************************/

template <class T, class SFINAE>
struct Switch {
    static_assert(!std::is_void_v<T>);
    static_assert(!std::is_const_v<T>);
    static_assert(!std::is_reference_v<T>);
    static_assert(!std::is_volatile_v<T>);
    static_assert(!std::is_same_v<T, Ref>);

    static Stat call(ara_input i, void* o, void* s, void* args) noexcept {
        static_assert(sizeof(T) >= 0, "Type should be complete");
        warn_unimplemented<T>();

        if (i.code != code::name) DUMP("Put<", type_name<T>(), ">: ", code_name(i.code));
        if (i.code >= 14) return -1; // ?

        switch (static_cast<code::codes>(i.code)) {
            case code::destruct:
                return impl_put<Destruct, T>(Pointer::from(o).load<T&>());

            case code::deallocate:
                return impl_put<Deallocate, T>(Pointer::from(o).load<T&>());

            case code::copy:
                return impl_put<Copy, T>(o, Pointer::from(s).load<T const&>(), i.tag);

            case code::relocate:
                return impl_put<Relocate, T>(o, Pointer::from(s).load<T&&>());

            case code::swap:
                return impl_put<Swap, T>(Pointer::from(o).load<T&>(), Pointer::from(s).load<T&>());

            case code::assign:
                return impl_put<Assign, T>(Pointer::from(o).load<T&>(), Pointer::from(s), static_cast<Mode>(i.tag));

            case code::element:
                return impl_put<Element, T>(*static_cast<Target*>(o), Pointer::from(s), reinterpret_cast<std::uintptr_t>(args), static_cast<Mode>(i.tag));

            case code::attribute:
                return impl_put<Attribute, T>(*static_cast<Target*>(o), Pointer::from(s), *static_cast<ara_str*>(args), static_cast<Mode>(i.tag));

            case code::load:
                return impl_put<Load, T>(*static_cast<Target *>(o), Pointer::from(s), static_cast<Mode>(i.tag));

            case code::dump:
                return impl_put<Dump, T>(*static_cast<Target *>(o), Pointer::from(s), static_cast<Mode>(i.tag));

            case code::call:
                return impl_put<Call, T>(*static_cast<Target *>(o), Pointer::from(s), *reinterpret_cast<ArgView *>(args), static_cast<Mode>(i.tag));

            case code::name:
                return impl_put<Name, T>(*static_cast<ara_str *>(o));

            case code::info:
                return impl_put<Info, T>(*static_cast<Idx *>(o), *static_cast<void const **>(s));

            // repr?
            // traits?
            case code::check: {
                return i.code < 14;
            }
        }
    }
};

/******************************************************************************************/

template <class SFINAE>
struct Switch<void, SFINAE> {
    static Stat call(ara_input i, void* o, void* s, void*) noexcept {
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
    return std::string_view(out.data, out.size);
}

/******************************************************************************************/

}
