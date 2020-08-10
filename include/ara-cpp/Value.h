#pragma once
#include <ara/Ref.h>
#include <ara/Call.h>
#include "Traits.h"

namespace ara {

/******************************************************************************/

struct Copyable;

struct Value;

template <class T>
static constexpr bool is_manageable = is_implementable<T> && !std::is_same_v<T, Value>;

/******************************************************************************/

struct Base {
    enum Loc : ara_mode {Trivial, Stack, Heap};

    Tagged<Loc> idx;

    void release() noexcept {idx.reset();}
    Loc location() const noexcept {return static_cast<Loc>(idx.tag());}
    Index index() const noexcept {return Index(idx);}
    std::string_view name() const noexcept {return index().name();}
    constexpr bool has_value() const noexcept {return idx.has_value();}
    explicit constexpr operator bool() const noexcept {return has_value();}
};

/******************************************************************************/

struct Value : Base {
    union Storage {
        void *pointer;
        char data[24];
    };

    template <class T>
    static constexpr Loc loc_of = is_stackable<T>(sizeof(Storage))
        && std::is_nothrow_move_constructible_v<T> ? (std::is_trivially_copyable_v<T> ? Trivial : Stack): Heap;

    Storage storage;

    /**************************************************************************/

    Value() noexcept = default;

    Value(std::nullptr_t) noexcept : Value() {}

    template <class T, class ...Args, std::enable_if_t<is_manageable<T>, int> = 0>
    Value(Type<T> t, Args &&...args);

    template <class T, std::enable_if_t<is_manageable<unqualified<T>>, int> = 0>
    Value(T &&t) : Value{Type<unqualified<T>>(), static_cast<T &&>(t)} {}

    Value(Value &&v) noexcept;
    Value(Value const &v) = delete;

    Value &operator=(Value &&v) noexcept;
    Value &operator=(Value const &v);// = delete;

    ~Value() {destruct();}

    /**************************************************************************/

    bool destruct() noexcept;
    void reset() noexcept {if (destruct()) release();}

    /**************************************************************************/

    template <class T, class ...Args>
    T & emplace(Type<T>, Args &&...args);

    template <class T, std::enable_if_t<!is_type<T>, int> = 0>
    unqualified<T> & emplace(T &&t) {
        return emplace(Type<unqualified<T>>(), static_cast<T &&>(t));
    }

    /**************************************************************************/

    template <class T>
    T const *target(Type<T> = {}) const {
        if (!index().equals<T>()) return nullptr;
        else if (location() == Loc::Heap) return static_cast<T const *>(storage.pointer);
        else return &storage_cast<T const>(storage);
    }

    template <class T>
    T *target(Type<T> t={}) {return const_cast<T *>(std::as_const(*this).target(t));}

    Pointer address() const {
        if (!has_value()) return Pointer::from(nullptr);
        else if (location() == Heap) return Pointer::from(storage.pointer);
        else return Pointer::from(const_cast<void *>(static_cast<void const *>(&storage)));
    }

    /**************************************************************************/

    std::optional<Copyable> clone() const;

    Ref as_ref() const & noexcept {return *this ? Ref(index(), Mode::Read, address()) : Ref();}
    Ref as_ref() & noexcept {return *this ? Ref(index(), Mode::Write, address()) : Ref();}

    template <class T>
    std::optional<T> get(Type<T> t={}) const {return as_ref().get(t);}

    template <class T=Value, bool Check=true, int N=0, class ...Ts>
    decltype(auto) method(Ts &&...ts) const {
        return Output<T, Check>()([&](Target &t) {
            return parts::with_args<N>([&](auto &args) {
                return Method::invoke(index(), t, address(), Mode::Read, args);
            }, static_cast<Ts &&>(ts)...);
        });
    }

    template <class T=Value, bool Check=true, int N=0, class ...Ts>
    decltype(auto) mutate(Ts &&...ts) {
        return Output<T, Check>()([&](Target &t) {
            return parts::with_args<N>([&](auto &args) {
                return Method::invoke(index(), t, address(), Mode::Write, args);
            }, static_cast<Ts &&>(ts)...);
        });
    }

    template <class T=Value, bool Check=true, int N=0, class ...Ts>
    decltype(auto) move(Ts &&...ts) {
        Ref ref(index(), location() == Heap ? Mode::Heap : Mode::Stack, address());
        release();
        return Output<T, Check>()([&](Target &t) {
            return parts::with_args<N>([&](auto &args) {
                return Method::invoke(ref.index(), t, ref.pointer(), ref.mode(), args);
            }, static_cast<Ts &&>(ts)...);
        });
    }

    // template <class T=Value, int N=0, class ...Ts>
    // maybe<T> attempt(Ts &&...ts) const {
    //     if (!has_value()) return Maybe<T>::none();
    //     return parts::attempt<T, N>(index(), Mode::Read, address(), c, static_cast<Ts &&>(ts)...);
    // }

    auto get_to(Target &t) const {return as_ref().get_to(t);}

    // bool assign_if(Ref const &p) {return stat::assign_if::ok == parts::assign_if(index(), address(), p);}

    /**************************************************************************/

    // template <class T>
    // T cast(Type<T> t={}) const & {Scope s; return cast(s, t);}

