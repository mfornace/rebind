#pragma once
#include "Call.h"
#include "Type.h"
#include "Common.h"
#include "Convert.h"

#include <map>
#include <stdexcept>
#include <array>
#include <type_traits>

namespace rebind {

/******************************************************************************/

/*
void drop      (void *        []            [])
bool copy      (void *,       void const *  [])
bool call      (void *,       void const *, ArgView, Caller &&)
bool method    (void *,       void const *, ArgView, Caller &&, name, tag)
bool name      (void *,       [],           [])
bool to_value  (Value &,      void *,       Qualifier)
bool to_ref    (Ref &,        void *,       Qualifier)
bool assign_if (void *,       [],           Ref const)
*/

/******************************************************************************************/

/// Insert new-allocated copy of the object
// template <class T>
// stat::copy default_copy(rebind_value &v, T const &t) noexcept {
//     if constexpr(std::is_nothrow_copy_constructible_v<T>) {
//         v.ptr = new T(t);
//         return stat::copy::ok;
//     } else if constexpr(std::is_copy_constructible_v<T>) {
//         try {
//             v.ptr = new T(t);
//             return stat::copy::ok;
//         } catch (...) {
//             return stat::copy::exception;
//         }
//     } else return stat::copy::unavailable;
// }

/******************************************************************************/

/// Delete the held object
template <class T>
stat::drop default_dealloc(Storage &s) noexcept {
    if (is_stack_type<T>) static_cast<T *>(&s)->~T();
    else delete static_cast<T *>(s.pointer);
    return stat::drop::ok;
}

/******************************************************************************/

template <class T>
stat::drop default_destruct(void *t) noexcept {
    static_cast<T *>(t)->~T();
    return stat::drop::ok;
}

/******************************************************************************/

/// Return a handle to std::type_info or some language-specific equivalent
template <class T>
stat::info default_info(void const *&o) noexcept {
    o = &typeid(T);
    return stat::info::ok;
}

/******************************************************************************/

/// Insert type T in a Value using data from a Ref
// template <class T>
// stat::from_ref default_from_ref(Value &v, Ref const &p, Scope &s) noexcept {
//     try {
//         if (auto o = FromRef<T>()(p, s)) {
//             v.emplace(Type<T>(), std::move(*o));
//             return stat::from_ref::ok;
//         } else {
//             return stat::from_ref::none;
//         }
//     } catch (...) {
// #warning "from_ref exception not implemented"
//         return stat::from_ref::exception;
//     }
// }

/******************************************************************************/

/// Use T to create a Value
template <class T>
stat::dump default_dump(Value &v, T &t, Qualifier const q) noexcept;
//  {
//     DUMP("to_value ", type_name<T>(), " ", v.name());
//     try { switch {
//         case Lvalue:
//             return ToValue<T>()(v, t) ? stat::request::ok : stat::request::none;
//         case Rvalue:
//             return ToValue<T>()(v, std::move(t)) ? stat::request::ok : stat::request::none;
//         case Const:
//             return ToValue<T>()(v, static_cast<T const &>(t)) ? stat::request::ok : stat::request::none;
//     } } catch (...) {
// #warning "to_value exception not implemented"
//     }
//     return stat::request::exception;
// }

/******************************************************************************/

/// Assign to self from another Ref
// template <class T>
// stat::assign_if default_assign_to(T &self, Ref const &other) noexcept {
//     assert_usable<T>();
//     DUMP("assign_if: ", type_name<T>());
//     try {
//         if (auto p = other.request<T &&>()) {
//             DUMP("assign_if: got T &&");
//             self = std::move(*p);
//             return stat::assign_if::ok;
//         }

//         if constexpr (std::is_copy_assignable_v<T>) {
//             if (auto p = other.request<T const &>()) {
//                 DUMP("assign_if: got T const &");
//                 self = *p;
//                 return stat::assign_if::ok;
//             }
//         }

//         if (auto p = other.request<T>()) {
//             DUMP("assign_if: T succeeded, type=", typeid(*p).name());
//             self = std::move(*p);
//             return stat::assign_if::ok;
//         }

//         return stat::assign_if::none;
//     } catch (...) {
//         return stat::assign_if::none;
//     }
// }

/******************************************************************************************/

template <class T, class SFINAE=void>
struct Impl {
    static Stat impl(Tag t, void *o, void *b, rebind_args args) noexcept {
        if (t != tag::name) DUMP("impl::", tag_name(t), ": ", type_name<T>());

        switch(t) {
            case tag::dealloc: {
                if constexpr(!is_manageable<T>) return stat::put(stat::drop::unavailable);
                else return stat::put(default_dealloc<T>(*static_cast<Storage *>(o)));
            }
            case tag::destruct: {
                if constexpr(!is_manageable<T>) return stat::put(stat::drop::unavailable);
                else return stat::put(default_destruct<T>(o));
            }
            case tag::copy: {
                // if constexpr(!is_manageable<T>) return stat::put(stat::copy::unavailable);
                // else return stat::put(default_copy(*static_cast<rebind_value *>(o), *static_cast<T const *>(b)));
            }
            case tag::dump: {
#warning "fix qualifier"
                return stat::put(default_dump(*static_cast<Output *>(o), *static_cast<T *>(b), Qualifier()));
            }
            case tag::assign: {
                // if constexpr(!std::is_move_assignable_v<T>) return stat::put(stat::assign_if::unavailable);
                // else return stat::put(default_assign(*static_cast<T *>(b), *static_cast<Ref const *>(o)));
            }
            case tag::call: {
                // if constexpr(!Call<T>::value) return stat::put(stat::call::unavailable);
                // else return stat::put(Call<T>::call_to(*static_cast<Value *>(o), *static_cast<T const *>(b), static_cast<ArgView &>(args)));
            }
            case tag::name: {
                return TypeName<T>::impl(*static_cast<std::string_view *>(o));
            }
            case tag::info: {
                return stat::put(default_info<T>(*static_cast<void const **>(o)));
            }
            case tag::check: {
                return reinterpret_cast<std::uintptr_t>(o) < 12; // legal cast
            }
        }
        return -1; // ?
    }
};

/******************************************************************************************/

template <class T>
Index fetch() noexcept {return &Impl<T>::impl;}

/******************************************************************************************/

}
