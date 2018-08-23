#pragma once
#include <stdexcept>
#include <string_view>
#include <vector>
#include <typeindex>
#include <algorithm>

namespace cpy {

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

struct WrongTypes : DispatchError {
    std::vector<unsigned int> indices;
    std::type_index source;
    std::type_index dest;
    unsigned int index;

    WrongTypes(char const *n, std::vector<unsigned int> &&v, std::type_index s, std::type_index d, unsigned int i)
        noexcept : DispatchError(n), indices(std::move(v)), source(s), dest(d), index(i) {}
};


struct DispatchMessage {
    std::vector<unsigned int> indices;
    std::type_index source = typeid(void);
    std::type_index dest = typeid(void);
    char const *scope;
    unsigned int index;

    WrongTypes error() noexcept {
        return {scope, std::move(indices), source, dest, index};
    }

    DispatchMessage(char const *s) : scope(s) {indices.reserve(4);}
};

/******************************************************************************/

}