    // template <class T>
    // T cast(Type<T> t={}) & {Scope s; return cast(s, t);}

    // template <class T>
    // T cast(Type<T> t={}) && {Scope s; return std::move(*this).cast(s, t);}

    /**************************************************************************/
};

template <>
struct Maybe<Value> {
    using type = Value;
    static constexpr auto none() {return nullptr;}
};

/******************************************************************************/

template <class T, class ...Args, std::enable_if_t<is_manageable<T>, int>>
Value::Value(Type<T>, Args&& ...args) {
    static_assert(std::is_constructible_v<T, Args &&...>);
    if constexpr(loc_of<T> == Loc::Heap) {
        storage.pointer = Allocator<T>::heap(static_cast<Args &&>(args)...);
    } else {
        Allocator<T>::stack(&storage.data, static_cast<Args &&>(args)...);
    }
    idx = Tagged(Index::of<T>(), loc_of<T>);
    // DUMP("construct! ", idx, index().name());
}

/******************************************************************************/

template <class T, class ...Args>
T & Value::emplace(Type<T>, Args &&...args) {
    assert_implementable<T>();
    reset();
    T *out;
    if constexpr(loc_of<T> == Loc::Heap) {
        storage.pointer = out = Allocator<T>::heap(static_cast<Args &&>(args)...);
    } else {
        out = Allocator<T>::stack(&storage.data, static_cast<Args &&>(args)...);
    }
    idx = Tagged(Index::of<T>(), loc_of<T>);
    // DUMP("emplace! ", idx, index().name(), Index::of<T>().name());
    return *out;
}

/******************************************************************************/

inline Value::Value(Value&& v) noexcept : Base{v.idx}, storage(v.storage) {
    if (location() == Loc::Stack) Relocate::invoke(index(), &storage.data, &v.storage.data);
    v.release();
}

inline Value& Value::operator=(Value&& v) noexcept {
    reset();
    idx = v.idx;
    if (location() == Loc::Stack) {
        Relocate::invoke(index(), &storage.data, &v.storage.data);
    } else {
        storage = v.storage;
    }
    v.release();
    return *this;
}

inline bool Value::destruct() noexcept {
    if (!has_value()) return false;
    switch (location()) {
        case Loc::Trivial: break;
        case Loc::Heap: {Deallocate::invoke(index(), Pointer::from(storage.pointer)); break;}
        default: {Destruct::invoke(index(), Pointer::from(&storage)); break;}
    }
    return true;
}

/******************************************************************************/

template <>
struct Output<Value, true> {
    template <class F>
    Value operator()(F &&f) const {
        DUMP("calling to get Value");
        Value out;
        Target target(Index(), &out.storage, sizeof(out.storage),
            Target::Trivial | Target::Relocatable | Target::MoveNoThrow | Target::Heap);
        auto const stat = f(target);
        DUMP("called the output!", stat);

        switch (stat) {
            case Method::None: {break;}
            case Method::Stack: {
                out.idx = Tagged(target.index(), Value::Stack);
                break;
            }
            case Method::Heap: {
                out.storage.pointer = target.output();
                out.idx = Tagged(target.index(), Value::Heap);
                break;
            }
            default: call_throw(std::move(target), stat);
        }
        return out;
    }
};

template <>
struct Output<Value, false> {
    template <class F>
    Value operator()(F &&f) const {
        DUMP("calling to get Value");
        Value out;
        Target target(Index(), &out.storage, sizeof(out.storage),
            Target::Trivial | Target::Relocatable | Target::MoveNoThrow | Target::Heap);
        auto const stat = f(target);
        DUMP("called the output!", stat);

        switch (stat) {
            case Method::None: {break;}
            case Method::Stack: {
                out.idx = Tagged(target.index(), Value::Stack);
                break;
            }
            case Method::Heap: {
                out.storage.pointer = target.output();
                out.idx = Tagged(target.index(), Value::Heap);
                break;
            }
            case Method::Impossible: {break;}
            case Method::WrongNumber: {break;}
            case Method::WrongType: {break;}
            case Method::WrongReturn: {break;}
            default: call_throw(std::move(target), stat);
        }
        return out;
    }
};

// struct Copyable : Value {
//     using Value::Value;
//     Copyable(Copyable &&) noexcept = default;
//     Copyable &operator=(Copyable &&v) noexcept = default;

//     Copyable(Copyable const &v) {*this = v;}

//     Copyable &operator=(Copyable const &v) {
//         switch (location()) {
//             case Loc::Trivial: {storage = v.storage; break;}
//             case Loc::Relocatable: {auto t = &storage.data; break;}
//             case Loc::Stack: {auto t = &storage.data; break;}
//             case Loc::Heap: {auto t = storage.pointer; break;}
//         }
//         idx = v.idx;
//         return *this;
//     }

//     Copyable clone() const {return *this;}
// };

    // Value(Value const &v) {
    //     if (stat::copy::ok != parts::copy(*this, v.index(), v.address())) throw std::runtime_error("no copy");
    // }

    // Value &operator=(Value const &v) {
    //     if (stat::copy::ok != parts::copy(*this, v.index(), v.address())) throw std::runtime_error("no copy");
    //     return *this;
    // }

/******************************************************************************/

}
