#pragma once
#include "Erased.h"

namespace rebind {

/******************************************************************************************/

class Ref : protected Erased {
protected:
    Qualifier qual = Const;

public:
    using Erased::index;
    using Erased::as_erased;
    using Erased::has_value;
    using Erased::table;
    using Erased::reset;
    using Erased::operator bool;

    constexpr Ref() noexcept = default;

    constexpr Ref(std::nullptr_t) noexcept : Ref() {}

    template <class T, std::enable_if_t<is_usable<T>, int> = 0>
    explicit constexpr Ref(T &t) noexcept : Erased(std::addressof(t)), qual(Lvalue) {}

    template <class T, std::enable_if_t<is_usable<T>, int> = 0>
    explicit constexpr Ref(T const &t) noexcept : Erased(std::addressof(const_cast<T &>(t))), qual(Const) {}

    template <class T, std::enable_if_t<is_usable<T>, int> = 0>
    explicit constexpr Ref(T &&t) noexcept : Erased(std::addressof(t)), qual(Rvalue) {}

    explicit constexpr Ref(Value const &);
    explicit constexpr Ref(Value &);
    explicit constexpr Ref(Value &&);

    Qualifier qualifier() const noexcept {return qual;}

    /**************************************************************************************/

    template <class T>
    bool set_if(T &&t) {
        if (qualifier_of<T &&> == qual && tab->index.equals<unqualified<T>>()) {
            ptr = const_cast<T *>(static_cast<T const *>(std::addressof(t)));
            return true;
        } else return false;
    }

    /**************************************************************************************/

    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> *target() const {
        return compatible_qualifier(qual, qualifier_of<T>) ? Erased::target<unqualified<T>>() : nullptr;
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
        throw std::move(s.set_error("invalid cast (rebind::Ref)"));
    }

    template <class T>
    T cast(Type<T> t={}) const {Scope s; return cast(s, t);}

    bool assign_if(Ref const &p) const {return qual != Const && Erased::assign_if(p);}

    /**************************************************************************************/

    std::string_view name(bool qualified=true) const noexcept {
        if (has_type()) return qualified ? table()->name(qual) : table()->name();
        else return "null";
    }

    /**************************************************************************************/

    template <class T>
    void set(T &t) {*this = Ref(t);}

    template <class T>
    void set(T &&t) {*this = Ref(std::move(t));}

    template <class T>
    void set(T const &t) {*this = Ref(t);}


    constexpr Ref(Erased const &o, Qualifier q) noexcept : Erased(o), qual(q) {}
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

    bool request_to(Value &v) const & {return Erased::request_to(v, qual);}
    bool request_to(Value &v) & {return Erased::request_to(v, qual);}
    bool request_to(Value &v) && {return Erased::request_to(v, qual);}
};

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
