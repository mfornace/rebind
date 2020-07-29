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

struct DeclAny {
    template <class T>
    operator T() const; // undefined
};

/******************************************************************************************/

struct Copy {
    enum stat : Stat {Assign, Stack, Heap, Impossible, Exception, OutOfMemory};

    template <class T>
    struct Default {
        static stat copy(Target& out, T const& self) noexcept {
            return out.make_noexcept([&] {
                if constexpr(is_copy_constructible_v<T> && std::is_copy_assignable_v<T>) {
                    if (out.can_yield(Target::Existing)) {
                        *static_cast<T*>(out.output()) = self;
                        return Assign;
                    }
                }
                if constexpr(is_copy_constructible_v<T>) {
                    if (out.can_yield(Target::constraint<T> | Target::Heap)) {
                        return out.construct<T>(self) ? Stack : Heap;
                    }
                }
                return Impossible;
            });
        }
    };

    static stat invoke(Idx f, Target& out, Pointer self) noexcept {
        return static_cast<stat>(f({code::copy, {}}, &out, self.base, {}));
    }
};

/******************************************************************************/

// Relocate an object (construct T in new location and destruct the old one)
// -- If t was on the Heap, it could have been trivially relocated; thus we can assume it is on the Stack
// -- We also assume that the type is nothrow move constructible currently... this might change.
// Need to add move assignment ...
struct Relocate {
    enum stat : Stat {OK, Impossible};

    template <class T>
    struct Default {
        static stat relocate(void* out, T&& self) noexcept {
            if constexpr(std::is_nothrow_move_constructible_v<T> && std::is_destructible_v<T>) {
                Allocator<T>::stack(out, std::move(self));
                self.~T();
                return OK;
            } else return Impossible;
        }
    };

    static stat invoke(Idx f, void* out, void* source) noexcept {
        return static_cast<stat>(f({code::relocate, {}}, out, source, {}));
    }
};

/******************************************************************************/

struct Swap {
    enum stat : Stat {OK, Impossible, Exception, OutOfMemory};

    template <class T>
    struct Default {
        static stat swap(T& first, T& second) noexcept {
            using std::swap;
            if constexpr(std::is_nothrow_swappable_v<T>) {
                swap(first, second);
                return OK;
            } else return Impossible;
        }
    };

    static stat invoke(Idx f, Pointer first, Pointer second) noexcept {
        return static_cast<stat>(f({code::swap, {}}, first.base, second.base, {}));
    }
};

/******************************************************************************/

/// Delete the held object, on Stack or on Heap.
// -- Always natively noexcept. If Impossible is returned, likely a programmer error.
struct Destruct {
    enum stat : Stat {OK, Impossible};

    template <class T>
    struct Default {
        static stat destruct(T& t) noexcept {
            if constexpr(std::is_destructible_v<T>) {t.~T(); return OK;}
            else return Impossible;
        }
    };

    static stat invoke(Idx f, Pointer t) noexcept {
        return static_cast<stat>(f({code::destruct, {}}, t.base, {}, {}));
    }

    template <class T>
    struct RAII {
        T& held;
        ~RAII() noexcept {Impl<T>::destruct(held);}
    };
};

/******************************************************************************/

struct Deallocate {
    enum stat : Stat {OK, Impossible};

    template <class T>
    struct Default {
        static stat deallocate(T& t) noexcept {
            if constexpr(std::is_destructible_v<T>) {
                t.~T();
                Allocator<T>::deallocate(std::addressof(t));
                return OK;
            } else return Impossible;
        }
    };

    static stat invoke(Idx f, Pointer t) noexcept {
        return static_cast<stat>(f({code::deallocate, {}}, t.base, {}, {}));
    }

    template <class T>
    struct RAII {
        T& held;
        ~RAII() noexcept {Impl<T>::destruct(held);}
    };
};

/******************************************************************************/

// Return a handle to std::type_info or some language-specific equivalent
// -- Should be done in a noexcept way and should always succeed
// -- Should always succeed and return a ara_str (basically like std::string_view)
struct Info {
    enum stat : Stat {OK};

    template <class T>
    struct Default {
        static stat info(Idx& out, void const*& t) noexcept {
            t = &typeid(T);
            out = fetch(Type<std::type_info>());
            return OK;
        }
    };

