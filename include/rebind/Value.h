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

struct Value : protected rebind_value {

    /**************************************************************************/

    constexpr Value() noexcept : rebind_value{nullptr, nullptr} {}

    template <class T, std::enable_if_t<is_usable<unqualified<T>>, int> = 0>
    Value(T &&t) noexcept(std::is_nothrow_move_constructible_v<unqualified<T>>)
        : rebind_value{fetch<unqualified<T>>(), raw::alloc<unqualified<T>>(static_cast<T &&>(t))} {}

    template <class T, class ...Args>
    Value(Type<T> t, Args &&...args)
        : rebind_value{fetch<T>(), raw::alloc<T>(static_cast<Args &&>(args)...)} {}

    Value(Value const &v)
        : rebind_value{v.ind, v.ptr ? v.ind->copy(v.ptr) : nullptr} {}

    Value(Ref const &r)
        : rebind_value{r.index(), r.address() ? r.index()->copy(r.address()) : nullptr} {}

    Value &operator=(Value const &v) {
        ind = v.ind;
        ptr = v.ptr ? v.ind->copy(v.ptr) : nullptr;
        return *this;
    }

    void release() noexcept {ind = nullptr; ptr = nullptr;}
    bool destroy_if() noexcept {return ptr ? ind->destroy(ptr), true: false;}

    Value(Value &&v) noexcept : rebind_value(static_cast<rebind_value &&>(v)) {v.release();}

    Value &operator=(Value &&v) noexcept {
        rebind_value::operator=(std::move(v));
        v.release();
        return *this;
    }

    ~Value() {destroy_if();}

    void reset() {destroy_if(); ind = nullptr; ptr = nullptr;}

    /**************************************************************************/

    // Index index() const noexcept {return ind ? ind->index : Index();}

    std::string_view name() const noexcept {return raw::name(ind);}

    rebind_value const &as_raw() const noexcept {return static_cast<rebind_value const &>(*this);}
    rebind_value &as_raw() noexcept {return static_cast<rebind_value &>(*this);}

    constexpr bool has_value() const noexcept {return ptr;}

    explicit constexpr operator bool() const noexcept {return ptr;}

    constexpr Index index() const noexcept {return ind;}

    constexpr void * address() const noexcept {return ptr;}

    template <class T>
    bool matches() const noexcept {return raw::matches<T>(ind);}

    /**************************************************************************/

    template <class T>
    unqualified<T> * set_if(T &&t) {
        using U = unqualified<T>;
        if (!ptr && raw::matches<U>(ind))
            ptr = raw::alloc<U>(static_cast<T &&>(t));
        return static_cast<U *>(ptr);
    }

    template <class T, class ...Args>
    T * place_if(Args &&...args) {
        if (!ptr && raw::matches<T>(ind))
            ptr = raw::alloc<T>(static_cast<Args &&>(args)...);
        return static_cast<unqualified<T> *>(ptr);
    }

    template <class T, class ...Args>
    T & place(Args &&...args) {
        destroy_if();
        T *out = raw::alloc<T>(static_cast<Args &&>(args)...);
        ptr = out;
        ind = fetch<T>();
        return *out;
    }

    /**************************************************************************/

    template <class T>
    T *target() & {return raw::target<T>(ind, ptr);}

    template <class T>
    T *target() && {return raw::target<T>(ind, ptr);}

    template <class T>
    T const *target() const & {return raw::target<T>(ind, ptr);}

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
        else return raw::request(ind, ptr, s, t, Const);
    }

    template <class T>
    Maybe<T> request(Scope &s, Type<T> t={}) & {
        if constexpr(std::is_convertible_v<Value &, T>) return some<T>(*this);
        else return raw::request(ind, ptr, s, t, Lvalue);
    }

    template <class T>
    Maybe<T> request(Scope &s, Type<T> t={}) && {
        if constexpr(std::is_convertible_v<Value &&, T>) return some<T>(std::move(*this));
        else return raw::request(ind, ptr, s, t, Rvalue);
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

    bool assign_if(Ref const &p) {return raw::assign_if(ind, ptr, p);}

    /**************************************************************************/

    template <class T>
    T cast(Type<T> t={}) const & {Scope s; return cast(s, t);}

    template <class T>
    T cast(Type<T> t={}) & {Scope s; return cast(s, t);}

    template <class T>
    T cast(Type<T> t={}) && {Scope s; return std::move(*this).cast(s, t);}

    /**************************************************************************/

    bool request_to(Value &v) const & {return raw::request_to(ind, ptr, v, Const);}
    bool request_to(Value &v) & {return raw::request_to(ind, ptr, v, Lvalue);}
    bool request_to(Value &v) && {return raw::request_to(ind, ptr, v, Rvalue);}
};

/******************************************************************************/

static_assert(std::is_standard_layout_v<Value>);

template <>
struct is_trivially_relocatable<Value> : std::true_type {};

/******************************************************************************/

// class OutputValue : protected Value {
//     using Value::name;
//     using Value::index;
//     using Value::matches;
//     using Value::has_value;
//     using Value::as_erased;
//     using Value::operator bool;

//     template <class T>
//     OutputValue(Type<T>) : Value(fetch<T>(), nullptr) {}

//     OutputValue(OutputValue const &) = delete;

// };

inline constexpr Ref::Ref(Value const &v) : Ref(v.index(), v.address(), Const) {}
inline constexpr Ref::Ref(Value &v) : Ref(v.index(), v.address(), Lvalue) {}
inline constexpr Ref::Ref(Value &&v) : Ref(v.index(), v.address(), Rvalue) {}

/******************************************************************************/

}
