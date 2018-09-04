#pragma once
#include "Value.h"

namespace cpy {

/******************************************************************************/

using Binary = std::basic_string<unsigned char>;

using BinaryView = std::basic_string_view<unsigned char>;

/******************************************************************************/

#ifdef INTPTR_MAX
using Integer = std::intptr_t;
#else
using Integer = std::ptrdiff_t;
#endif

using Real = double;

template <class T>
struct ToValue<T, std::enable_if_t<(std::is_integral_v<T>)>> {
    Integer operator()(T t) const {return static_cast<Integer>(t);}
};

template <class T>
struct ToValue<T, std::enable_if_t<(std::is_floating_point_v<T>)>> {
    Real operator()(T t) const {return static_cast<Real>(t);}
};

template <class T>
struct FromValue<T, std::enable_if_t<std::is_arithmetic_v<T>>> {
    T operator()(Value const &u, Dispatch &message) const {
        auto t = u.type();
        if (t == typeid(bool)) return static_cast<T>(std::any_cast<bool>(u.any));
        if (t == typeid(Integer)) return static_cast<T>(std::any_cast<Integer>(u.any));
        if (t == typeid(Real)) return static_cast<T>(std::any_cast<Real>(u.any));
        throw message.error("not convertible to arithmetic value", u.type(), typeid(T));
    }
};

/******************************************************************************/

struct Sequence {
    Vector<Value> contents;
    Vector<std::size_t> shape;

    Sequence() = default;

    Sequence(std::initializer_list<Value> const &v) : contents(v) {}

    template <class ...Ts>
    static Sequence from_values(Ts &&...ts) {
        Sequence out;
        out.contents.reserve(sizeof...(Ts));
        (out.contents.emplace_back(static_cast<Ts &&>(ts)), ...);
        return out;
    }

    template <class V>
    explicit Sequence(V &&v) {
        contents.reserve(std::size(v));
        for (auto &&x : v) contents.emplace_back(static_cast<decltype(x) &&>(x));
    }
};

/******************************************************************************/

template <class T, class Alloc>
struct ToValue<std::vector<T, Alloc>> {
    Sequence operator()(std::vector<T, Alloc> t) const {return Sequence(std::move(t));}
};


template <class T>
struct FromValue<T, std::void_t<decltype(from_value(+Type<T>(), Sequence(), std::declval<Dispatch &>()))>> {
    // The common return type between the following 2 visitor member functions
    using out_type = std::remove_const_t<decltype(false ? std::declval<T &&>() :
        from_value(Type<T>(), std::declval<Value &&>(), std::declval<Dispatch &>()))>;

    out_type operator()(Value u, Dispatch &message) const {
        message.source = u.type();
        message.dest = typeid(T);
        return from_value(+Type<T>(), std::move(u), message);
    }
};

// static_assert(decltype(FromValue<double &>()(Value(), std::declval<Dispatch &>()))::aaa);

/******************************************************************************/

template <class V>
struct VectorFromValue {
    using T = no_qualifier<typename V::value_type>;

    V operator()(Value u, Dispatch &message) const {
        auto &&cts = std::move(std::any_cast<Sequence &>(u.any).contents);
        V out;
        out.reserve(cts.size());
        message.indices.emplace_back(0);
        for (auto &x : cts) {
            out.emplace_back(FromValue<T>()(std::move(x), message));
            ++message.indices.back();
        }
        message.indices.pop_back();
        return out;
    }
};

template <class T>
struct FromValue<Vector<T>> : VectorFromValue<Vector<T>> {};

}