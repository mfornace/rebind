#pragma once
#include <rebind/Ref.h>

namespace rebind {

/******************************************************************************/

enum class Loc : rebind_qualifier {Trivial, Relocatable, Stack, Heap};

/******************************************************************************/

struct Copyable;

union Storage {
    void *pointer;
    char data[24];
};

/******************************************************************************/

struct Value {
    rebind_index idx = nullptr;
    Storage storage;

    Value() noexcept = default;

    Value(std::nullptr_t) noexcept : Value() {}

    template <class T, class ...Args, std::enable_if_t<is_usable<T>, int> = 0>
    Value(Type<T> t, Args &&...args);

    template <class T, std::enable_if_t<is_usable<unqualified<T>>, int> = 0>
    Value(T &&t) : Value{Type<unqualified<T>>(), static_cast<T &&>(t)} {}

    Value(Value &&v) noexcept;

    Value &operator=(Value &&v) noexcept;

    ~Value() {
        if (has_value()) index().call<stat::drop>(tag::dealloc, {}, storage.pointer);
    }

    /**************************************************************************/

    void reset() noexcept {
        if (has_value()) {
            index().call<stat::drop>(tag::dealloc, {}, storage.pointer);
            release();
        }
    }

    void release() noexcept {idx = nullptr;}

    Loc location() const noexcept {return static_cast<Loc>(rebind_get_tag(idx));}

    Index index() const noexcept {return {rebind_get_index(idx)};}

    std::string_view name() const noexcept {return index().name();}

    constexpr bool has_value() const noexcept {return idx;}

    explicit constexpr operator bool() const noexcept {return has_value();}

    /**************************************************************************/

    template <class T, class ...Args>
    T & emplace(Type<T>, Args &&...args) {
        assert_usable<T>();
        reset();
#warning "not done"
        T *out = raw::alloc<T>(static_cast<Args &&>(args)...);
        storage.pointer = out;
        idx = Index::of<T>().fptr;
        return *out;
    }

    template <class T, std::enable_if_t<!is_type<T>, int> = 0>
    unqualified<T> & emplace(T &&t) {
        return emplace(Type<unqualified<T>>(), static_cast<T &&>(t));
    }

    /**************************************************************************/

    std::optional<Copyable> clone() const;

    // template <class T>
    // T *target() & {return raw::target<T>(index(), address());}

    // template <class T>
    // T *target() && {return raw::target<T>(index(), address());}

    // template <class T>
    // T const *target() const & {return raw::target<T>(index(), address());}

    // template <class T, class ...Args>
    // static Value from(Args &&...args) {
    //     static_assert(std::is_constructible_v<T, Args &&...>);
    //     return Value(new T{static_cast<Args &&>(args)...});
    // }

    // template <class T>
    // void set(T &&t) {*this = Value(static_cast<T &&>(t));}

    /**************************************************************************/

    // template <class T>
    // Maybe<T> request(Scope &s, Type<T> t={}) const &;
    //  {
        // if constexpr(std::is_convertible_v<Value const &, T>) return some<T>(*this);
        // else return raw::request(index(), address(), s, t, Const);
    // }

    // template <class T>
    // Maybe<T> request(Scope &s, Type<T> t={}) &;
    // {
        // if constexpr(std::is_convertible_v<Value &, T>) return some<T>(*this);
        // else return raw::request(index(), address(), s, t, Lvalue);
    // }

    // template <class T>
    // Maybe<T> request(Scope &s, Type<T> t={}) &&;
    // {
        // if constexpr(std::is_convertible_v<Value &&, T>) return some<T>(std::move(*this));
        // else return raw::request(index(), address(), s, t, Rvalue);
    // }

    /**************************************************************************/

    // template <class T>
    // Maybe<T> request(Type<T> t={}) const & {Scope s; return request(s, t);}

    // template <class T>
    // Maybe<T> request(Type<T> t={}) & {Scope s; return request(s, t);}

    // template <class T>
    // Maybe<T> request(Type<T> t={}) && {Scope s; return std::move(*this).request(s, t);}

    /**************************************************************************/

