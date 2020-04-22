#pragma once
#include "Parts.h"
#include "Impl.h"
// #include "Scope.h"

namespace ara {

/******************************************************************************************/

struct Ref {
    Tagged<Tag> idx;
    Pointer ptr;

    constexpr Ref() noexcept = default;

    constexpr Ref(std::nullptr_t) noexcept : Ref() {}

    Ref(Tagged<Tag> i, void *p) noexcept : idx(i), ptr(p) {}
    Ref(Index i, Tag t, void *p) noexcept : Ref({i, t}, p) {}

    template <class T>
    explicit Ref(T &t) noexcept : idx(Index::of<T>(), Tag::Mutable), ptr(std::addressof(t)) {}

    template <class T>
    explicit Ref(T const &t) noexcept : idx(Index::of<T>(), Tag::Const), ptr(std::addressof(const_cast<T &>(t))) {}

    /**************************************************************************************/

    Ref(Ref &&r) noexcept : idx(r.idx), ptr(r.ptr) {r.idx.reset();}

    Ref &operator=(Ref &&r) noexcept {
        idx = r.idx;
        ptr = r.ptr;
        r.idx.reset();
        return *this;
    }

    Ref(Ref const &) = delete;
    Ref &operator=(Ref const &) = delete;

    ~Ref() {
        if (!has_value()) return;
        switch (tag()) {
            case Tag::Stack: {Destruct::call(index(), ptr, Destruct::Stack); return;}
            case Tag::Heap:  {Destruct::call(index(), ptr, Destruct::Heap); return;}
            default: {return;}
        }
    }

    /**********************************************************************************/

    Tag tag() const noexcept {return static_cast<Tag>(idx.tag());}

    constexpr bool has_value() const noexcept {return idx.has_value();}

    explicit constexpr operator bool() const noexcept {return has_value();}

    Index index() const noexcept {return Index(idx);}

    std::string_view name() const noexcept {return index().name();}

    void reset() noexcept {this->~Ref(); idx.reset();}

    /**********************************************************************************/

    template <class T>
    bool binds_to(Qualifier q) const;

    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> *target() const {
        return binds_to<unqualified<T>>(qualifier_of<T>) ? ptr.address<std::remove_reference_t<T>>() : nullptr;
    }

    template <class T, int N=0, class ...Ts>
    T call(Caller c, Ts &&...ts) const {
        DUMP("Ref::call:", type_name<T>(), "(", sizeof...(Ts), ")");
        return parts::call<T, N>(index(), tag(), ptr.base, c, static_cast<Ts &&>(ts)...);
    }

    template <class T, int N=0, class ...Ts>
    maybe<T> get(Caller c, Ts &&...ts) const {
        if (!has_value()) return Maybe<T>::none();
        return parts::get<T, N>(index(), Tag::Const, ptr.base, c, static_cast<Ts &&>(ts)...);
    }

    /**************************************************************************************/

    template <class T, std::enable_if_t<!std::is_reference_v<T>, int> = 0>
    std::optional<T> load(Scope &s, Type<T> t={});

    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> * load(Scope &s, Type<T> t={}) const;

    template <class T, std::enable_if_t<!std::is_reference_v<T>, int> = 0>
    T cast(Type<T> t={});

    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    T cast(Type<T> t={}) const;

    /**************************************************************************************/

    bool load_to(Target &t, Scope &s) {return false;}
};

/******************************************************************************************/

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

template <class T, std::enable_if_t<!std::is_reference_v<T>, int>>
std::optional<T> Ref::load(Scope &s, Type<T> t) {
    std::optional<T> out;
    if (!has_value()) {
        DUMP("no value");
        // nothing
    } else if (index().equals<T>()) {
        DUMP("load exact match");
        switch (tag()) {
            case Tag::Stack:   {if constexpr(std::is_constructible_v<T, T&&>) {out.emplace(ptr.load<T &&>()); reset();} break;}
            case Tag::Heap:    {if constexpr(std::is_constructible_v<T, T&&>) {out.emplace(ptr.load<T &&>()); reset();} break;}
            case Tag::Const:   {if constexpr(is_copy_constructible_v<T>) out.emplace(ptr.load<T const &>()); break;}
            case Tag::Mutable: {if constexpr(is_copy_constructible_v<T>) out.emplace(ptr.load<T &>()); break;}
        }
    } else {
        storage_like<T> storage;
        Target target{Index::of<T>(), &storage, sizeof(storage), Target::Stack};

        switch (Dump::call(index(), target, ptr, tag())) {
            case Dump::OK: {
                auto &t = storage_cast<T>(storage);
                out.emplace(std::move(t));
                t.~T();
                break;
            }
            case Dump::OutOfMemory: {break;}
            case Dump::Exception: {break;}
            case Dump::None: {
                if constexpr(is_complete_v<Loadable<T>>) {
                    DUMP("try compile time load ", type_name<T>());
                    out = Loadable<T>()(*this, s);
                } else {
                    DUMP("trying indirect load");
                    switch (Load::call(index(), target, ptr, tag())) {
                        case Load::Stack: {
                            auto &t = storage_cast<T>(storage);
                            out.emplace(std::move(t));
                            Destruct::impl<T>::put(t, Destruct::Stack);
                            break;
                        }
                        case Load::Heap: {
                            auto &t = storage_cast<T>(storage);
                            out.emplace(std::move(t));
                            Destruct::impl<T>::put(t, Destruct::Heap);
                            break;
                        }
                        case Load::None: {break;}
                    }
                }
            }
        }
    }
    return out;
}

template <class T, std::enable_if_t<std::is_reference_v<T>, int>>
std::remove_reference_t<T> * Ref::load(Scope &s, Type<T> t) const {
    DUMP("load reference ", type_name<T>(), " ", name());
    if (auto t = target<T>()) return t;
    return nullptr;
}

/******************************************************************************************/

template <class Mod>
struct Module {
    static void init() {
        Ref ref(fetch<Mod>(), Tag::Const, nullptr);
        ref.call<void>(Caller());
    }

    template <class T, int N=1, class ...Ts>
    static T call(std::string_view name, Caller caller, Ts&& ...ts) {
        return parts::call<T, N>(fetch<Mod>(), Tag::Const, nullptr, caller, name, std::forward<Ts>(ts)...);
    }

    template <class T, int N=1, class ...Ts>
    static T get(std::string_view name, Caller caller, Ts&& ...ts) {
        return parts::get<T, N>(fetch<Mod>(), Tag::Const, nullptr, caller, name, std::forward<Ts>(ts)...);
    }
};

/******************************************************************************************/

}
