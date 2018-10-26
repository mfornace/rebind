#pragma once
#include "Signature.h"
#include <vector>

namespace cpy {

static constexpr bool Debug = true;

/******************************************************************************/

template <class T>
using SizeOf = std::integral_constant<std::size_t, sizeof(T)>;

template <class V>
auto binary_search(V const &v, typename V::value_type::first_type t) {
    auto it = std::lower_bound(v.begin(), v.end(), t,
        [](auto const &x, auto const &t) {return x.first < t;});
    return (it != v.end() && it->first == t) ? it : v.end();
}

// struct Identity {
//     template <class T>
//     T const & operator()(T const &t) const {return t;}
// };

// struct Ignore {
//     template <class T>
//     Ignore(T const &) {}

//     template <class T>
//     Ignore &operator=(T const &) {return *this;}
// };

/******************************************************************************/

// template <class T, class U>
// static constexpr bool Reinterpretable = sizeof(T) == sizeof(U) && alignof(T) == alignof(U)
//                                       && std::is_pod_v<T> && std::is_pod_v<U>;

// static_assert(!std::is_same_v<unsigned char, char>);
// static_assert(Reinterpretable<unsigned char, char>);
// Standard: a char, a signed char, and an unsigned char occupy
// the same amount of storage and have the same alignment requirements

/******************************************************************************/

template <class T>
using Vector = std::vector<T>;

template <class ...Ts>
struct ZipType {using type = std::tuple<Ts...>;};

template <class T, class U>
struct ZipType<T, U> {using type = std::pair<T, U>;};

template <class ...Ts>
using Zip = Vector<typename ZipType<Ts...>::type>;

template <class T, class V, class F>
Vector<T> mapped(V const &v, F &&f) {
    Vector<T> out;
    out.reserve(std::size(v));
    for (auto &&x : v) out.emplace_back(f(x));
    return out;
}

/******************************************************************************/

/// Interface: return a new frame given a shared_ptr of *this
struct Frame {
    virtual std::shared_ptr<Frame> operator()(std::shared_ptr<Frame> &&) = 0;
    virtual ~Frame() {};
};

class Caller {
    std::weak_ptr<Frame> model;
public:
    Caller() = default;

    Caller(std::shared_ptr<Frame> const &f): model(f) {}

    std::shared_ptr<Frame> operator()() const {
        if (auto p = model.lock()) return p.get()->operator()(std::move(p));
        return {};
    }

    template <class T>
    T * target() {
        if (auto p = model.lock()) return dynamic_cast<T *>(p.get());
        return nullptr;
    }
};

/******************************************************************************/

enum class Qualifier : unsigned char {V, C, L, R};

struct cvalue {constexpr operator Qualifier() const {return Qualifier::C;}};
struct lvalue {constexpr operator Qualifier() const {return Qualifier::L;}};
struct rvalue {constexpr operator Qualifier() const {return Qualifier::R;}};

template <class T, class Ref> struct Qualified;
template <class T> struct Qualified<T, cvalue> {using type = T const &;};
template <class T> struct Qualified<T, lvalue> {using type = T &;};
template <class T> struct Qualified<T, rvalue> {using type = T &&;};

template <class Ref, class T> using qualified = typename Qualified<Ref, T>::type;

template <class T>
static constexpr Qualifier qualifier_of = (!std::is_reference_v<T>) ? Qualifier::V
    : (std::is_rvalue_reference_v<T> ? Qualifier::R :
        (std::is_const_v<std::remove_reference_t<T>> ? Qualifier::C : Qualifier::L));

/******************************************************************************/

// template <class T>
// struct UnqualifiedID {static constexpr bool id = true;}

// template <class T>
// struct QualifiedID {static constexpr bool * id_ptr = &UnqualifiedID<std::decay_t<T>>::id;};

// class TypeID {
//     bool **ptr;
// public:
//     template <class T>
//     constexpr TypeID(Type<T>) : ptr(&QualifiedTypeID<T>::id_ptr) {}

//     friend operator=(TypeID)
// };

/******************************************************************************/

}