#pragma once
#include "Signature.h"
#include <vector>
#include <iostream>
#include <sstream>
#include <memory>

#define DUMP(...) ::rebind::dump(__FILE__, __LINE__, __VA_ARGS__);

namespace rebind {

template <class T, class SFINAE=void>
struct is_trivially_relocatable : std::is_trivially_copyable<T> {};

template <class T>
static constexpr bool is_trivially_relocatable_v = is_trivially_relocatable<T>::value;

/******************************************************************************/

extern bool Debug;
void set_debug(bool debug) noexcept;
bool debug() noexcept;

namespace runtime {
    char const * unknown_exception_description() noexcept;
}

/******************************************************************************/

/// Copy CV and reference qualifier from one type to another
template <class From, class To> struct copy_qualifier_t {using type = To;};
template <class From, class To> struct copy_qualifier_t<From &, To> {using type = To &;};
template <class From, class To> struct copy_qualifier_t<From const &, To> {using type = To const &;};
template <class From, class To> struct copy_qualifier_t<From &&, To> {using type = To &&;};

template <class From, class To> using copy_qualifier = typename copy_qualifier_t<From, To>::type;

/******************************************************************************/

/// For debugging purposes
template <class ...Ts>
void dump(char const *s, int n, Ts const &...ts) {
    if (!Debug) return;
    std::cout << '[' << s << ':' << n << "] ";
    int x[] = {(std::cout << ts, 0)...};
    std::cout << std::endl;
}

/******************************************************************************/

/// To avoid template type deduction on a given parameter
template <class T>
struct SameType {using type = T;};

/******************************************************************************/

struct Ignore {
    template <class ...Ts>
    Ignore(Ts const &...ts) {}
};

/******************************************************************************/

template <class T>
using SizeOf = std::integral_constant<std::size_t, sizeof(T)>;

/// Binary search of an iterable of std::pair
template <class V>
auto binary_search(V const &v, typename V::value_type::first_type t) {
    auto it = std::lower_bound(v.begin(), v.end(), t,
        [](auto const &x, auto const &t) {return x.first < t;});
    return (it != v.end() && it->first == t) ? it : v.end();
}

/******************************************************************************/

template <class T>
using Vector = std::vector<T>;

template <class T, class ...Ts>
Vector<T> vectorize(Ts &&...ts) {
    Vector<T> out;
    out.reserve(sizeof...(Ts));
    (out.emplace_back(static_cast<Ts &&>(ts)), ...);
    return out;
}

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
    virtual void enter() {};
    virtual ~Frame() {};
};

/******************************************************************************/

class Caller {
    std::weak_ptr<Frame> model;
public:
    Caller() = default;

    explicit operator bool() const {return !model.expired();}

    Caller(std::shared_ptr<Frame> const &f): model(f) {}

    void enter() {if (auto p = model.lock()) p->enter();}

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

}
