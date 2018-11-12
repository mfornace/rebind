#pragma once
#include <stdexcept>
#include <string_view>
#include <vector>
#include <typeindex>
#include <algorithm>
#include <string>
#include <any>
#include <deque>
#include <optional>

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
    std::deque<std::any> storage;
    std::string scope;
    std::vector<unsigned int> indices;
    std::type_index source = typeid(void);
    std::type_index dest = typeid(void);
    unsigned int index = 0, expected = 0, received = 0;

    std::nullopt_t error() noexcept {return std::nullopt;}

    std::nullopt_t error(std::string msg) noexcept {
        scope = std::move(msg);
        return std::nullopt;
    }

    std::nullopt_t error(std::type_index s, std::type_index d) noexcept {
        source = s;
        dest = d;
        return std::nullopt;
    }

    std::nullopt_t error(std::string msg, std::type_index s, std::type_index d, unsigned int e=0, unsigned int r=0) noexcept {
        scope = std::move(msg);
        source = s;
        dest = d;
        expected = e;
        received = r;
        return std::nullopt;
    }

    WrongType exception() noexcept {
        return {std::move(scope), std::move(indices), source, dest, index, expected, received};
    }

    template <class T>
    auto store(T &&t) {
        return std::addressof(storage.emplace_back().emplace<no_qualifier<T>>(static_cast<T &&>(t)));
    }

    Dispatch(char const *s="mismatched type") : scope(s) {indices.reserve(8);}
};

/******************************************************************************/

}