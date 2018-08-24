#pragma once
#include "Signature.h"
#include <vector>
#include <variant>

#include <boost/container/vector.hpp>

namespace cpy {

/******************************************************************************/

template <class T>
using SizeOf = std::integral_constant<std::size_t, sizeof(T)>;

template <class V>
auto binary_search(V const &v, typename V::value_type::first_type t) {
    auto it = std::lower_bound(v.begin(), v.end(), t,
        [](auto const &x, auto const &t) {return x.first < t;});
    return (it != v.end() && it->first == t) ? it : v.end();
}

template <class ...Ts>
std::variant<Ts...> variant_type(Pack<Ts...>); // undefined

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

template <class T, class U>
using Zip = Vector<std::pair<T, U>>;

template <class T, class V, class F=Identity>
Vector<T> mapped(V const &v, F &&f) {
    Vector<T> out;
    out.reserve(std::size(v));
    for (auto &&x : v) out.emplace_back(f(x));
    return out;
}

/******************************************************************************/

// Roughly, a type safe version of std::any but simpler and non-owning
class Caller {
    void *metadata = nullptr;
    std::type_index index = typeid(void);
public:
    Caller() = default;

    template <class T>
    Caller(T *t) : metadata(t), index(typeid(T)) {}

    template <class T>
    T & cast(Type<T> = {}) {
        if (index != typeid(T)) throw DispatchError("invalid Caller cast");
        return *static_cast<T *>(metadata);
    }
};

/******************************************************************************/

}