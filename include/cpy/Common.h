#pragma once
#include "Signature.h"
#include <vector>
#include <variant>

namespace cpy {

/******************************************************************************/

template <class T>
using SizeOf = std::integral_constant<std::size_t, sizeof(T)>;

template <class V>
auto binary_lookup(V const &v, typename V::value_type t) {
    auto it = std::lower_bound(v.begin(), v.end(), t,
        [](auto const &x, auto const &y) {return x.first < y.first;});
    return (it != v.end() && it->first != t.first) ? it : v.end();
}

template <class ...Ts>
std::variant<Ts...> variant_type(Pack<Ts...>); // undefined

template <class T>
using no_qualifier = std::remove_cv_t<std::remove_reference_t<T>>;

struct Identity {
    template <class T>
    T const & operator()(T const &t) const {return t;}
};

/******************************************************************************/

template <class T, class U>
static constexpr bool Reinterpretable = (sizeof(T) == sizeof(U)) && alignof(T) == alignof(U);

static_assert(!std::is_same_v<unsigned char, char>);
static_assert(Reinterpretable<unsigned char, char>);
// Standard: a char, a signed char, and an unsigned char occupy
// the same amount of storage and have the same alignment requirements

/******************************************************************************/

template <class T>
using Vector = std::vector<T>;

template <class T, class V, class F=Identity>
Vector<T> mapped(V const &v, F &&f) {
    Vector<T> out;
    out.reserve(std::size(v));
    for (auto &&x : v) out.emplace_back(f(x));
    return out;
}

/******************************************************************************/

// Roughly, a type safe version of std::any but simpler and non-owning
class CallingContext {
    void *metadata = nullptr;
    std::type_index index = typeid(void);
public:
    CallingContext() = default;

    template <class T>
    CallingContext(T *t) : metadata(t), index(typeid(T)) {}

    template <class T>
    T & get() {
        if (index != typeid(T)) throw DispatchError("invalid context");
        return *static_cast<T *>(metadata);
    }
};

/******************************************************************************/

}