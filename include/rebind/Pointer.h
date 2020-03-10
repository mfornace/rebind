#pragma once
#include "Table.h"
#include "Error.h"
#include <stdexcept>

namespace rebind {

/******************************************************************************************/

struct Opaque {
    Table tab;
    void *ptr = nullptr;

    Opaque() = default;

    constexpr Opaque(Table t, void *p) noexcept : tab(t), ptr(p) {}

    template <class T>
    explicit Opaque(T &t) : tab(get_table<T>()), ptr(static_cast<void *>(std::addressof(t))) {}

    template <class T>
    T & emplace(T &t) {tab = get_table<T>(); ptr = static_cast<void *>(std::addressof(t)); return t;}

    Table const &table() const {return tab;}

    void reset() {ptr = nullptr; tab = {};}

    void * value() const {return ptr;}

    bool has_value() const {return ptr;}
    explicit operator bool() const {return ptr;}

    template <class T>
    T *target() const {
        return (has_value() && tab == get_table<unqualified<T>>()) ? static_cast<T *>(ptr) : nullptr;
    }

    void try_destroy() const noexcept {if (has_value()) tab->destroy(ptr);}

    template <class T>
    std::remove_reference_t<T> * request_reference(Qualifier q) const {
        using T0 = std::remove_reference_t<T>;
        if (auto p = target<T0>()) return p;
        if (has_value() && tab->has_base(type_index<T0>())) return static_cast<T0 *>(ptr);
        return nullptr;
    }

    template <class T>
    std::optional<T> request_value(Scope &s, Qualifier q) const;

    TypeIndex index() const noexcept {return has_value() ? tab->index : TypeIndex();}

    std::string_view name() const noexcept {
        if (has_value()) return tab->name();
        else return "<null>";
    }

    Opaque allocate_copy() const {
        if (has_value()) return {tab, tab->copy(ptr)};
        else return {};
    }
};

/******************************************************************************************/

class Pointer : protected Opaque {
protected:
    Qualifier qual = Const;

public:
    using Opaque::name;
    using Opaque::index;
    using Opaque::has_value;
    using Opaque::table;
    using Opaque::reset;
    using Opaque::operator bool;

    constexpr Pointer() noexcept = default;

    constexpr Pointer(std::nullptr_t) noexcept : Pointer() {}

    template <class T>
    constexpr Pointer(T *t, Qualifier q) noexcept : Opaque(t), qual(q) {}

    // template <class T>
    // constexpr Pointer(T &&t) noexcept : Pointer(std::addressof(t), qualifier_of<T &&>) {}

    constexpr Pointer(Opaque o, Qualifier q) noexcept : Opaque(std::move(o)), qual(q) {}

    Qualifier qualifier() const noexcept {return qual;}

    /**************************************************************************************/

    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> * request(Scope &s, Type<T> t={}) const {
        assert_usable<unqualified<T>>();
        return Opaque::request_reference<unqualified<T>>(qual);
    }

    template <class T, std::enable_if_t<!std::is_reference_v<T>, int> = 0>
    std::optional<T> request(Scope &s, Type<T> t={}) const;

    template <class T>
    auto request(Type<T> t={}) const {Scope s; return request(s, t);}

    template <class T>
    T cast(Scope &s, Type<T> t={}) const {
        if (auto p = request(s, t)) return static_cast<T &&>(*p);
        throw std::move(s.set_error("invalid cast (rebind::Pointer)"));
    }

    template <class T>
    T cast(Type<T> t={}) const {Scope s; return cast(s, t);}

    /**************************************************************************************/

    // Value request_value(TypeIndex t, Scope s={}) const;

    // Pointer request_pointer(TypeIndex t, Scope s={}) const;

    std::string_view qualified_name() const noexcept {
        if (has_value()) return table()->name(qual);
        else return "<null>";
    }

    /**************************************************************************************/

    template <class T>
    static Pointer from(T &t) {return {std::addressof(t), Lvalue};}

    template <class T>
    static Pointer from(T const &t) {return {const_cast<T *>(std::addressof(t)), Const};}

    template <class T>
    static Pointer from(T &&t) {return {std::addressof(t), Rvalue};}

    static Pointer from(Value &t);
    static Pointer from(Value const &t);
    static Pointer from(Value &&t);
};

/******************************************************************************************/

using Arguments = Vector<Pointer>;

/******************************************************************************************/

template <class ...Ts>
Arguments to_arguments(Ts &&...ts) {
    Arguments out;
    out.reserve(sizeof...(Ts));
    (out.emplace_back(std::addressof(ts), qualifier_of<Ts &&>), ...);
    return out;
}

/******************************************************************************************/

}
