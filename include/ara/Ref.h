#pragma once
#include "Parts.h"
#include "Impl.h"
#include <stdexcept>

namespace ara {

/******************************************************************************************/

union Ref {
    ara_ref c;

    Ref() noexcept : c{nullptr, nullptr} {}
    Ref(std::nullptr_t) noexcept : Ref() {}

    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    explicit Ref(T &&t) noexcept
        : Ref(Index::of<unqualified<T>>(),
          std::is_const_v<std::remove_reference_t<T>> ? Mode::Read : Mode::Write,
          Pointer::from(const_cast<void*>(static_cast<void const*>(std::addressof(t))))) {}

    Ref(Tagged<Mode> i, Pointer p) noexcept : c{i.base, p.base} {}
    Ref(Index i, Mode t, Pointer p) noexcept : Ref{{i, t}, p} {}

    Ref(Ref &&r) noexcept : c{std::exchange(r.c.mode_index, nullptr), r.c.pointer} {}

    Ref &operator=(Ref &&r) noexcept {
        c.mode_index = std::exchange(r.c.mode_index, nullptr);
        c.pointer = r.c.pointer;
        return *this;
    }

    Ref(Ref const &) = delete;
    Ref &operator=(Ref const &) = delete;

    Ref copy() const {
        switch (mode()) {
            case Mode::Read: return Ref(index(), mode(), pointer());
            case Mode::Write: return Ref(index(), mode(), pointer());
            default: throw std::runtime_error("not copyable");
        }
    }

    ~Ref() noexcept {destroy_if_managed();}

    /**********************************************************************************/

    Index index() const noexcept {return ara_get_index(c.mode_index);}
    Mode mode() const noexcept {return static_cast<Mode>(ara_get_mode(c.mode_index));}
    Pointer pointer() const noexcept {return bit_cast<Pointer>(c.pointer);}

    constexpr bool has_value() const noexcept {return c.mode_index;}
    explicit constexpr operator bool() const noexcept {return has_value();}

    std::string_view name() const noexcept {return index().name();}

    void destroy_if_managed() {
        if (!has_value()) return;
        switch (mode()) {
            case Mode::Stack: {Destruct::invoke(index(), pointer()); c.mode_index = nullptr; return;}
            case Mode::Heap:  {Deallocate::invoke(index(), pointer()); c.mode_index = nullptr; return;}
            default: {return;}
        }
    }

    /**********************************************************************************/

    template <class T>
    bool binds_to(Qualifier q) const;

    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> *target() const {
        return binds_to<unqualified<T>>(qualifier_of<T>) ? pointer().address<std::remove_reference_t<T>>() : nullptr;
    }

    template <class T, bool Check=true, int N=0, class ...Ts>
    decltype(auto) call(Caller c, Ts &&...ts) const {
        DUMP("Ref::call:", type_name<T>(), "(", sizeof...(Ts), ")");
        return Output<T, Check>()([&](Target &t) {
            return parts::with_args<N>([&](auto &args) {
                return Method::invoke(index(), t, pointer(), Mode::Write, args);
            }, c, static_cast<Ts &&>(ts)...);
        });
    }

    // template <class T, int N=0, class ...Ts>
    // maybe<T> attempt(Caller c, Ts &&...ts) const {
    //     if (!has_value()) return Maybe<T>::none();
    //     return parts::attempt<T, N>(index(), pointer(), Mode::Read, c, static_cast<Ts &&>(ts)...);
    // }

    /**************************************************************************************/

    template <class T, std::enable_if_t<!std::is_reference_v<T>, int> = 0>
    std::optional<T> get(Type<T> t={});

    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T>* get(Type<T> t={}) const;

    template <class T>
    bool get(T &t);

    template <class T, std::enable_if_t<!std::is_reference_v<T>, int> = 0>
    T cast(Type<T> t={});

    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    T cast(Type<T> t={}) const;

    /**************************************************************************************/

