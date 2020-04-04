#pragma once
#include "Raw.h"
#include "Scope.h"
#include "Common.h"

namespace rebind {

/******************************************************************************************/

struct Ref : protected rebind_ref {

    constexpr Ref() noexcept : rebind_ref{nullptr, nullptr} {}

    constexpr Ref(std::nullptr_t) noexcept : Ref() {}

    Ref(Index i, void *p, Qualifier q) noexcept
        : rebind_ref{i, rebind_make_ptr(p, q)} {DUMP(p, " ", tagged_ptr, " ", address());}

    template <class T>
    explicit Ref(T &t) noexcept
        : rebind_ref{Index::of<T>(), rebind_make_ptr(std::addressof(t), Lvalue)} {}

    template <class T>
    explicit Ref(T const &t) noexcept
        : rebind_ref{Index::of<T>(), rebind_make_ptr(std::addressof(const_cast<T &>(t)), Const)} {}

    template <class T>
    explicit Ref(T &&t) noexcept
        : rebind_ref{Index::of<T>(), rebind_make_ptr(std::addressof(t), Rvalue)} {}

    explicit Ref(Value const &);
    explicit Ref(Value &);
    explicit Ref(Value &&);

    Qualifier qualifier() const noexcept {return static_cast<Qualifier>(rebind_get_qualifier(tagged_ptr));}

    /**********************************************************************************/

    constexpr bool has_value() const noexcept {return tagged_ptr;}

    explicit constexpr operator bool() const noexcept {return has_value();}

    constexpr Index index() const noexcept {return {ind};}

    void * address() const noexcept {return rebind_get_address(tagged_ptr);}

    void reset() {ind = nullptr; tagged_ptr = nullptr;}

    /**********************************************************************************/

    template <class T>
    bool set_if(T &&t) {
        if (qualifier_of<T &&> == qualifier() && index() == Index::of<unqualified<T>>()) {
            tagged_ptr = rebind_make_ptr(const_cast<T *>(static_cast<T const *>(std::addressof(t))), qualifier());
            return true;
        } else return false;
    }

    /**************************************************************************************/

    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> *target() const {
        return compatible_qualifier(qualifier(), qualifier_of<T>) ? raw::target<unqualified<T>>(ind, address()) : nullptr;
    }

    /**************************************************************************************/

    template <class T>
    auto request(Scope &s, Type<T> t={}) const {return raw::request(ind, address(), s, t, qualifier());}

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

    bool assign_if(Ref const &p) const {
        return qualifier() != Const && stat::assign_if::ok == raw::assign_if(index(), address(), p);
    }

    /**************************************************************************************/

    std::string_view name() const noexcept {return index().name();}

    /**************************************************************************************/

    template <class T>
    void set(T &t) {*this = Ref(t);}

    template <class T>
    void set(T &&t) {*this = Ref(std::move(t));}

    template <class T>
    void set(T const &t) {*this = Ref(t);}

    /**************************************************************************************/

    bool request_to(Output &v) const & {return stat::request::ok == raw::request_to(v, index(), address(), qualifier());}
    bool request_to(Output &v) & {return stat::request::ok == raw::request_to(v, index(), address(), qualifier());}
    bool request_to(Output &v) && {return stat::request::ok == raw::request_to(v, index(), address(), qualifier());}

    /**************************************************************************************/

    template <class ...Args>
    Value call_value(Args &&...args) const;

    template <class ...Args>
    Ref call_ref(Args &&...args) const;

    bool call_to(Value &, ArgView) const noexcept;

    bool call_to(Ref &, ArgView) const noexcept;
};

/******************************************************************************************/

static_assert(std::is_standard_layout_v<Ref>);

template <>
struct is_trivially_relocatable<Ref> : std::true_type {};

/******************************************************************************************/

}
