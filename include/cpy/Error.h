#pragma once
#include <stdexcept>
#include <string_view>
#include <vector>

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

    WrongTypes() : DispatchError("wrong argument types") {}
};

/******************************************************************************/

}