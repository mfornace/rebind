#pragma once
#include "Erased.h"

namespace rebind {

/******************************************************************************************/

class Pointer : protected Erased {
protected:
    Qualifier qual = Const;

public:
    using Erased::index;
    using Erased::has_value;
    using Erased::table;
    using Erased::reset;
    using Erased::operator bool;

    constexpr Pointer() noexcept = default;

    constexpr Pointer(std::nullptr_t) noexcept : Pointer() {}

    template <class T, std::enable_if_t<is_usable<T>, int> = 0>
    explicit constexpr Pointer(T &t) noexcept : Erased(std::addressof(t)), qual(Lvalue) {}

    template <class T, std::enable_if_t<is_usable<T>, int> = 0>
    explicit constexpr Pointer(T const &t) noexcept : Erased(std::addressof(const_cast<T &>(t))), qual(Const) {}

    template <class T, std::enable_if_t<is_usable<T>, int> = 0>
    explicit constexpr Pointer(T &&t) noexcept : Erased(std::addressof(t)), qual(Rvalue) {}

    Qualifier qualifier() const noexcept {return qual;}

    /**************************************************************************************/

    template <class T>
    bool set(T &&t) {
        if (qualifier_of<T &&> == qual && tab->index.equals<unqualified<T>>()) {
            ptr = const_cast<T *>(static_cast<T const *>(std::addressof(t)));
            return true;
        } else return false;
    }

    /**************************************************************************************/

    template <class T>
    auto request(Scope &s, Type<T> t={}) const {return Erased::request(s, t, qual);}

    template <class T>
    auto request(Type<T> t={}) const {Scope s; return request(s, t);}

    /**************************************************************************************/

    template <class T>
    T cast(Scope &s, Type<T> t={}) const {
        if (auto p = request(s, t)) return static_cast<T &&>(*p);
        throw std::move(s.set_error("invalid cast (rebind::Pointer)"));
    }

    template <class T>
    T cast(Type<T> t={}) const {Scope s; return cast(s, t);}

    /**************************************************************************************/

    std::string_view name(bool qualified=true) const noexcept {
        if (has_value()) return qualified ? table()->name(qual) : table()->name();
        else return "<null>";
    }

    /**************************************************************************************/

    template <class T>
    void set(T &t) {*this = Pointer(t, Lvalue);}

    template <class T>
    void set(T &&t) {*this = Pointer(t, Rvalue);}

    template <class T>
    void set(T const &t) {*this = Pointer(const_cast<T &>(t), Const);}


    constexpr Pointer(Erased const &o, Qualifier q) noexcept : Erased(o), qual(q) {}
    // template <class T>
    // static Pointer from(T &t) {return {t, Lvalue};}

    // template <class T>
    // static Pointer from(T const &t) {return {const_cast<T &>(t), Const};}

    // template <class T>
    // static Pointer from(T &&t) {return {t, Rvalue};}

    // static Pointer from(Value &t);
    // static Pointer from(Value const &t);
    // static Pointer from(Value &&t);

    // static Pointer from(Pointer p) {return p;}

    bool request_to(Value &v) const & {return Erased::request_to(v, qual);}
    bool request_to(Value &v) & {return Erased::request_to(v, qual);}
    bool request_to(Value &v) && {return Erased::request_to(v, qual);}
};

/******************************************************************************************/

using Arguments = Vector<Pointer>;

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