    // template <class T>
    // T cast(Scope &s, Type<T> t={}) const & {
    //     if (auto p = from_ref(s, t)) return static_cast<T &&>(*p);
    //     throw std::move(s.set_error("invalid cast (rebind::Value const &)"));
    // }

    // template <class T>
    // T cast(Scope &s, Type<T> t={}) & {
    //     if (auto p = from_ref(s, t)) return static_cast<T &&>(*p);
    //     throw std::move(s.set_error("invalid cast (rebind::Value &)"));
    // }

    // template <class T>
    // T cast(Scope &s, Type<T> t={}) && {
    //     if (auto p = from_ref(s, t)) return static_cast<T &&>(*p);
    //     throw std::move(s.set_error("invalid cast (rebind::Value &&)"));
    // }

    // bool assign_if(Ref const &p) {return stat::assign_if::ok == raw::assign_if(index(), address(), p);}

    /**************************************************************************/

    // template <class T>
    // T cast(Type<T> t={}) const & {Scope s; return cast(s, t);}

    // template <class T>
    // T cast(Type<T> t={}) & {Scope s; return cast(s, t);}

    // template <class T>
    // T cast(Type<T> t={}) && {Scope s; return std::move(*this).cast(s, t);}

    /**************************************************************************/

    // bool request_to(Output &v) const & {return stat::request::ok == raw::request_to(v, index(), address(), Const);}
    // bool request_to(Output &v) & {return stat::request::ok == raw::request_to(v, index(), address(), Lvalue);}
    // bool request_to(Output &v) && {return stat::request::ok == raw::request_to(v, index(), address(), Rvalue);}

    /**************************************************************************/

    // template <class ...Args>
    // Value operator()(Args &&...args) const;

    // bool call_to(Value &, ArgView) const;
};

/******************************************************************************/

template <class T, class ...Args, std::enable_if_t<is_usable<T>, int>>
Value::Value(Type<T> t, Args&& ...args) {
    if constexpr((sizeof(T) <= sizeof(Storage)) && (alignof(Storage) % alignof(T) == 0)) {
        raw::alloc_to<T>(&storage, static_cast<Args &&>(args)...);
        idx = rebind_tag_index(Index::of<T>().fptr,
            is_trivially_relocatable_v<T> ? (std::is_trivially_copyable_v<T> ? Loc::Trivial : Loc::Relocatable) : Loc::Stack);
    } else {
        storage.pointer = raw::alloc<T>(static_cast<Args &&>(args)...);
        idx = rebind_tag_index(Index::of<T>().fptr, Loc::Heap);
    }
}

inline Value::Value(Value&& v) noexcept : idx(v.idx), storage(v.storage) {
    if (location() == Loc::Stack) {
        index().call<stat::relocate>(tag::relocate, {}, &storage, {}, &v.storage);
    }
    v.release();
}

inline Value& Value::operator=(Value&& v) noexcept {
    reset();
    idx = v.idx;
    if (location() == Loc::Stack) {
        index().call<stat::relocate>(tag::relocate, {}, &storage, {}, &v.storage);
    } else {
        storage = v.storage;
    }
    v.release();
    return *this;
}

/******************************************************************************/

struct Copyable : Value {
    using Value::Value;
    Copyable(Copyable &&) noexcept = default;
    Copyable &operator=(Copyable &&v) noexcept = default;

    Copyable(Copyable const &v) {*this = v;}

    Copyable &operator=(Copyable const &v) {
        switch (location()) {
            case Loc::Trivial: {storage = v.storage; break;}
            case Loc::Relocatable: {auto t = &storage.data; break;}
            case Loc::Stack: {auto t = &storage.data; break;}
            case Loc::Heap: {auto t = storage.pointer; break;}
        }
        idx = v.idx;
        return *this;
    }

    Copyable clone() const {return *this;}
};

/******************************************************************************/

// struct {
    // Value(Value const &v) {
    //     if (stat::copy::ok != raw::copy(*this, v.index(), v.address())) throw std::runtime_error("no copy");
    // }

    // Value &operator=(Value const &v) {
    //     if (stat::copy::ok != raw::copy(*this, v.index(), v.address())) throw std::runtime_error("no copy");
    //     return *this;
    // }
// }

/******************************************************************************/

}