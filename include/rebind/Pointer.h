#pragma once
#include "Error.h"
#include "Type.h"

namespace rebind {
/******************************************************************************************/

struct Table {
    TypeIndex index;
    void (*destroy)(void *) noexcept;
    bool (*relocate)(void *, unsigned short, unsigned short) noexcept; // either relocate the object to the void *, or ...
    void *(*copy)(void *);
    void *(*request_reference)(void *, Qualifier, TypeIndex);
    void *(*request_value)(void *, Qualifier, TypeIndex);
    std::string name, const_name, lvalue_name, rvalue_name;
};

/******************************************************************************************/

/// Return new-allocated copy of the object
template <class T>
void * default_copy(void const *p) {return new T(*static_cast<T const *>(p));}

/// Destroy the object
template <class T>
void default_destroy(void *p) noexcept {static_cast<T *>(p)->~T();}

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
struct TableGenerator {
    static Table const *address;

    static Table table() {
        static_assert(std::is_nothrow_move_constructible_v<T>);
        void (*copy)(void *) = nullptr;
        if constexpr(std::is_copy_constructible_v<T>) copy = default_copy<T>;
        std::string const name = typeid(T).name();
        return {typeid(T), default_destroy<T>, default_relocate<T>, copy, nullptr,
                name, name + "const &", name + "&", name + "&&"};
    }
};

template <class T>
Table const * TableGenerator<T>::address = nullptr;

/******************************************************************************************/

struct Opaque {
    Table const *table = nullptr;
    void *ptr = nullptr;

    template <class T>
    explicit Opaque(T *t) : table(TableGenerator<T>::address), ptr(static_cast<void *>(t)) {}

    void reset() {ptr = nullptr;}

    bool has_value() const {return ptr;}

    template <class T>
    T *target();

    explicit operator bool() const {return ptr;}

    constexpr Opaque() = default;

    constexpr Opaque(Table const *t, void *p) noexcept : table(t), ptr(p) {}

    void try_destroy() const noexcept {if (has_value()) table->destroy(ptr);}

    template <class T>
    T * request_reference(Qualifier q) const {return has_value() ? table->request_reference(ptr, q, typeid(T)) : nullptr;}

    template <class T>
    T * request_value(Qualifier q) const {return has_value() ? table->request_value(ptr, q, typeid(T)) : nullptr;}

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

    void * value() const {return ptr;}

    Qualifier qualifier() const {return qual;}

    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> * request(Scope s={}) const;// {return Opaque::request_reference<T>(qual);}

    template <class T, std::enable_if_t<!std::is_reference_v<T>, int> = 0>
    std::optional<T> request(Scope s={}) const;

    std::string_view qualified_name() const noexcept {
        if (has_value()) {
            if (qual == Lvalue) return table->lvalue_name;
            else if (qual == Rvalue) return table->rvalue_name;
            else return table->const_name;
        } else return {};
    }
    // bool relocate(void *, unsigned short n) noexcept {return table->relocate(ptr, n);}

    /**************************************************************************************/

    template <class T>
    static Pointer from(T &t) {return {std::addressof(t), Lvalue};}

    template <class T>
    static Pointer from(T const &t) {return {const_cast<T *>(std::addressof(t)), Const};}

    template <class T>
    static Pointer from(T &&t) {return {std::addressof(t), Rvalue};}
};

/******************************************************************************************/

}