    Load::stat get_to(Target &t) noexcept;
};

/******************************************************************************************/

template <>
struct is_trivially_relocatable<Ref> : std::true_type {};

template <class T>
bool Ref::binds_to(Qualifier q) const {
    if (!index().equals<T>()) return false;
    if (q == Qualifier::C) return true;
    switch (mode()) {
        case Mode::Read: return false;
        case Mode::Write: return q == Qualifier::L;
        default: return q == Qualifier::R;
    }
}

inline Load::stat dump_or_load(Target &target, Index source, Pointer p, Mode t) noexcept {
    switch (Dump::invoke(source, target, p, t)) {
        case Dump::Write: {DUMP("dump succeeded"); return Load::Write;}
        case Dump::Read: {DUMP("dump succeeded"); return Load::Read;}
        case Dump::Heap: {DUMP("dump succeeded"); return Load::Heap;}
        case Dump::Stack: {DUMP("dump succeeded"); return Load::Stack;}
        case Dump::OutOfMemory: {return Load::OutOfMemory;}
        case Dump::Exception: {return Load::Exception;}
        case Dump::None: {
            auto target_index = std::exchange(target.c.index, source);
            DUMP("try backup load");
            return Load::invoke(target_index, target, p, t);
        }
    }
}

inline Load::stat Ref::get_to(Target& target) noexcept {
    DUMP("get_to", has_value(), name(), target.name(), pointer().base);
    if (!has_value()) return Load::None;
    return dump_or_load(target, index(), pointer(), mode());
}

template <class T, std::enable_if_t<!std::is_reference_v<T>, int>>
std::optional<T> Ref::get(Type<T>) {
    std::optional<T> out;
    if (!has_value()) {
        DUMP("no value");
        // nothing
    } else if (index().equals<T>()) {
        DUMP("load exact match");
        switch (mode()) {
            case Mode::Stack: {if constexpr(std::is_constructible_v<T, T&&>) {out.emplace(pointer().load<T &&>());} break;}
            case Mode::Heap:  {if constexpr(std::is_constructible_v<T, T&&>) {out.emplace(pointer().load<T &&>());} break;}
            case Mode::Read:  {if constexpr(is_copy_constructible_v<T>) out.emplace(pointer().load<T const &>()); break;}
            case Mode::Write: {if constexpr(is_copy_constructible_v<T>) out.emplace(pointer().load<T &>()); break;}
        }
    } else {
        storage_like<T> storage;
        Target target(Index::of<T>(), &storage, sizeof(storage), Target::constraint<T>);
        switch (get_to(target)) {
            case Load::Stack: {
                DUMP("load succeeded");
                Destruct::RAII<T> raii{storage_cast<T>(storage)};
                out.emplace(std::move(raii.held));
                break;
            }
            case Load::Heap: {
                DUMP("load succeeded");
                Deallocate::RAII<T> raii{*static_cast<T *>(target.output())};
                out.emplace(std::move(raii.held));
                break;
            }
            case Load::Exception: {target.rethrow_exception();}
            case Load::OutOfMemory: {throw std::bad_alloc();}
            default: {}
        }
    }
    return out;
}


template <class T>
bool Ref::get(T& t) {
    if (!has_value()) {
        DUMP("no value");
        return false;
        // nothing
    }
    if (index().equals<T>()) {
        DUMP("load exact match");
        switch (mode()) {
            case Mode::Read:  {
                if constexpr(std::is_copy_assignable_v<T>) {t = pointer().load<T const &>(); return true;}
                else return false;
            }
            case Mode::Write: {
                if constexpr(std::is_copy_assignable_v<T>) {t = pointer().load<T &>(); return true;}
                else return false;
            }
            default: {
                if constexpr(std::is_move_assignable_v<T>) {t = pointer().load<T &&>(); return true;}
                else return false;
            }
        }
    } else {
        Target target(Index::of<T>(), std::addressof(t), sizeof(T), Target::Existing);
        switch (get_to(target)) {
            case Load::Write: {
                DUMP("load succeeded");
                c.mode_index = target.c.index;
                return true;
            }
            case Load::Exception: {target.rethrow_exception();}
            case Load::OutOfMemory: {throw std::bad_alloc();}
            default: {}
        }
    }
}


template <class T, std::enable_if_t<std::is_reference_v<T>, int>>
std::remove_reference_t<T>* Ref::get(Type<T>) const {
    DUMP("load reference", type_name<T>(), name());
    if (auto t = target<T>()) return t;
    return nullptr;
}

/******************************************************************************************/

}