    static stat invoke(Idx f, Idx& out, void const*& t) noexcept {
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
    struct Default {
        static stat name(ara_str& s) noexcept {
            s.data = TypeName<T>::name.data();
            s.size = TypeName<T>::name.size();
            return OK;
        }
    };

    static stat invoke(Idx f, ara_str &s) noexcept {
        return static_cast<stat>(f({code::name, {}}, &s, {}, {}));
    }
};

/******************************************************************************/

ARA_DETECT_TRAIT(has_element, decltype(Impl<T>::element(std::declval<Target&>(), std::declval<T const&>(), std::intptr_t())));

struct Element {
    enum stat : Stat {None, Write, Read, Stack, Heap, Exception, OutOfMemory};

    template <class T>
    struct Default {
        static stat element_nothrow(Target& out, Pointer self, std::intptr_t i, Mode mode) noexcept {
            if constexpr(has_element_v<T>) {
                return out.make_noexcept([&] {
                    switch (mode) {
                        case Mode::Stack: {return Impl<T>::element(out, self.load<T&&>(), i);}
                        case Mode::Heap:  {return Impl<T>::element(out, self.load<T&&>(), i);}
                        case Mode::Write: {return Impl<T>::element(out, self.load<T&>(), i);}
                        case Mode::Read:  {return Impl<T>::element(out, self.load<T const&>(), i);}
                    }
                });
            } else return None;
        }
    };

    static stat invoke(Idx f, Target& out, Pointer self, std::intptr_t i, Mode mode) noexcept {
        return static_cast<stat>(f({code::element, static_cast<Code>(mode)},
            &out, self.base, reinterpret_cast<void*>(i)));
    }
};

/******************************************************************************/

ARA_DETECT_TRAIT(has_attribute, decltype(Impl<T>::attribute(std::declval<Target&>(), std::declval<T const&>(), std::string_view())));

struct Attribute {
    enum stat : Stat {None, Write, Read, Stack, Heap, Exception, OutOfMemory};

