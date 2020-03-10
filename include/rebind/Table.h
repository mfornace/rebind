#pragma once
#include "Type.h"
#include "Common.h"
#include <map>
#include <stdexcept>
#include <array>
#include <type_traits>

namespace rebind {

/******************************************************************************/

template <class T>
static constexpr bool is_usable = true
    && !std::is_reference_v<T>
    && !std::is_const_v<T>
    && !std::is_volatile_v<T>
    && !std::is_void_v<T>
    && std::is_nothrow_move_constructible_v<T>
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
    // static_assert(!std::is_null_pointer_v<T>);
    static_assert(!is_type_t<T>::value);
}

/******************************************************************************************/

class Overload;
class Function;
class Pointer;
class Value;
class Scope;


struct TypeTable {
    TypeIndex index;
    void (*m_destroy)(void*) noexcept = nullptr;
    bool (*m_relocate)(void*, void*, unsigned short, unsigned short) noexcept = nullptr; // either relocate the object to the void*, or ...
    void* (*m_copy)(void const*) = nullptr;
    Value (*m_response)(void*, Qualifier, TypeIndex) = nullptr;
    Value (*m_request)(Scope& s, Pointer const&) = nullptr;

    std::string m_name;
    std::array<std::string, 3> qualified_names;

    std::vector<TypeIndex> bases;
    std::map<std::string, Overload> methods;

    std::string const& name() const noexcept {return m_name;}

    std::string const& name(Qualifier q) const noexcept {return qualified_names[q];}

    void destroy(void*p) const noexcept {
        if (!m_destroy) std::terminate();
        m_destroy(p);
    }

    bool has_base(TypeIndex const &idx) const noexcept {
        for (auto const &i : bases) if (idx == i) return true;
        return false;
    }

    void* copy(void const* p) const {
        if (m_copy) return m_copy(p);
        throw std::runtime_error("held type is not copyable: " + name());
    }
};

/******************************************************************************************/

struct Table {
    constexpr Table() = default;
    TypeTable const* ptr = nullptr;

    TypeTable const* operator->() const {
        if (!ptr) throw std::runtime_error("Null TypeTable");
        return ptr;
    }

    constexpr bool operator==(Table t) const {return ptr == t.ptr;}
    constexpr bool operator!=(Table t) const {return ptr != t.ptr;}
    constexpr operator bool() const {return ptr;}
};

/******************************************************************************************/

// struct Table : Overloaded<Table> {
//     using Overloaded<Table>::Overloaded;
// };

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
Value default_response(void*, Qualifier, TypeIndex);

template <class T>
Value default_request(Scope &s, Pointer const &);

/******************************************************************************************/

template <class T>
TypeTable default_table() {
    assert_usable<T>();

    TypeTable t;

    t.index = typeid(T);
    t.m_name = typeid(T).name();
    t.qualified_names = {
        t.m_name + QualifierSuffixes[Const].data(),
        t.m_name + QualifierSuffixes[Lvalue].data(),
        t.m_name + QualifierSuffixes[Rvalue].data()};

    t.m_copy = default_copy<T>;
    t.m_destroy = default_destroy<T>;
    t.m_relocate = default_relocate<T>;
    t.m_response = default_response<T>;
    t.m_request = default_request<T>;
    return t;
}

/******************************************************************************************/

template <class T>
struct TableImplementation {
    static inline TypeTable table = default_table<T>();
};

template <class T>
Table get_table() {return {std::addressof(TableImplementation<T>::table)};}

/******************************************************************************************/

}
