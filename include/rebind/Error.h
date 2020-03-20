#pragma once
#include "Signature.h"
#include "Common.h"

#include <stdexcept>
#include <string_view>
#include <vector>
#include <string>
#include <any>
#include <deque>
#include <optional>
#include <variant>

namespace rebind {

/******************************************************************************/

/// Exception for something the API user caused
struct ClientError : std::exception {
    std::string_view message;
    explicit ClientError(std::string_view const &s) noexcept : message(s) {}
    char const * what() const noexcept override {return message.empty() ? "rebind::ClientError" : message.data();}
};

/******************************************************************************/

struct DispatchError : std::invalid_argument {
    using std::invalid_argument::invalid_argument;
};

/******************************************************************************/

/// Exception for wrong number of arguments
struct WrongNumber : DispatchError {
    unsigned int expected, received;
    WrongNumber(unsigned int n0, unsigned int n)
        : DispatchError("wrong number of arguments"), expected(n0), received(n) {}
};

/******************************************************************************/

/// Exception for wrong type of an argument
struct WrongType : DispatchError {
    std::vector<unsigned int> indices;
    std::string source;
    Index dest;
    int index = -1, expected = -1, received = -1;

    WrongType(std::string_view const &n) : DispatchError(n.data()) {}
};

/******************************************************************************/

struct CallingScope {
    // Caller information within a function call
    Caller caller;

    // Lifetime extension for function calls
    std::deque<std::any> storage; // deque so references don't go bad when doing emplace_back()

    explicit CallingScope(Caller c) : caller(std::move(c)) {}

    /// Store a value which will last the lifetime of a conversion from_ref. Return its address
    template <class T>
    unqualified<T> * emplace(T &&t) {
        return std::addressof(storage.emplace_back().emplace<unqualified<T>>(static_cast<T &&>(t)));
    }
};

/******************************************************************************/

struct Scope {
    std::variant<std::monostate, CallingScope, WrongType> content;

    std::vector<unsigned int> indices;

    Scope() noexcept = default;
    explicit Scope(Caller c) noexcept : content(std::in_place_type<CallingScope>, std::move(c)) {}

    WrongType & set_error(std::string_view s) {
        if (auto p = std::get_if<WrongType>(&content)) return *p;
        else return content.emplace<WrongType>(s);
    }

    /// Set error information and return std::nullopt for convenience
    std::nullopt_t error(std::string_view s="mismatched type") noexcept {
        set_error(s);
        return std::nullopt;
    }

    /// Set error information and return std::nullopt for convenience
    std::nullopt_t error(Index d) noexcept {
        auto &err = set_error("mismatched type");
        err.dest = std::move(d);
        return std::nullopt;
    }

    /// Set error information and return std::nullopt for convenience
    std::nullopt_t error(std::string_view msg, Index d, int e=-1, int r=-1) noexcept {
        auto &err = set_error(msg);
        err.dest = std::move(d);
        err.expected = e;
        err.received = r;
        return std::nullopt;
    }

    CallingScope * scope() {return std::get_if<CallingScope>(&content);}
};

/******************************************************************************/

}