    template <class T>
    struct Default {
        static stat attribute_nothrow(Target& out, Pointer self, ara_str name, Mode mode) noexcept {
            if constexpr(has_attribute_v<T>) {
                std::string_view const s(name.data, name.size);
                return out.make_noexcept([&] {
                    switch (mode) {
                        case Mode::Stack: {if (!Impl<T>::attribute(out, self.load<T&&>(), s)) return None; break;}
                        case Mode::Heap:  {if (!Impl<T>::attribute(out, self.load<T&&>(), s)) return None; break;}
                        case Mode::Write: {if (!Impl<T>::attribute(out, self.load<T&>(), s)) return None; break;}
                        case Mode::Read:  {if (!Impl<T>::attribute(out, self.load<T const&>(), s)) return None; break;}
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

    static stat invoke(Idx f, Target &out, Pointer self, ara_str name, Mode qualifier) noexcept {
        return static_cast<stat>(f({code::attribute, static_cast<Code>(qualifier)}, &out, self.base, &name));
    }
};

/******************************************************************************/

ARA_DETECT_TRAIT(has_operator_equal, decltype(true || std::declval<T const&>() == std::declval<T const&>()));
ARA_DETECT_TRAIT(has_equal, decltype(Impl<T>::equal(std::declval<T const&>(), std::declval<T const&>(), DeclAny())));

struct Equal {
    enum stat : Stat {Impossible, False, True};

    template <class T>
    struct Default {
        static stat equal(T const& lhs, T const& rhs) {
            if constexpr(has_operator_equal_v<T>) {
                return (lhs == rhs) ? True : False;
            } else return Impossible;
        }

        static stat equal_nothrow(T const& lhs, T const& rhs) noexcept {
            if constexpr(has_equal_v<T>) {
                // return out.make_noexcept([&] {
                return Impl<T>::equal(lhs, rhs);
                // });
            } else return Impossible;
        }
    };

    static stat invoke(Idx f, Pointer lhs, Pointer rhs) noexcept {
        return static_cast<stat>(f({code::equal, {}}, lhs.base, rhs.base, {}));
    }
};

/******************************************************************************/

ARA_DETECT_TRAIT(has_compare, decltype(Impl<T>::compare(std::declval<T const&>(), std::declval<T const&>(), DeclAny())));
ARA_DETECT_TRAIT(has_operator_less, decltype(true || std::declval<T const&>() < std::declval<T const&>()));

struct Compare {
    enum stat : Stat {Less, Equal, Equivalent, Greater, Unordered};

    template <class T>
    struct Default {
        // If < and == are provided, assume strongly ordered
        // If only < is provided, assume partially ordered
        // Override to customize this behavior. Will improve with proper C++20 <=>.
        static stat compare(T const& lhs, T const& rhs) {
            if constexpr(has_operator_equal_v<T> && has_operator_less_v<T>) {
                if (lhs == rhs) return Equal;
                else if (lhs < rhs) return Less;
                else return Greater;
            } else if constexpr(has_operator_less_v<T>) {
                if (lhs < rhs) return Less;
                else if (rhs < lhs) return Greater;
                else return Equivalent;
            } else return Unordered;
        }

        static stat compare_nothrow(T const& lhs, T const& rhs) noexcept {
            if constexpr(has_compare_v<T>) {
                // return out.make_noexcept([&] {
                return Impl<T>::compare(lhs, rhs);
                // });
            } else return Unordered;
        }
    };

    static stat invoke(Idx f, Pointer lhs, Pointer rhs) noexcept {
        return static_cast<stat>(f({code::compare, {}}, lhs.base, rhs.base, {}));
    }
};

/******************************************************************************/

ARA_DETECT_TRAIT(has_hash, decltype(std::size_t() + std::hash<T>()(std::declval<T const&>())));

struct Hash {
    enum stat : Stat {OK, Impossible};

    template <class T>
    struct Default {
        static stat hash(std::size_t& out, T const& self) {
            if constexpr(has_hash_v<T>) {
                out = std::hash<T>()(self);
                return OK;
            } else return Impossible;
        }

        static stat hash_nothrow(std::size_t& out, T const& self) noexcept {
            // return out.make_noexcept([&] {
            return Impl<T>::hash(out, self);
            // });
        }
    };

    static stat invoke(Idx f, std::size_t& out, Pointer self) noexcept {
        return static_cast<stat>(f({code::hash, {}}, &out, self.base, {}));
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
    struct Default {
        static stat dump_nothrow(Target &out, Pointer source, Mode qualifier) noexcept {
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

    static stat invoke(Idx f, Target &out, Pointer source, Mode qualifier) noexcept {
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
    struct Default {
        // currently not planned that out can be a reference, I think
        static stat load_nothrow(Target &out, Pointer source, Mode qualifier) noexcept {
            if constexpr(has_load_v<T>) {
                static_assert(std::is_move_constructible_v<T>);
                return out.make_noexcept([&] {
                    ara_ref r{ara_mode_index(out.index(), static_cast<ara_mode>(qualifier)), source.base};
                    out.set_index<T>();
                    if (auto o = Impl<T>::load(reinterpret_cast<Ref &>(r))) {
                        if (auto p = out.get<T>()) {
                            if constexpr(std::is_move_assignable_v<T>) *p = std::move(*o);
                            else return None;
                        } else {
                            if (!out.emplace<T>(std::move(*o))) return None;
                        }

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

    static stat invoke(Idx f, Target &out, Pointer source, Mode qualifier) noexcept {
        return static_cast<stat>(f({code::load, static_cast<Code>(qualifier)}, &out, source.base, {}));
    }
};

/******************************************************************************/

ARA_DETECT_TRAIT(has_call, decltype(Impl<T>::call(DeclAny())));

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
    struct Default {
        static stat call_nothrow(Target &out, ArgView &args) noexcept {
            stat s = Impossible;
            if constexpr(has_call_v<T>) {
                Impl<T>::call({out, args, s});
            }
            return s;
        }
    };

    static stat invoke(Idx f, Target &out, ArgView &args) noexcept {
        return static_cast<stat>(f({code::call, {}}, {}, &out, reinterpret_cast<ara_args *>(&args)));
    }

    [[nodiscard]] static stat wrong_number(Target &, Code, Code) noexcept;
    static stat wrong_type(Target &, Code, Index, Qualifier) noexcept;
    [[nodiscard]] static stat wrong_return(Target &, Index, Qualifier) noexcept;
};

/******************************************************************************************/

ARA_DETECT_TRAIT(has_method, decltype(Impl<T>::method(DeclAny(), std::declval<T&&>())));
// stat operator()(Target *out, T, ArgView &args)

// Call an object
// -- if Target is null, do not return output (change later for Exception actually...)
// -- on success, return one of first 5 qualifiers
// -- Impossible: calling is not implemented
// -- WrongNumber: wrong number of arguments
// -- WrongReturn: wrong type or qualifier of return
// -- WrongType: wrong type of argument
// -- Exception: Exception while executing the function
struct Method : Call {

    template <class T>
    struct Default {
        static stat method_nothrow(Target &out, Pointer self, ArgView &args, Mode mode) noexcept {
            stat s = Impossible;
            if constexpr(has_method_v<T>) {
                switch (mode) {
                    case Mode::Stack: {Impl<T>::method({out, args, s}, self.load<T&&>()); break;}
                    case Mode::Heap:  {Impl<T>::method({out, args, s}, self.load<T&&>()); break;}
                    case Mode::Write: {Impl<T>::method({out, args, s}, self.load<T&>()); break;}
                    case Mode::Read:  {Impl<T>::method({out, args, s}, self.load<T const&>()); break;}
                }
            }
            return s;
        }
    };

    static stat invoke(Idx f, Target &out, Pointer self, Mode qualifier, ArgView &args) noexcept {
        return static_cast<stat>(f({code::method, static_cast<Code>(qualifier)}, &out, self.base, reinterpret_cast<ara_args *>(&args)));
    }
};

/******************************************************************************************/

template <class Op>
Stat return_stat(typename Op::stat status) {
    DUMP("Return output", static_cast<Stat>(status));
    return static_cast<Stat>(status);
}

/******************************************************************************************/

template <class T>
struct Default :
    Copy::Default<T>,
    Relocate::Default<T>,
    Swap::Default<T>,
    Destruct::Default<T>,
    Deallocate::Default<T>,
    Info::Default<T>,
    Name::Default<T>,
    Element::Default<T>,
    Attribute::Default<T>,
    Hash::Default<T>,
    Equal::Default<T>,
    Compare::Default<T>,
    Dump::Default<T>,
    Load::Default<T>,
    Call::Default<T>,
    Method::Default<T> {};

template <class T, class SFINAE>
struct Impl : Default<T> {};

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

    static Stat invoke(ara_input i, void* o, void* s, void* args) noexcept {
        static_assert(sizeof(T) >= 0, "Type should be complete");
        warn_unimplemented<T>();

        if (i.code != code::name) DUMP("Switch<", type_name<T>(), ">: ", code_name(i.code));
        if (!code::valid(i.code)) return -1; // ?

        switch (static_cast<code::codes>(i.code)) {
            case code::destruct:
                return Impl<T>::destruct(Pointer::from(o).load<T&>());

            case code::deallocate:
                return Impl<T>::deallocate(Pointer::from(o).load<T&>());

            case code::copy:
                return Impl<T>::copy(*static_cast<Target*>(o), Pointer::from(s).load<T const&>());

            case code::relocate:
                return Impl<T>::relocate(o, Pointer::from(s).load<T&&>());

            case code::swap:
                return Impl<T>::swap(Pointer::from(o).load<T&>(), Pointer::from(s).load<T&>());

            case code::hash:
                return Impl<T>::hash_nothrow(*static_cast<std::size_t*>(o), Pointer::from(s).load<T const&>());

            case code::element:
                return Impl<T>::element_nothrow(*static_cast<Target*>(o), Pointer::from(s), reinterpret_cast<std::uintptr_t>(args), static_cast<Mode>(i.tag));

            case code::attribute:
                return Impl<T>::attribute_nothrow(*static_cast<Target*>(o), Pointer::from(s), *static_cast<ara_str*>(args), static_cast<Mode>(i.tag));

            case code::compare:
                return Impl<T>::compare_nothrow(Pointer::from(o).load<T const&>(), Pointer::from(s).load<T const&>());

            case code::equal:
                return Impl<T>::equal_nothrow(Pointer::from(o).load<T const&>(), Pointer::from(s).load<T const&>());

            case code::load:
                return Impl<T>::load_nothrow(*static_cast<Target *>(o), Pointer::from(s), static_cast<Mode>(i.tag));

            case code::dump:
                return Impl<T>::dump_nothrow(*static_cast<Target *>(o), Pointer::from(s), static_cast<Mode>(i.tag));

            case code::call:
                return Impl<T>::call_nothrow(*static_cast<Target *>(o), *reinterpret_cast<ArgView *>(args));
            
            case code::method:
                return Impl<T>::method_nothrow(*static_cast<Target *>(o), Pointer::from(s), *reinterpret_cast<ArgView *>(args), static_cast<Mode>(i.tag));

            case code::name:
                return Impl<T>::name(*static_cast<ara_str *>(o));

            case code::info:
                return Impl<T>::info(*static_cast<Idx *>(o), *static_cast<void const **>(s));

            case code::check:
                return code::valid(i.code);
        }
    }
};

/******************************************************************************************/

template <class SFINAE>
struct Switch<void, SFINAE> {
    static Stat invoke(ara_input i, void* o, void* s, void*) noexcept {
        switch (i.code) {
            case code::name: {
                return Name::Default<void>::name(*static_cast<ara_str *>(o));
            }
            case code::info: {
                return Info::Default<void>::info(*static_cast<Idx *>(o), *static_cast<void const **>(s));
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
    Name::invoke(*this, out);
    return std::string_view(out.data, out.size);
}

/******************************************************************************************/

}
