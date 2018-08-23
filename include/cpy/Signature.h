/**
 * @brief C++ functor signature deduction
 * @file Signature.h
 */

#pragma once
#include <utility>
#include <typeindex>

namespace cpy {

template <class T>
using no_qualifier = std::remove_cv_t<std::remove_reference_t<T>>;

/******************************************************************************************/

template <class T>
struct Type  {
    T operator*() const; // undefined
    constexpr Type<no_qualifier<T>> operator+() const {return {};}
    operator std::type_index() const {return typeid(T);}
};

template <class T>
struct IndexedType {
    std::size_t index;
    T operator*() const; // undefined
    constexpr Type<no_qualifier<T>> operator+() const {return {};}
};

/******************************************************************************************/

template <class U> std::integral_constant<int, -1> index_of_type();

template <class U, class T, class ...Ts, std::enable_if_t<std::is_same_v<U, T>, int> = 0>
std::integral_constant<int, 0> index_of_type();

template <class U, class T, class ...Ts, std::enable_if_t<!std::is_same_v<U, T>, int> = 0>
auto index_of_type() {return std::integral_constant<int, decltype(index_of_type<U, Ts...>())::value + 1>();}

template <bool ...Ts>
struct BoolPack;

template <bool ...Ts>
static constexpr bool any_of_c = !std::is_same_v<BoolPack<false, Ts...>, BoolPack<Ts..., false>>;

/******************************************************************************************/

/// A lightweight ordered container of types
template <class ...Ts> struct Pack {
    using pack_type = Pack;
    using size = std::integral_constant<std::size_t, sizeof...(Ts)>;

    template <class T>
    static constexpr bool contains = any_of_c<std::is_same_v<T, Ts>...>;

    template <class T>
    static constexpr int position = decltype(index_of_type<Ts...>())::value;

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

template <class F>
void visit_packs(F const f) {f();}

template <class F, class T, class ...Ts>
void visit_packs(F const f, T, Ts ...) {
    T::for_each([f](auto t) {visit_packs([f](auto ...us) {f(decltype(t)(), us...);}, Ts()...);});
}

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

}