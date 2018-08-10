/**
 * @brief C++ functor signature deduction
 * @file Signature.h
 */

#pragma once
#include <utility>

/******************************************************************************************/

template <class T>
struct IndexedType {
    std::size_t index;
    T operator*() const; // undefined
};

/******************************************************************************************/

/// A lightweight ordered container of types
template <class ...Ts> struct Pack {
    using size = std::integral_constant<std::size_t, sizeof...(Ts)>;

    template <class F, std::size_t ...Is>
    static constexpr auto apply(F &&f, std::index_sequence<Is...>) {
        return static_cast<F &&>(f)(IndexedType<Ts>{Is}...);
    }

    template <class F>
    static constexpr auto apply(F &&f) {
        return apply(static_cast<F &&>(f), std::make_index_sequence<sizeof...(Ts)>());
    }

    template <class F, std::size_t ...Is>
    static void for_each(F &&f, std::index_sequence<Is...>) {(f(IndexedType<Ts>{Is}), ...);}

    template <class F>
    static void for_each(F &&f) {for_each(f, std::make_index_sequence<sizeof...(Ts)>());}
};

/******************************************************************************************/

template <class F, class=void>
struct Signature;

template <class R, class ...Ts>
struct Signature<R(Ts...)> : Pack<R, Ts...> {
    using return_type = R;
};

template <class R, class ...Ts>
struct Signature<R(*)(Ts...)> : Signature<R(Ts...)> {};

#define CPY_TMP(C, Q, C2) \
    template <class R, class C, class ...Ts> \
    struct Signature<R (C::* )(Ts...) Q> : Pack<R, C2, Ts...> { \
        using return_type = R; \
    };

    CPY_TMP(C, , C &);
    CPY_TMP(C, const, C const &);
    CPY_TMP(C, &, C &);
    CPY_TMP(C, const &, C const &);
    CPY_TMP(C, &&, C &&);
    CPY_TMP(C, const &&, C const &&);
#undef CPY_TMP

/******************************************************************************************/

template <class T>
struct FunctorSignature;

#define CPY_TMP(Q) template <class R, class C, class ...Ts> \
    struct FunctorSignature<R (C::* )(Ts...) Q> : Signature<R(Ts...)> {};
    CPY_TMP( );
    CPY_TMP(&);
    CPY_TMP(&&);
    CPY_TMP(const);
    CPY_TMP(const &);
    CPY_TMP(const &&);
#undef CPY_TMP

/******************************************************************************************/

template <class F>
struct Signature<F, std::void_t<decltype(&F::operator())>> : FunctorSignature<decltype(&F::operator())> {};
