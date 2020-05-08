#pragma once
#include <ara/Ref.h>
#include <ara/Call.h>

namespace ara {

/******************************************************************************/

struct Copyable;

struct Value;

template <class T>
static constexpr bool is_manageable = is_usable<T> && !std::is_same_v<T, Value>;

// parts::alloc_to<T>(&storage, static_cast<Args &&>(args)...);
        // if (std::is_trivially_copyable_v<T>) loc = Loc::Trivial;
        // else if (is_trivially_relocatable_v<T>) loc = Loc::Relocatable;
        // else loc = Loc::Stack;

/******************************************************************************/

struct Value {
    union Storage {
        void *pointer;
        char data[24];
    };

    enum Loc : ara_tag {Trivial, Stack, Heap};

    template <class T>
    static constexpr Loc loc_of = is_stackable<T>(sizeof(Storage))
        && std::is_nothrow_move_constructible_v<T> ? (std::is_trivially_copyable_v<T> ? Trivial : Stack): Heap;

    Tagged<Loc> idx;
    Storage storage;

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

    void release() noexcept {idx.reset();}

    Loc location() const noexcept {return static_cast<Loc>(idx.tag());}

    Index index() const noexcept {return Index(idx);}

    std::string_view name() const noexcept {return index().name();}

    constexpr bool has_value() const noexcept {return idx.has_value();}

    explicit constexpr operator bool() const noexcept {return has_value();}

    /**************************************************************************/

    template <class T, class ...Args>
    T & emplace(Type<T>, Args &&...args);

    template <class T, std::enable_if_t<!is_type<T>, int> = 0>
    unqualified<T> & emplace(T &&t) {
        return emplace(Type<unqualified<T>>(), static_cast<T &&>(t));
    }

    /**************************************************************************/

    std::optional<Copyable> clone() const;

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

    Ref as_ref() const & noexcept {return *this ? Ref::from_existing(index(), address(), false) : Ref();}
    Ref as_ref() & noexcept {return *this ? Ref::from_existing(index(), address(), true) : Ref();}

    template <class T>
    std::optional<T> load(Type<T> t={}) const {return as_ref().load(t);}

    /**************************************************************************/

    template <class T=Value, int N=0, class ...Ts>
    T call(Caller c, Ts &&...ts) const {
        return parts::call<T, N>(index(), Tag::Const, address(), c, static_cast<Ts &&>(ts)...);
    }

    template <class T=Value, int N=0, class ...Ts>
    T mutate(Caller c, Ts &&...ts) {
        return parts::call<T, N>(index(), Tag::Mutable, address(), c, static_cast<Ts &&>(ts)...);
    }

    template <class T=Value, int N=0, class ...Ts>
    T move(Caller c, Ts &&...ts) {
        Reference ref(index(), location() == Heap ? Tag::Heap : Tag::Stack, address());
        release();
        return parts::call<T, N>(ref.index(), ref.tag(), ref.pointer(), c, static_cast<Ts &&>(ts)...);
    }

    template <class T=Value, int N=0, class ...Ts>
    maybe<T> get(Caller c, Ts &&...ts) const {
        if (!has_value()) return Maybe<T>::none();
        return parts::get<T, N>(index(), Tag::Const, address(), c, static_cast<Ts &&>(ts)...);
    }

    // bool assign_if(Ref const &p) {return stat::assign_if::ok == parts::assign_if(index(), address(), p);}

    /**************************************************************************/

    // template <class T>
    // T cast(Type<T> t={}) const & {Scope s; return cast(s, t);}

    // template <class T>
    // T cast(Type<T> t={}) & {Scope s; return cast(s, t);}

    // template <class T>
    // T cast(Type<T> t={}) && {Scope s; return std::move(*this).cast(s, t);}

    /**************************************************************************/

    auto load_to(Target &t) const {return as_ref().load_to(t);}

    /**************************************************************************/

    // template <class ...Args>
    // Value operator()(Args &&...args) const;
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
        storage.pointer = parts::alloc<T>(static_cast<Args &&>(args)...);
    } else {
        parts::alloc_to<T>(&storage.data, static_cast<Args &&>(args)...);
    }
    idx = Tagged(Index::of<T>(), loc_of<T>);
    // DUMP("construct! ", idx, index().name());
}

/******************************************************************************/

template <class T, class ...Args>
T & Value::emplace(Type<T>, Args &&...args) {
    assert_usable<T>();
    reset();
    T *out;
    if constexpr(loc_of<T> == Loc::Heap) {
        storage.pointer = out = parts::alloc<T>(static_cast<Args &&>(args)...);
    } else {
        out = parts::alloc_to<T>(&storage.data, static_cast<Args &&>(args)...);
    }
    idx = Tagged(Index::of<T>(), loc_of<T>);
    // DUMP("emplace! ", idx, index().name(), Index::of<T>().name());
    return *out;
}

/******************************************************************************/

inline Value::Value(Value&& v) noexcept : idx(v.idx), storage(v.storage) {
    if (location() == Loc::Stack) Relocate::call(index(), &storage.data, &v.storage.data);
    v.release();
}

inline Value& Value::operator=(Value&& v) noexcept {
    reset();
    idx = v.idx;
    if (location() == Loc::Stack) {
        Relocate::call(index(), &storage.data, &v.storage.data);
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
        case Loc::Heap: {Destruct::call(index(), Pointer::from(storage.pointer), Destruct::Heap); break;}
        default: {Destruct::call(index(), Pointer::from(&storage), Destruct::Stack); break;}
    }
    return true;
}

/******************************************************************************/

template <>
struct CallReturn<Value> {
    static Value call(Index i, Tag qualifier, Pointer self, ArgView &args);

    static Value get(Index i, Tag qualifier, Pointer self, ArgView &args);
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


template <class Out, class Module>
Out global_call();

/******************************************************************************/

// struct {
    // Value(Value const &v) {
    //     if (stat::copy::ok != parts::copy(*this, v.index(), v.address())) throw std::runtime_error("no copy");
    // }

    // Value &operator=(Value const &v) {
    //     if (stat::copy::ok != parts::copy(*this, v.index(), v.address())) throw std::runtime_error("no copy");
    //     return *this;
    // }
// }

/******************************************************************************/

}