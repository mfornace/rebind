#pragma once
#include "Type.h"
#include "Common.h"
#include <map>
#include <stdexcept>
#include <array>
#include <type_traits>

namespace rebind {

/******************************************************************************/

class Function;
class Overload;
class Ref;
class Value;
class Scope;

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

struct Table {
    using destroy_t = void (*)(void*) noexcept;
    using copy_t = void* (*)(void const *);
    using to_value_t = bool (*)(Value &, void *, Qualifier);
    using to_ref_t = bool (*)(Ref &, void *, Qualifier);
    using assign_if_t = bool (*)(void *, Ref const &);
    using from_ref_t = bool (*)(Value &, Ref const &, Scope &);

    /**************************************************************************************/

    std::type_info const *info;
    // Destructor function pointer
    destroy_t m_destroy = nullptr;
    // Relocate function pointer
    bool (*m_relocate)(void *, void *, unsigned short, unsigned short) noexcept = nullptr; // either relocate the object to the void*, or ...
    // Copy function pointer
    copy_t m_copy = nullptr;
    // ToValue function pointer
    to_value_t m_to_value = nullptr;
    // ToValue function pointer
    to_ref_t m_to_ref = nullptr;
    // FromRef function pointer
    from_ref_t m_from_ref = nullptr;
    // FromRef function pointer
    assign_if_t m_assign_if = nullptr;
    // Output name
    std::string m_name;
    std::array<std::string, 3> qualified_names;
    // List of base classes that this type can be cast into
    Vector<Index> bases;
    // List of methods on this class
    std::map<std::string, Function, std::less<>> methods;

    /**************************************************************************************/

    std::string const& name() const noexcept {return m_name;}

    std::string const& name(Qualifier q) const noexcept {return qualified_names[q];}

    void destroy(void*p) const noexcept {
        if (!m_destroy) std::terminate();
        m_destroy(p);
    }

    bool has_base(Index const &idx) const noexcept {
        for (auto const &i : bases) if (idx == i) return true;
        return false;
    }

    void* copy(void const* p) const {
        if (m_copy) return m_copy(p);
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

/// Not sure yet
template <class T>
bool default_relocate(void*out, void*p, unsigned short size, unsigned short align) noexcept {
    if (sizeof(T) <= size && alignof(T) <= align) {
        new(out) T(std::move(*static_cast<T *>(p)));
        static_cast<T *>(p)->~T();
        return true;
    } else {
        *static_cast<T**>(out) = new T(std::move(*static_cast<T *>(p)));
        static_cast<T*>(p)->~T();
        return false;
    }
}

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
    t.info = &typeid(T);
    t.m_name = typeid(T).name();
    t.qualified_names = {
        t.m_name + QualifierSuffixes[Const].data(),
        t.m_name + QualifierSuffixes[Lvalue].data(),
        t.m_name + QualifierSuffixes[Rvalue].data()
    };

    if constexpr(is_usable<T>) {
        t.m_copy = default_copy<T>;
        t.m_destroy = default_destroy<T>;
        t.m_relocate = default_relocate<T>;
        t.m_to_value = default_to_value<T>;
        t.m_to_ref = default_to_ref<T>;
        t.m_from_ref = default_from_ref<T>;
        t.m_assign_if = default_assign_if<T>;
    } else {
        t.m_copy = [](void const *) -> void * {throw std::runtime_error("bad");};
        t.m_to_value = [](Value &, void *, Qualifier const) -> bool {throw std::runtime_error("bad");};
        t.m_to_ref = [](Ref &, void *, Qualifier const) -> bool {throw std::runtime_error("bad");};
        t.m_from_ref = [](Value &, Ref const &, Scope &) -> bool {throw std::runtime_error("bad");};
        t.m_assign_if = [](void *, Ref const &) -> bool {throw std::runtime_error("bad");};
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
