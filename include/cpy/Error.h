#pragma once
#include <stdexcept>
#include <string_view>
#include <vector>
#include <typeindex>
#include <algorithm>
#include <string>

namespace cpy {

/******************************************************************************/

struct ClientError : std::exception {
    std::string_view message;
    explicit ClientError(std::string_view const &s) noexcept : message(s) {}
    char const * what() const noexcept override {return message.empty() ? "cpy::ClientError" : message.data();}
};

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

struct WrongType : DispatchError {
    std::vector<unsigned int> indices;
    std::type_index source;
    std::type_index dest;
    unsigned int index, expected, received;

    WrongType(std::string const &n, std::vector<unsigned int> &&v,
               std::type_index s, std::type_index d,
               unsigned int i, unsigned int e=0, unsigned int r=0)
        noexcept : DispatchError(n), indices(std::move(v)), source(s), dest(d),
                   index(i), expected(e), received(r) {}
};

/******************************************************************************/

struct Dispatch {
    std::string scope;
    std::vector<unsigned int> indices;
    std::type_index source = typeid(void);
    std::type_index dest = typeid(void);
    unsigned int index;

    WrongType error() noexcept {
        return {scope, std::move(indices), source, dest, index};
    }

    WrongType error(std::string const &scope2) noexcept {
        return {scope2, std::move(indices), source, dest, index};
    }

    WrongType error(std::type_index s, std::type_index d) noexcept {
        return {scope, std::move(indices), s, d, index};
    }

    WrongType error(std::string const &scope2, std::type_index s, std::type_index d, unsigned int e=0, unsigned int r=0) noexcept {
        return {scope2, std::move(indices), s, d, index, e, r};
    }

    Dispatch(char const *s="mismatched type") : scope(s) {indices.reserve(8);}
};

/******************************************************************************/

}