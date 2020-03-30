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

/// Return new-allocated copy of the object
template <class T>
bool default_copy(rebind_value &v, T const &t) noexcept {
    if constexpr(std::is_nothrow_constructible_v<T>) {
        v.ptr = new T(t);
        return true;
    } else if constexpr(std::is_copy_constructible_v<T>) {
        try {
            v.ptr = new T(t);
            return true;
        } catch (...) {
            return false;
        }
    } else return false;
}

/******************************************************************************/

/// Delete the object pointer
template <class T>
bool default_drop(rebind_value &t) noexcept {delete static_cast<T *>(t.ptr); return true;}

/******************************************************************************/

template <class T>
bool default_info(void const *&o) noexcept {o = &typeid(T); return true;}

/******************************************************************************/

template <class T>
bool default_from_ref(Value &v, Ref const &p, Scope &s) noexcept {
    try {
        if (auto o = FromRef<T>()(p, s)) return v.emplace(Type<T>(), std::move(*o)), true;
    } catch (...) {}
#warning "from_ref exception not implemented"
    return false;
}

/******************************************************************************/

template <class T>
bool default_to_value(Output &v, T &t, Qualifier const q) noexcept {
    DUMP("to_value ", type_name<T>(), " ", v.name());
    assert_usable<T>();
    try {
        if (q == Lvalue) {
            return ToValue<T>()(v, t);
        } else if (q == Rvalue) {
            return ToValue<T>()(v, std::move(t));
        } else {
            return ToValue<T>()(v, static_cast<T const &>(t));
        }
    } catch (...) {
#warning "to_value exception not implemented"
        return false;
    }
}

/******************************************************************************/

template <class T>
bool default_to_ref(Ref &v, T &t, Qualifier const q) noexcept {
    assert_usable<T>();
    try {
        if (q == Lvalue) {
            return ToRef<T>()(v, t);
        } else if (q == Rvalue) {
            return ToRef<T>()(v, std::move(t));
        } else {
            return ToRef<T>()(v, static_cast<T const &>(t));
        }
    } catch (...) {
#warning "exception not implemented"
        return false;
    }
}

/******************************************************************************/

template <class T>
bool default_assign_to(T &self, Ref const &other) noexcept {
    assert_usable<T>();
    DUMP("assign_if: ", type_name<T>());
    if (auto p = other.request<T &&>()) {
        DUMP("assign_if: got T &&");
        self = std::move(*p);
        return true;
    }
    if constexpr (std::is_copy_assignable_v<T>) {
        if (auto p = other.request<T const &>()) {
            DUMP("assign_if: got T const &");
            self = *p;
            return true;
        }
    }
#warning "exception not implemented"
    if (auto p = other.request<T>()) {
        DUMP("assign_if: T succeeded, type=", typeid(*p).name());
        self = std::move(*p);
        return true;
    }
    return false;
}

/******************************************************************************************/

template <class T, class SFINAE=void>
struct Impl {
    static rebind_bool impl(Tag t, void *o, void *b, rebind_args args) noexcept {
        if (t != tag::name) DUMP("impl::", tag_name(t), ": ", type_name<T>());

        if constexpr(is_usable<T>) {
            switch (t) {
                case tag::drop:
                    return default_drop<T>(*static_cast<rebind_value *>(o));
                case tag::copy:
                    return default_copy(*static_cast<rebind_value *>(o), *static_cast<T const *>(b));
                case tag::request_to_value:
                    return default_to_value(*static_cast<Output *>(o), *static_cast<T *>(b), Qualifier());
                case tag::request_to_ref:
                    return default_to_ref(*static_cast<Ref *>(o), *static_cast<T *>(b), Qualifier());
            }

            if constexpr(std::is_move_assignable_v<T>) if (t == tag::assign_to) {
                return default_assign_to(*static_cast<T *>(b), *static_cast<Ref const *>(o));
            }

            if constexpr(CallToValue<T>::value) if (t == tag::call_to_value) {
                return CallToValue<T>::call_to(*static_cast<Value *>(o), *static_cast<T const *>(b), static_cast<ArgView &>(args));
            }

            if constexpr(CallToRef<T>::value) if (t == tag::call_to_ref) {
                return CallToRef<T>::call_to(*static_cast<Ref *>(o), *static_cast<T const *>(b), static_cast<ArgView &>(args));
            }
        }

        if (t == tag::check)
            return reinterpret_cast<std::uintptr_t>(o) < 12;

        if (t == tag::name)
            return TypeName<T>::impl(*static_cast<std::string_view *>(o));

        if (t == tag::info)
            return default_info<T>(*static_cast<void const **>(o));

        return false;
    }
};

/******************************************************************************************/

template <class T>
Index fetch() noexcept {return &Impl<T>::impl;}

/******************************************************************************************/

}
