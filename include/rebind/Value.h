#pragma once
#include "Ref.h"
#include "Error.h"
#include "Type.h"
#include <optional>

namespace rebind {

template <class T, class ...Args>
T * alloc(Args &&...args) {
    assert_usable<T>();
    if constexpr(std::is_constructible_v<T, Args &&...>) {
        return new T(static_cast<Args &&>(args)...);
    } else {
        return new T{static_cast<Args &&>(args)...};
    }
}

template <class T>
using Maybe = std::conditional_t<std::is_reference_v<T>, std::remove_reference_t<T> *, std::optional<T>>;

template <class T, class Arg>
Maybe<T> some(Arg &&t) {
    if constexpr(std::is_reference_v<T>) return std::addressof(t);
    else return static_cast<Arg &&>(t);
}

/******************************************************************************/

class Value : protected Erased {
    using Erased::Erased;

public:

    using Erased::name;
    using Erased::address;
    using Erased::index;
    using Erased::table;
    using Erased::matches;
    using Erased::has_value;
    using Erased::as_erased;
    using Erased::operator bool;

    /**************************************************************************/

    constexpr Value() noexcept = default;

    template <class T, std::enable_if_t<is_usable<unqualified<T>>, int> = 0>
    Value(T &&t) noexcept : Erased(alloc<unqualified<T>>(static_cast<T &&>(t))) {}

    template <class T, class ...Args>
    Value(Type<T> t, Args &&...args) : Erased(alloc<T>(static_cast<Args &&>(args)...)) {}

    Value(Value const &v) : Erased(v.allocate_copy()) {}

    Value(Ref const &v) : Erased(v.as_erased().allocate_copy()) {}

    Value &operator=(Value const &v) {
        Erased::operator=(v.allocate_copy());
        return *this;
    }

    Value(Value &&v) noexcept : Erased(static_cast<Erased &&>(v)) {Erased::reset();}

    Value &operator=(Value &&v) noexcept {
        Erased::operator=(std::move(v));
        static_cast<Erased &>(v).reset();
        return *this;
    }

    ~Value() {Erased::try_destroy();}

    void reset() {Erased::try_destroy(); Erased::reset();}

    /**************************************************************************/

    template <class T>
    unqualified<T> * set_if(T &&t) {
        using U = unqualified<T>;
        if (!ptr && matches<U>()) ptr = alloc<U>(static_cast<T &&>(t));
        return static_cast<U *>(ptr);
    }

    template <class T, class ...Args>
    T * place_if(Args &&...args) {
        if (!ptr && matches<T>()) ptr = alloc<T>(static_cast<Args &&>(args)...);
        return static_cast<unqualified<T> *>(ptr);
    }

    template <class T, class ...Args>
    T & place(Args &&...args) {
        Erased::try_destroy();
        return *Erased::reset(alloc<T>(static_cast<Args &&>(args)...));
    }

    /**************************************************************************/

    template <class T>
    T *target() & {return Erased::target<T>();}

    template <class T>
    T *target() && {return Erased::target<T>();}

    template <class T>
    T const *target() const & {return Erased::target<T>();}

    // template <class T, class ...Args>
    // static Value from(Args &&...args) {
    //     static_assert(std::is_constructible_v<T, Args &&...>);
    //     return Value(new T{static_cast<Args &&>(args)...});
    // }

    template <class T>
    void set(T &&t) {*this = Value(static_cast<T &&>(t));}

    /**************************************************************************/

    template <class T>
    Maybe<T> request(Scope &s, Type<T> t={}) const & {
        if constexpr(std::is_convertible_v<Value const &, T>) return some<T>(*this);
        else return Erased::request(s, t, Const);
    }

    template <class T>
    Maybe<T> request(Scope &s, Type<T> t={}) & {
        if constexpr(std::is_convertible_v<Value &, T>) return some<T>(*this);
        else return Erased::request(s, t, Lvalue);
    }

    template <class T>
    Maybe<T> request(Scope &s, Type<T> t={}) && {
        if constexpr(std::is_convertible_v<Value &&, T>) return some<T>(std::move(*this));
        else return Erased::request(s, t, Rvalue);
    }

    /**************************************************************************/

    template <class T>
    Maybe<T> request(Type<T> t={}) const & {Scope s; return request(s, t);}

    template <class T>
    Maybe<T> request(Type<T> t={}) & {Scope s; return request(s, t);}

    template <class T>
    Maybe<T> request(Type<T> t={}) && {Scope s; return std::move(*this).request(s, t);}

    /**************************************************************************/

    template <class T>
    T cast(Scope &s, Type<T> t={}) const & {
        if (auto p = from_ref(s, t)) return static_cast<T &&>(*p);
        throw std::move(s.set_error("invalid cast (rebind::Value const &)"));
    }

    template <class T>
    T cast(Scope &s, Type<T> t={}) & {
        if (auto p = from_ref(s, t)) return static_cast<T &&>(*p);
        throw std::move(s.set_error("invalid cast (rebind::Value &)"));
    }

    template <class T>
    T cast(Scope &s, Type<T> t={}) && {
        if (auto p = from_ref(s, t)) return static_cast<T &&>(*p);
        throw std::move(s.set_error("invalid cast (rebind::Value &&)"));
    }

    bool assign_if(Ref const &p) {return Erased::assign_if(p);}

    /**************************************************************************/

    template <class T>
    T cast(Type<T> t={}) const & {Scope s; return cast(s, t);}

    template <class T>
    T cast(Type<T> t={}) & {Scope s; return cast(s, t);}

    template <class T>
    T cast(Type<T> t={}) && {Scope s; return std::move(*this).cast(s, t);}

    /**************************************************************************/

    bool request_to(Value &v) const & {return Erased::request_to(v, Const);}
    bool request_to(Value &v) & {return Erased::request_to(v, Lvalue);}
    bool request_to(Value &v) && {return Erased::request_to(v, Rvalue);}
};

/******************************************************************************/

class OutputValue : protected Value {
    using Value::name;
    using Value::index;
    using Value::matches;
    using Value::has_value;
    using Value::as_erased;
    using Value::operator bool;

    template <class T>
    OutputValue(Type<T>) : Value(get_table<T>(), nullptr) {}

    OutputValue(OutputValue const &) = delete;

};

inline constexpr Ref::Ref(Value const &v) : Ref(v.as_erased(), Const) {}
inline constexpr Ref::Ref(Value &v) : Ref(v.as_erased(), Lvalue) {}
inline constexpr Ref::Ref(Value &&v) : Ref(v.as_erased(), Rvalue) {}

/******************************************************************************/

}
