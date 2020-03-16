#pragma once
#include "Table.h"
#include "Error.h"

namespace rebind {

/******************************************************************************************/

struct Erased {
    // invariant; if ptr is non-null, Table is non-null
    void *ptr = nullptr;
    Table tab;

    Erased() = default;

    constexpr Erased(Table t, void *p) noexcept : tab(t), ptr(p) {}

    template <class T, std::enable_if_t<is_usable<T>, int> = 0>
    explicit Erased(T *t) : tab(get_table<T>()), ptr(static_cast<void *>(t)) {}

    // template <class T>
    // void set(T &t) {
    //     tab = get_table<T>();
    //     ptr = static_cast<void *>(std::addressof(t));
    //     return t;
    // }

    void reset() {ptr = nullptr; tab = {};}

    template <class T>
    T * reset(T *t) noexcept {ptr = t; tab = get_table<T>(); return t;}

    /**************************************************************************************/

    Table table() const {return tab;}

    void * value() const {return ptr;}

    template <class T>
    bool matches(Type<T> t={}) const {return tab == get_table<unqualified<T>>();}

    bool has_value() const {return ptr;}

    explicit operator bool() const {return ptr;}

    Index index() const noexcept {return tab ? tab->index : Index();}

    std::string_view name() const noexcept {
        if (tab) return tab->name();
        else return "<null>";
    }

    /**************************************************************************************/

    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> * request(Scope &s, Type<T> t, Qualifier q) const;

    template <class T, std::enable_if_t<!std::is_reference_v<T>, int> = 0>
    std::optional<T> request(Scope &s, Type<T> t, Qualifier q) const;

    bool request_to(Value &, Qualifier q) const;

    bool request_to(Pointer &, Qualifier q) const;

    /**************************************************************************************/

    template <class T>
    T *target() const {
        return (has_value() && tab == get_table<unqualified<T>>()) ? static_cast<T *>(ptr) : nullptr;
    }

    void try_destroy() const noexcept {if (has_value()) tab->destroy(ptr);}

    Erased allocate_copy() const {
        if (has_value()) return {tab, tab->copy(ptr)};
        else return {};
    }

    /**************************************************************************/

    Erased const &as_erased() const {return *this;}
};

/******************************************************************************************/

}