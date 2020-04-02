#pragma once
#include "Ref.h"
#include <optional>

namespace rebind {

template <class T>
using Maybe = std::conditional_t<std::is_reference_v<T>, std::remove_reference_t<T> *, std::optional<T>>;

template <class T, class Arg>
Maybe<T> some(Arg &&t) {
    if constexpr(std::is_reference_v<T>) return std::addressof(t);
    else return static_cast<Arg &&>(t);
}

/******************************************************************************/

// struct CopyError : std::runtime_error {
//     using std::runtime_error::runtime_error;
// };

/******************************************************************************/

struct RawValue : protected rebind_value {

    constexpr RawValue() noexcept : rebind_value{nullptr, nullptr} {}
    constexpr RawValue(Index i, void *p) noexcept : rebind_value{i, p} {}

    ~RawValue() {if (ptr) raw::drop(*this);}

    RawValue(RawValue &&v) noexcept : rebind_value(static_cast<rebind_value &&>(v)) {v.release();}

    RawValue &operator=(RawValue &&v) noexcept {
        rebind_value::operator=(std::move(v));
        v.release();
        return *this;
    }

    RawValue(RawValue const &v) {
        if (stat::copy::ok != raw::copy(*this, v.index(), v.address())) throw std::runtime_error("no copy");
    }

    RawValue &operator=(RawValue const &v) {
        if (stat::copy::ok != raw::copy(*this, v.index(), v.address())) throw std::runtime_error("no copy");
        return *this;
    }

    /**************************************************************************/

    void release() noexcept {ind = nullptr; ptr = nullptr;}

    void reset() {if (has_value()) {raw::drop(*this); release();}}

    constexpr Index index() const noexcept {return {ind};}

    std::string_view name() const noexcept {return index().name();}

    constexpr void * address() const noexcept {return ptr;}

    constexpr bool has_value() const noexcept {return address();}
    explicit constexpr operator bool() const noexcept {return has_value();}


    template <class T>
    bool matches() const noexcept {return index() == fetch<T>();}
};

/******************************************************************************/

struct Value : public RawValue {
    constexpr Value() noexcept = default;

    constexpr Value(std::nullptr_t) noexcept : Value() {}

    template <class T, std::enable_if_t<is_usable<unqualified<T>>, int> = 0>
    Value(T &&t) noexcept(std::is_nothrow_move_constructible_v<unqualified<T>>)
        : RawValue{Type<unqualified<T>>(), raw::alloc<unqualified<T>>(static_cast<T &&>(t))} {}

    template <class T, class ...Args, std::enable_if_t<is_usable<T>, int> = 0>
    Value(Type<T> t, Args &&...args)
        : RawValue{t, raw::alloc<T>(static_cast<Args &&>(args)...)} {}


    Value(Ref const &r) {
        if (stat::copy::ok != raw::copy(*this, r.index(), r.address())) throw std::runtime_error("no copy");
    }

    /**************************************************************************/

    template <class T, class ...Args>
    T & emplace(Type<T>, Args &&...args) {
        assert_usable<T>();
        if (ptr) raw::drop(*this);
        T *out = raw::alloc<T>(static_cast<Args &&>(args)...);
        ptr = out;
        ind = fetch<T>();
        return *out;
    }

    template <class T, std::enable_if_t<!is_type<T>, int> = 0>
    unqualified<T> & emplace(T &&t) {
        return emplace(Type<unqualified<T>>(), static_cast<T &&>(t));
    }

    /**************************************************************************/

    template <class T>
    T *target() & {return raw::target<T>(index(), address());}

    template <class T>
    T *target() && {return raw::target<T>(index(), address());}

    template <class T>
    T const *target() const & {return raw::target<T>(index(), address());}

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
        else return raw::request(index(), address(), s, t, Const);
    }

    template <class T>
    Maybe<T> request(Scope &s, Type<T> t={}) & {
        if constexpr(std::is_convertible_v<Value &, T>) return some<T>(*this);
        else return raw::request(index(), address(), s, t, Lvalue);
    }

    template <class T>
    Maybe<T> request(Scope &s, Type<T> t={}) && {
        if constexpr(std::is_convertible_v<Value &&, T>) return some<T>(std::move(*this));
        else return raw::request(index(), address(), s, t, Rvalue);
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

    bool assign_if(Ref const &p) {return stat::assign_if::ok == raw::assign_if(index(), address(), p);}

    /**************************************************************************/

    template <class T>
    T cast(Type<T> t={}) const & {Scope s; return cast(s, t);}

    template <class T>
    T cast(Type<T> t={}) & {Scope s; return cast(s, t);}

    template <class T>
    T cast(Type<T> t={}) && {Scope s; return std::move(*this).cast(s, t);}

    /**************************************************************************/

    bool request_to(Output &v) const & {return stat::request::ok == raw::request_to(v, index(), address(), Const);}
    bool request_to(Output &v) & {return stat::request::ok == raw::request_to(v, index(), address(), Lvalue);}
    bool request_to(Output &v) && {return stat::request::ok == raw::request_to(v, index(), address(), Rvalue);}

    /**************************************************************************/

    template <class ...Args>
    Value call_value(Args &&...args) const;

    template <class ...Args>
    Ref call_ref(Args &&...args) const;

    bool call_to(Value &, ArgView) const;

    bool call_to(Ref &, ArgView) const;
};

/******************************************************************************/

static_assert(std::is_standard_layout_v<Value>);

template <>
struct is_trivially_relocatable<Value> : std::true_type {};

inline Ref::Ref(Value const &v) : Ref(v.index(), v.address(), Const) {DUMP("ref from value");}

inline Ref::Ref(Value &v) : Ref(v.index(), v.address(), Lvalue) {DUMP("ref from value");}

inline Ref::Ref(Value &&v) : Ref(v.index(), v.address(), Rvalue) {DUMP("ref from value");}

/******************************************************************************/

struct Output : public RawValue {
    template <class T>
    explicit Output(Type<T> t) : RawValue{t, nullptr} {}

    Output(Output const &) = delete;

    template <class T>
    unqualified<T> * set_if(T &&t) {
        using U = unqualified<T>;
        if (!ptr && this->matches<U>())
            ptr = raw::alloc<U>(static_cast<T &&>(t));
        return static_cast<U *>(ptr);
    }

    template <class T, class ...Args>
    T * emplace_if(Args &&...args) {
        if (!ptr && this->matches<T>())
            ptr = raw::alloc<T>(static_cast<Args &&>(args)...);
        return static_cast<unqualified<T> *>(ptr);
    }
};

/******************************************************************************/

}

#include "Default.h"
