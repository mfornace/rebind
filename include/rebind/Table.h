#pragma once
#include "Type.h"
#include "Common.h"
#include <map>
#include <stdexcept>
#include <array>
#include <type_traits>

namespace rebind {

/******************************************************************************/

// class Function;
// class Overload;
class Ref;
class Value;
class Scope;
struct Arguments;

enum class Flag : unsigned char {ref, value};

template <class T>
static constexpr bool is_usable = true
    && !std::is_reference_v<T>
    // && !std::is_function_v<T>
    // && !std::is_void_v<T>
    && !std::is_const_v<T>
    && !std::is_volatile_v<T>
    && !std::is_void_v<T>
    && std::is_nothrow_move_constructible_v<T>
    && !std::is_same_v<T, Ref>
    && !std::is_same_v<T, Value>
    // && !std::is_null_pointer_v<T>
    && !is_type_t<T>::value;

/******************************************************************************/

template <class T>
constexpr void assert_usable() {
    static_assert(!std::is_reference_v<T>);
    static_assert(!std::is_const_v<T>);
    static_assert(!std::is_volatile_v<T>);
    static_assert(!std::is_void_v<T>);
    static_assert(std::is_nothrow_move_constructible_v<T>);
    static_assert(!std::is_same_v<T, Ref>);
    static_assert(!std::is_same_v<T, Value>);
    // static_assert(!std::is_null_pointer_v<T>);
    static_assert(!is_type_t<T>::value);
}

/******************************************************************************************/

struct CTable {
    using destroy_t = void (*)(void*) noexcept;
    using copy_t = void* (*)(void const *);
    using to_value_t = bool (*)(Value &, void *, Qualifier);
    using to_ref_t = bool (*)(Ref &, void *, Qualifier);
    using assign_if_t = bool (*)(void *, Ref const &);
    using from_ref_t = bool (*)(Value &, Ref const &, Scope &);
    using call_t = bool(*)(void const *, void *, Caller &&, Arguments, Flag);

    // Destructor function pointer
    destroy_t destroy = nullptr;
    // Copy function pointer
    copy_t copy = nullptr;
    // Callable function pointer
    call_t call = nullptr;
    // ToValue function pointer
    to_value_t to_value = nullptr;
    // ToValue function pointer
    to_ref_t to_ref = nullptr;
    // FromRef function pointer
    from_ref_t from_ref = nullptr;
    // FromRef function pointer
    assign_if_t assign_if = nullptr;
    // Optional C++ type_info
    std::type_info const *info = nullptr;
};

/******************************************************************************************/

struct Table {
    CTable c;

    // List of base classes that this type can be cast into
    Vector<Index> bases;

    // Output name
    std::array<std::string, 4> names;

    // List of properties on this class
    std::map<std::string, Value, std::less<>> properties;

    /**************************************************************************************/

    std::string const& name() const noexcept {return names[0];}

    std::string const& name(Qualifier q) const noexcept {return names[1 + q];}

    void destroy(void*p) const noexcept {
        if (!c.destroy) std::terminate();
        c.destroy(p);
    }

    bool has_base(Index const &idx) const noexcept {
        for (auto const &i : bases) if (idx == i) return true;
        return false;
    }

    void* copy(void const* p) const {
        if (c.copy) return c.copy(p);
        throw std::runtime_error("held type is not copyable: " + name());
    }
};

/******************************************************************************************/

/// Return new-allocated copy of the object
template <class T>
void* default_copy(void const* p) {
    if constexpr(std::is_copy_constructible_v<T>) return new T(*static_cast<T const*>(p));
    return nullptr;
}

/// Destroy the object
template <class T>
void default_destroy(void* p) noexcept {delete static_cast<T *>(p);}

template <class T>
bool default_to_value(Value &, void *, Qualifier const);

template <class T>
bool default_to_ref(Ref &, void *, Qualifier const);

template <class T>
bool default_from_ref(Value &, Ref const &, Scope &);

template <class T>
bool default_assign_if(void *, Ref const &);

/******************************************************************************************/

template <class T>
Table default_table() {
    Table t;
    t.c.info = &typeid(T);
    std::string name = typeid(T).name();
    t.names = {
        name,
        name + QualifierSuffixes[Const],
        name + QualifierSuffixes[Lvalue],
        name + QualifierSuffixes[Rvalue]
    };

    if constexpr(is_usable<T>) {
        t.c.copy = default_copy<T>;
        t.c.destroy = default_destroy<T>;
        t.c.to_value = default_to_value<T>;
        t.c.to_ref = default_to_ref<T>;
        t.c.from_ref = default_from_ref<T>;
        if constexpr(std::is_move_assignable_v<T>) {
            t.c.assign_if = default_assign_if<T>;
        }
    } else {
        t.c.copy = [](void const *) -> void * {throw std::runtime_error("bad");};
        t.c.to_value = [](Value &, void *, Qualifier const) -> bool {throw std::runtime_error("bad");};
        t.c.to_ref = [](Ref &, void *, Qualifier const) -> bool {throw std::runtime_error("bad");};
        t.c.from_ref = [](Value &, Ref const &, Scope &) -> bool {throw std::runtime_error("bad");};
        t.c.assign_if = [](void *, Ref const &) -> bool {throw std::runtime_error("bad");};
    }

    return t;
}

/******************************************************************************************/

template <class T>
struct TableImplementation {
    static inline Table table = default_table<T>();
};

template <class T>
Index fetch() noexcept {return {std::addressof(TableImplementation<T>::table)};}

/******************************************************************************************/

}
