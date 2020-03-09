#pragma once
#include "Type.h"
#include "Common.h"
#include <map>
#include <stdexcept>
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

class OverloadedFunction;
class Function;
class Pointer;
class Value;


struct TypeTable {
    TypeIndex index;
    void (*destroy)(void *) noexcept;
    bool (*relocate)(void *, void *, unsigned short, unsigned short) noexcept; // either relocate the object to the void *, or ...
    void *(*copy)(void const *);
    Value (*response)(void *, Qualifier, TypeIndex);
    Value (*request)(Pointer const &);

    std::string name, const_name, lvalue_name, rvalue_name;

    std::vector<TypeIndex> bases;
    std::map<std::string_view, OverloadedFunction> methods;
};

/******************************************************************************************/

struct Table {
    constexpr Table() = default;
    TypeTable const *ptr = nullptr;

    TypeTable const* operator->() const {
        if (!ptr) throw std::runtime_error("Null TypeTable");
        return ptr;
    }

    bool has_base(TypeIndex const &idx) const {
        for (auto const &i : ptr->bases) if (idx == i) return true;
        return false;
    }

    constexpr bool operator==(Table t) const {return ptr == t.ptr;}
    constexpr bool operator!=(Table t) const {return ptr != t.ptr;}
    constexpr operator bool() const {return ptr;}
};

/******************************************************************************************/

struct OverloadedTable : Overload<Table> {
    using Overload<Table>::Overload;
};

/******************************************************************************************/

/// Return new-allocated copy of the object
template <class T>
void * default_copy(void const *p) {
    if constexpr(std::is_copy_constructible_v<T>) return new T(*static_cast<T const *>(p));
    return nullptr;
}

/// Destroy the object
template <class T>
void default_destroy(void *p) noexcept {delete static_cast<T *>(p);}

/// Not sure yet
template <class T>
bool default_relocate(void *out, void *p, unsigned short size, unsigned short align) noexcept {
    if (sizeof(T) <= size && alignof(T) <= align) {
        new(out) T(std::move(*static_cast<T *>(p)));
        static_cast<T *>(p)->~T();
        return true;
    } else {
        *static_cast<T **>(out) = new T(std::move(*static_cast<T *>(p)));
        static_cast<T *>(p)->~T();
        return false;
    }
}

template <class T>
Value default_response(void *, Qualifier, TypeIndex);

template <class T>
Value default_request(Pointer const &);

/******************************************************************************************/

template <class T>
TypeTable default_table() {
    assert_usable<T>();

    TypeTable t;

    t.index = typeid(T);
    t.name = typeid(T).name();
    t.const_name = t.name + QualifierSuffixes[Const].data();
    t.lvalue_name = t.name + QualifierSuffixes[Lvalue].data();
    t.rvalue_name = t.name + QualifierSuffixes[Rvalue].data();

    t.copy = default_copy<T>;
    t.destroy = default_destroy<T>;
    t.relocate = default_relocate<T>;
    t.response = default_response<T>;
    t.request = default_request<T>;
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