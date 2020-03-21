#pragma once
#include "Raw.h"
#include "Scope.h"

namespace rebind {

/******************************************************************************************/

struct Ref : protected rebind_ref {

    constexpr Ref() noexcept : rebind_ref{nullptr, nullptr, 0} {}

    constexpr Ref(std::nullptr_t) noexcept : Ref() {}

    constexpr Ref(Index i, void *p, Qualifier q) noexcept
        : rebind_ref{i, p, static_cast<unsigned char>(q)} {}

    template <class T, std::enable_if_t<is_usable<T>, int> = 0>
    explicit constexpr Ref(T &t) noexcept
        : rebind_ref{fetch<T>(), std::addressof(t), Lvalue} {}

    template <class T, std::enable_if_t<is_usable<T>, int> = 0>
    explicit constexpr Ref(T const &t) noexcept
        : rebind_ref{fetch<T>(), std::addressof(const_cast<T &>(t)), Const} {}

    template <class T, std::enable_if_t<is_usable<T>, int> = 0>
    explicit constexpr Ref(T &&t) noexcept
        : rebind_ref{fetch<T>(), std::addressof(t), Rvalue} {}

    explicit constexpr Ref(Value const &);
    explicit constexpr Ref(Value &);
    explicit constexpr Ref(Value &&);

    Qualifier qualifier() const noexcept {return static_cast<Qualifier>(qual);}

    /**********************************************************************************/

    // Index index() const noexcept {return ind ? ind->index : Index();}

    rebind_ref const &as_raw() const noexcept {return static_cast<rebind_ref const &>(*this);}
    rebind_ref &as_raw() noexcept {return static_cast<rebind_ref &>(*this);}

    constexpr bool has_value() const noexcept {return ptr;}

    explicit constexpr operator bool() const noexcept {return ptr;}

    constexpr Index index() const noexcept {return ind;}

    constexpr void * address() const noexcept {return ptr;}

    void reset() {ind = nullptr; ptr = nullptr; qual = Const;}

    /**********************************************************************************/

    template <class T>
    bool set_if(T &&t) {
        if (qualifier_of<T &&> == qual && raw::matches<unqualified<T>>(ind)) {
            ptr = const_cast<T *>(static_cast<T const *>(std::addressof(t)));
            return true;
        } else return false;
    }

    /**************************************************************************************/

    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> *target() const {
        return compatible_qualifier(qualifier(), qualifier_of<T>) ? raw::target<unqualified<T>>(ind, ptr) : nullptr;
    }

    /**************************************************************************************/

    template <class T>
    auto request(Scope &s, Type<T> t={}) const {return raw::request(ind, ptr, s, t, qualifier());}

    template <class T>
    auto request(Type<T> t={}) const {Scope s; return request(s, t);}

    /**************************************************************************************/

    template <class T>
    T cast(Scope &s, Type<T> t={}) const {
        if (auto p = request(s, t)) return static_cast<T &&>(*p);
        throw std::move(s.set_error("invalid cast (rebind::Ref)"));
    }

    template <class T>
    T cast(Type<T> t={}) const {Scope s; return cast(s, t);}

    bool assign_if(Ref const &p) const {return qual != Const && raw::assign_if(ind, ptr, p);}

    /**************************************************************************************/

    std::string_view name(bool qualified=true) const noexcept {
        if (ind) return qualified ? ind->name(qualifier()) : ind->name();
        else return "null";
    }

    /**************************************************************************************/

    template <class T>
    void set(T &t) {*this = Ref(t);}

    template <class T>
    void set(T &&t) {*this = Ref(std::move(t));}

    template <class T>
    void set(T const &t) {*this = Ref(t);}

    // constexpr Ref(Erased const &o, Qualifier q) noexcept : Erased(o), qual(q) {}
    // template <class T>
    // static Ref from(T &t) {return {t, Lvalue};}

    // template <class T>
    // static Ref from(T const &t) {return {const_cast<T &>(t), Const};}

    // template <class T>
    // static Ref from(T &&t) {return {t, Rvalue};}

    // static Ref from(Value &t);
    // static Ref from(Value const &t);
    // static Ref from(Value &&t);

    // static Ref from(Ref p) {return p;}

    bool request_to(Value &v) const & {return raw::request_to(ind, ptr, v, qualifier());}
    bool request_to(Value &v) & {return raw::request_to(ind, ptr, v, qualifier());}
    bool request_to(Value &v) && {return raw::request_to(ind, ptr, v, qualifier());}
};

/******************************************************************************************/

static_assert(std::is_standard_layout_v<Ref>);

template <>
struct is_trivially_relocatable<Ref> : std::true_type {};

/******************************************************************************************/

using Arguments = Vector<Ref>;

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
