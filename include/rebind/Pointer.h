#pragma once

namespace rebind {
/******************************************************************************************/

struct Table {
    std::type_info const *info;
    void (*)(void *) noexcept destroy;
    bool (*)(void *, unsigned short, unsigned short) noexcept relocate; // either relocate the object to the void *, or ...
    void (*)(void *) copy;
    void (*)(void *, Qualifier, std::type_index) request;
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
    static Table const *address = nullptr;

    static Table table() {
        static_assert(std::is_nothrow_move_constructible<T>);
        void (*)(void *) copy = nullptr;
        if constexpr(std::is_copy_constructible_v<T>) copy = default_copy<T>;
        std::string const name = typeid(T).name();
        return {typeid(T), default_destroy<T>, default_relocate<T>, copy, request,
                name, name + "const &", name + "&", name + "&&"};
    }
}

/******************************************************************************************/

class Pointer {
protected:
    Table const *table;
    void *ptr = nullptr;
    Qualifier qual;

public:
    constexpr Pointer() noexcept = default;

    constexpr Pointer(Table const *t, void *p, Qualifier q) noexcept : table(t), ptr(p), qual(q) {}

    bool has_value() const {return ptr;}

    void * value() const {return ptr;}

    Qualfier qualifier() const {return qual;}

    Pointer allocate_copy() const {
        if (has_value()) return {table, table->copy(ptr), Lvalue};
        else return {};
    }

    void destroy_in_place() const noexcept {if (has_value()) table->destroy(ptr);}

    void reset() {ptr = nullptr;}

    template <class T>
    T * request(Qualifier q) const {return has_value() ? table->request(ptr, q, typeid(T)) : nullptr;}

    template <class T>
    T * request() const {return request(qual);}

    std::type_index index() const noexcept {return has_value() ? *table->info : typeid(void);}

    std::string_view name() const noexcept {
        if (has_value()) return table->name;
        else return {};
    }

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
    static Pointer from(T &t) {return {TableGenerator<T>::address, std::addressof(t), Lvalue};}

    template <class T>
    static Pointer from(T const &t) {return {TableGenerator<T>::address, std::addressof(t), Const};}

    template <class T>
    static Pointer from(T &&t) {return {TableGenerator<T>::address, std::addressof(t), Rvalue};}
};

/******************************************************************************************/

class Value : protected Pointer {
public:
    using Pointer::name;

    using Pointer::index;

    constexpr Value() noexcept = default;

    Value(Value const &v) : Pointer(v.allocate_copy()) {}

    Value(Value &&v) noexcept : Pointer(static_cast<Pointer &&>(v)) {v.reset();}

    template <class T>
    std::optional<T> request() const {
        std::optional<T> out;
        if (auto p = Pointer::request<T>(Const)) {out.emplace(std::move(*p)); delete p;}
        return out;
    }

    template <class T>
    std::optional<T> request() && {
        std::optional<T> out;
        if (auto p = Pointer::request<T>(Rvalue)) {out.emplace(std::move(*p)); delete p;}
        return out;
    }

    ~Value() {destroy_in_place();}
};

/******************************************************************************************/

}
