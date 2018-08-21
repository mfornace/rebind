#pragma once
#include <stdexcept>
#include <string_view>
#include <vector>
#include <typeindex>
#include <algorithm>

namespace cpy {

template <class T>
using SizeOf = std::integral_constant<std::size_t, sizeof(T)>;

template <class V>
auto binary_lookup(V const &v, typename V::value_type t) {
    auto it = std::lower_bound(v.begin(), v.end(), t,
        [](auto const &x, auto const &y) {return x.first < y.first;});
    return (it != v.end() && it->first != t.first) ? it : v.end();
}

/******************************************************************************/

struct DispatchError : std::invalid_argument {
    using std::invalid_argument::invalid_argument;
};

/******************************************************************************/

struct WrongNumber : DispatchError {
    unsigned int expected, received;
    WrongNumber(unsigned int n0, unsigned int n)
        : DispatchError("wrong number of arguments"), expected(n0), received(n) {}
};


/******************************************************************************/

struct ClientError : std::exception {
    std::string_view message;
    explicit ClientError(std::string_view const &s) noexcept : message(s) {}
    char const * what() const noexcept override {return message.empty() ? "cpy::ClientError" : message.data();}
};

/******************************************************************************/

struct DispatchMessage {
    std::vector<unsigned int> indices;
    std::type_index source = typeid(void);
    std::type_index dest = typeid(void);
    char const *scope;
    unsigned int index;

    DispatchMessage(char const *s) : scope(s) {indices.reserve(4);}
};

struct WrongTypes : DispatchError {
    std::vector<unsigned int> indices;
    std::type_index source;
    std::type_index dest;
    unsigned int index;

    WrongTypes(DispatchMessage &&m)
        : DispatchError(m.scope), indices(std::move(m.indices)), dest(m.dest), source(m.source), index(m.index) {}
};

/******************************************************************************/

}