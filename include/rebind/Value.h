#pragma once
#include "Pointer.h"
#include "Error.h"
#include <optional>

namespace rebind {

/******************************************************************************/

class Value : protected Opaque {
    using Opaque::Opaque;
public:

    using Opaque::name;
    using Opaque::index;
    using Opaque::has_value;
    using Opaque::operator bool;

    /**************************************************************************/

    constexpr Value() noexcept = default;

    template <class T>
    Value(T t) noexcept : Opaque(new T(std::move(t))) {}

    Value(Value const &v) : Opaque(v.allocate_copy()) {}

    Value(Value &&v) noexcept : Opaque(static_cast<Opaque &&>(v)) {
        v.reset_pointer();
    }

    Value &operator=(Value const &v) {
        Opaque::operator=(v.allocate_copy());
        return *this;
    }

    Value &operator=(Value &&v) {
        Opaque::operator=(std::move(v));
        v.reset_pointer();
        return *this;
    }

    ~Value() {Opaque::try_destroy();}

    void reset() {Opaque::try_destroy(); reset_pointer();}

    /**************************************************************************/

    template <class T, class ...Args>
    T &emplace(Type<T>, Args &&...args) {return *new T(static_cast<Args &&>(args)...);}

    template <class T>
    T *target() & {return Opaque::target<T>();}

    template <class T>
    T *target() && {return Opaque::target<T>();}

    template <class T>
    T const *target() const & {return Opaque::target<T>();}

    template <class T, class ...Args>
    static Value from(Args &&...args) {
        static_assert(std::is_constructible_v<T, Args &&...>);
        return Value(new T{static_cast<Args &&>(args)...});
    }

    template <class T>
    static Value from(T &&t) {return static_cast<T &&>(t);}

    /**************************************************************************/

    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> * request(Scope &s, Type<T> t={}) const & {return Opaque::request_reference<T>(Const);}

    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> * request(Scope &s, Type<T> t={}) & {return Opaque::request_reference<T>(Lvalue);}

    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> * request(Scope &s, Type<T> t={}) && {return std::move(*this).request_reference<T>(Rvalue);}

    /**************************************************************************/

    template <class T, std::enable_if_t<!std::is_reference_v<T>, int> = 0>
    std::optional<T> request(Scope &s, Type<T> t={}) const & {return Pointer(*this, Const).request(s, t);}

    template <class T, std::enable_if_t<!std::is_reference_v<T>, int> = 0>
    std::optional<T> request(Scope &s, Type<T> t={}) & {return Pointer(*this, Lvalue).request(s, t);}

    template <class T, std::enable_if_t<!std::is_reference_v<T>, int> = 0>
    std::optional<T> request(Scope &s, Type<T> t={}) && {return Pointer(*this, Rvalue).request(s, t);}

    /**************************************************************************/

    template <class T>
    auto request(Type<T> t={}) const & {Scope s; return request(s, t);}

    template <class T>
    auto request(Type<T> t={}) & {Scope s; return request(s, t);}

    template <class T>
    auto request(Type<T> t={}) && {Scope s; return std::move(*this).request(s, t);}

    /**************************************************************************/

    template <class T>
    T cast(Scope &s, Type<T> t={}) const & {
        if (auto p = request(s, t)) return static_cast<T &&>(*p);
        throw std::move(s.set_error("invalid cast (rebind::Value const &)"));
    }

    template <class T>
    T cast(Scope &s, Type<T> t={}) & {
        if (auto p = request(s, t)) return static_cast<T &&>(*p);
        throw std::move(s.set_error("invalid cast (rebind::Value &)"));
    }

    template <class T>
    T cast(Scope &s, Type<T> t={}) && {
        if (auto p = request(s, t)) return static_cast<T &&>(*p);
        throw std::move(s.set_error("invalid cast (rebind::Value &&)"));
    }

    /**************************************************************************/

    template <class T>
    T cast(Type<T> t={}) const & {Scope s; return cast(s, t);}

    template <class T>
    T cast(Type<T> t={}) & {Scope s; return cast(s, t);}

    template <class T>
    T cast(Type<T> t={}) && {Scope s; return cast(s, t);}

    /**************************************************************************/
};

/******************************************************************************/

}