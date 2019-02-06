#pragma once
#include "Signature.h"

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
    std::string source;
    std::type_index dest;
    int index, expected, received;

    WrongType(std::string const &n, std::vector<unsigned int> &&v,
              std::string &&s, std::type_index &&d, int i, int e=0, int r=0) noexcept
        : DispatchError(n), indices(std::move(v)), source(std::move(s)),
          dest(std::move(d)), index(i), expected(e), received(r) {}
};

/******************************************************************************/

struct Dispatch {
    std::string scope;
    Caller caller;
    std::deque<std::any> storage; // deque is used so references don't go bad when doing emplace_back()
    std::vector<unsigned int> indices;
    std::string source;
    std::type_index dest = typeid(void);
    int index = -1, expected = -1, received = -1;

    std::nullopt_t error() noexcept {return std::nullopt;}

    std::nullopt_t error(std::string msg) noexcept {
        scope = std::move(msg);
        return std::nullopt;
    }

    std::nullopt_t error(std::type_index d) noexcept {
        dest = std::move(d);
        return std::nullopt;
    }

    std::nullopt_t error(std::string msg, std::type_index d, int e=-1, int r=-1) noexcept {
        scope = std::move(msg);
        dest = std::move(d);
        expected = e;
        received = r;
        return std::nullopt;
    }

    WrongType exception() && noexcept {
        return {std::move(scope), std::move(indices), std::move(source), std::move(dest), index, expected, received};
    }

    template <class T>
    auto store(T &&t) {
        return std::addressof(storage.emplace_back().emplace<no_qualifier<T>>(static_cast<T &&>(t)));
    }

    Dispatch(Caller c={}, char const *s="mismatched type") : scope(s), caller(std::move(c)) {indices.reserve(8);}
};

/******************************************************************************/

}
