#pragma once
#include "Error.h"
#include "Type.h"
#include <map>
#include <stdexcept>

namespace rebind {

/******************************************************************************/

template <class T>
static constexpr bool is_usable = std::is_nothrow_move_constructible_v<T> && !is_type_t<T>::value;

/******************************************************************************************/

class Function;

struct Table {
    TypeIndex index;
    void (*destroy)(void *) noexcept;
    bool (*relocate)(void *, unsigned short, unsigned short) noexcept; // either relocate the object to the void *, or ...
    void *(*copy)(void *);
    void *(*response)(void *, Qualifier, TypeIndex);

    std::string name, const_name, lvalue_name, rvalue_name;

    std::vector<TypeIndex> bases;
    std::map<std::string_view, Function> methods;

    bool has_base(TypeIndex const &idx) const {
        for (auto const &i : bases) if (idx == i) return true;
        return false;
    }
};

/******************************************************************************************/

/// Return new-allocated copy of the object
template <class T>
void * default_copy(void const *p) {return new T(*static_cast<T const *>(p));}

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

class Value;

template <class T>
Value default_response(void *, Qualifier, TypeIndex);

template <class T>
struct TableGenerator {
    static Table *address;

    static Table table() {
        static_assert(std::is_nothrow_move_constructible_v<T>);

        void (*copy)(void *) = nullptr;
        if constexpr(std::is_copy_constructible_v<T>) copy = default_copy<T>;

        std::string const name = typeid(T).name();

        return {typeid(T), default_destroy<T>, default_relocate<T>, copy, default_response<T>,
                name, name + "const &", name + "&", name + "&&"};
    }
};

template <class T>
Table * TableGenerator<T>::address = nullptr;

/******************************************************************************************/

struct Opaque {
    Table const *table = nullptr;
    void *ptr = nullptr;

    template <class T>
    explicit Opaque(T *t) : table(TableGenerator<T>::address), ptr(static_cast<void *>(t)) {}

    void reset_pointer() {ptr = nullptr; table = nullptr;}

    void * value() const {return ptr;}

    bool has_value() const {return ptr;}

    template <class T>
    T *target() const {return (table == TableGenerator<T>::address) ? static_cast<T *>(ptr) : nullptr;}

    explicit operator bool() const {return ptr;}

    constexpr Opaque() = default;

    constexpr Opaque(Table const *t, void *p) noexcept : table(t), ptr(p) {}

    void try_destroy() const noexcept {if (has_value()) table->destroy(ptr);}

    template <class T>
    std::remove_reference_t<T> * request_reference(Qualifier q) const {
        using T0 = std::remove_reference_t<T>;
        if (auto p = target<T0>()) return p;
        if (has_value() && table->has_base(type_index<T0>())) return static_cast<T0 *>(ptr);
        return nullptr;
    }

    template <class T>
    std::optional<T> request_value(Scope &s, Qualifier q) const;

    TypeIndex index() const noexcept {return has_value() ? table->index : TypeIndex();}

    std::string_view name() const noexcept {
        if (has_value()) return table->name;
        else return {};
    }

    Opaque allocate_copy() const {
        if (has_value()) return {table, table->copy(ptr)};
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
    using Opaque::operator bool;

    constexpr Pointer() noexcept = default;

    constexpr Pointer(std::nullptr_t) : Pointer() {}

    template <class T>
    constexpr Pointer(T *t, Qualifier q) noexcept : Opaque(t), qual(q) {}

    constexpr Pointer(Opaque o, Qualifier q) noexcept : Opaque(std::move(o)), qual(q) {}

    Qualifier qualifier() const {return qual;}

    void reset() {reset_pointer();}

    /**************************************************************************************/

    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> * request(Scope &s, Type<T> t={}) const {
        return Opaque::request_reference<T>(qual);
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
        if (has_value()) {
            if (qual == Lvalue) return table->lvalue_name;
            else if (qual == Rvalue) return table->rvalue_name;
            else return table->const_name;
        } else return {};
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

}
