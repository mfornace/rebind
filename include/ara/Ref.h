#pragma once
#include "Parts.h"
#include "Impl.h"

namespace ara {

/******************************************************************************************/

struct Ref {
    ara_ref c;

    static Ref from_existing(Index i, Pointer p, bool mutate) noexcept {
        return {Tagged<Tag>(i, mutate ? Tag::Mutable : Tag::Const).base, p.base};
    }

    /**********************************************************************************/

    Index index() const noexcept {return ara_get_index(c.tag_index);}
    Tag tag() const noexcept {return static_cast<Tag>(ara_get_tag(c.tag_index));}
    Pointer pointer() const noexcept {return bit_cast<Pointer>(c.pointer);}

    constexpr bool has_value() const noexcept {return c.tag_index;}
    explicit constexpr operator bool() const noexcept {return has_value();}

    std::string_view name() const noexcept {return index().name();}

    void reset() noexcept {this->~Ref(); c.tag_index = nullptr;}

    /**********************************************************************************/

    template <class T>
    bool binds_to(Qualifier q) const;

    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> *target() const {
        return binds_to<unqualified<T>>(qualifier_of<T>) ? pointer().address<std::remove_reference_t<T>>() : nullptr;
    }

    template <class T, int N=0, class ...Ts>
    T call(Caller c, Ts &&...ts) const {
        DUMP("Ref::call:", type_name<T>(), "(", sizeof...(Ts), ")");
        return parts::call<T, N>(index(), tag(), pointer().base, c, static_cast<Ts &&>(ts)...);
    }

    template <class T, int N=0, class ...Ts>
    maybe<T> get(Caller c, Ts &&...ts) const {
        if (!has_value()) return Maybe<T>::none();
        return parts::get<T, N>(index(), Tag::Const, pointer().base, c, static_cast<Ts &&>(ts)...);
    }

    /**************************************************************************************/

    template <class T, std::enable_if_t<!std::is_reference_v<T>, int> = 0>
    std::optional<T> load(Type<T> t={});

    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> * load(Type<T> t={}) const;

    template <class T, std::enable_if_t<!std::is_reference_v<T>, int> = 0>
    T cast(Type<T> t={});

    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    T cast(Type<T> t={}) const;

    /**************************************************************************************/

    Load::stat load_to(Target &t) noexcept;
};

static_assert(std::is_aggregate_v<Ref>);

/******************************************************************************************/

struct Reference : Ref {

    Reference() noexcept = default;
    Reference(std::nullptr_t) noexcept : Reference() {}

    Reference(Tagged<Tag> i, Pointer p) noexcept : Ref{i.base, p.base} {}
    Reference(Index i, Tag t, Pointer p) noexcept : Reference{{i, t}, p} {}

    template <class T>
    explicit Reference(T &t) noexcept : Reference(Index::of<T>(), Tag::Mutable, Pointer::from(std::addressof(t))) {}

    template <class T>
    explicit Reference(T const &t) noexcept : Reference(Index::of<T>(), Tag::Const, Pointer::from(std::addressof(const_cast<T &>(t)))) {}

    /**************************************************************************************/

    Reference(Reference &&r) noexcept : Ref{std::exchange(r.c.tag_index, nullptr), r.c.pointer} {}

    Reference &operator=(Reference &&r) noexcept {
        c.tag_index = std::exchange(r.c.tag_index, nullptr);
        c.pointer = r.c.pointer;
        return *this;
    }

    Reference(Reference const &) = delete;
    Reference &operator=(Reference const &) = delete;

    ~Reference() {
        if (!has_value()) return;
        switch (tag()) {
            case Tag::Stack: {Destruct::call(index(), pointer(), Destruct::Stack); return;}
            case Tag::Heap:  {Destruct::call(index(), pointer(), Destruct::Heap); return;}
            default: {return;}
        }
    }
};

/******************************************************************************************/

static_assert(std::is_standard_layout_v<Reference>);
static_assert(std::is_standard_layout_v<Ref>);

template <>
struct is_trivially_relocatable<Ref> : std::true_type {};


template <class T>
bool Ref::binds_to(Qualifier q) const {
    if (!index().equals<T>()) return false;
    if (q == Qualifier::C) return true;
    switch (tag()) {
        case Tag::Const: return false;
        case Tag::Mutable: return q == Qualifier::L;
        default: return q == Qualifier::R;
    }
}

inline Load::stat dump_or_load(Target &target, Index i, Pointer p, Tag t) noexcept {
    switch (Dump::call(i, target, p, t)) {
        case Dump::OK: {return Load::OK;}
        case Dump::OutOfMemory: {return Load::OutOfMemory;}
        case Dump::Exception: {return Load::Exception;}
        case Dump::None: {return Load::call(i, target, p, t);}
    }
}

inline Load::stat Ref::load_to(Target &target) noexcept {
    DUMP("load_to", has_value(), name(), pointer().base);
    if (!has_value()) return Load::None;
    return dump_or_load(target, index(), pointer(), tag());
}

template <class T, std::enable_if_t<!std::is_reference_v<T>, int>>
std::optional<T> Ref::load(Type<T> t) {
    std::optional<T> out;
    if (!has_value()) {
        DUMP("no value");
        // nothing
    } else if (index().equals<T>()) {
        DUMP("load exact match");
        switch (tag()) {
            case Tag::Stack:   {if constexpr(std::is_constructible_v<T, T&&>) {out.emplace(pointer().load<T &&>()); reset();} break;}
            case Tag::Heap:    {if constexpr(std::is_constructible_v<T, T&&>) {out.emplace(pointer().load<T &&>()); reset();} break;}
            case Tag::Const:   {if constexpr(is_copy_constructible_v<T>) out.emplace(pointer().load<T const &>()); break;}
            case Tag::Mutable: {if constexpr(is_copy_constructible_v<T>) out.emplace(pointer().load<T &>()); break;}
        }
    } else {
        storage_like<T> storage;
        auto target = Target::from(Index::of<T>(), &storage, sizeof(storage), Target::Stack);
        switch (load_to(target)) {
            case Load::OK: {
                DUMP("load succeeded");
                Destructor<T> raii{storage_cast<T>(storage)};
                out.emplace(std::move(raii.held));
                break;
            }
            case Load::None: {break;}
            case Load::Exception: {target.rethrow_exception();}
            case Load::OutOfMemory: {throw std::bad_alloc();}
        }
    }
    return out;
}

template <class T, std::enable_if_t<std::is_reference_v<T>, int>>
std::remove_reference_t<T> * Ref::load(Type<T> t) const {
    DUMP("load reference", type_name<T>(), name());
    if (auto t = target<T>()) return t;
    return nullptr;
}

/******************************************************************************************/

template <class Mod>
struct Module {
    static void init(Caller caller={}) {
        parts::call<void, 0>(fetch(Type<Mod>()), Tag::Const, Pointer::from(nullptr), caller);
    }

    template <class T, int N=1, class ...Ts>
    static T call(std::string_view name, Caller caller, Ts&& ...ts) {
        return parts::call<T, N>(fetch(Type<Mod>()), Tag::Const, Pointer::from(nullptr), caller, name, std::forward<Ts>(ts)...);
    }

    template <class T, int N=1, class ...Ts>
    static T get(std::string_view name, Caller caller, Ts&& ...ts) {
        return parts::get<T, N>(fetch(Type<Mod>()), Tag::Const, Pointer::from(nullptr), caller, name, std::forward<Ts>(ts)...);
    }
};

/******************************************************************************************/

}
