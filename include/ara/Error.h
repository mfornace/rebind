#pragma once
#include "Index.h"
#include <exception>
#include <string_view>
#include <string>

namespace ara {

/******************************************************************************/

struct InvalidStatus : std::exception {
    char const *message;
    Stat status;

    InvalidStatus(char const *m, Stat s) : message(m), status(s) {}
    char const * what() const noexcept override {return message;}
};

/******************************************************************************/

/// Exception for something the API user caused
struct PreconditionError : std::logic_error {
    using std::logic_error::logic_error;
};

/******************************************************************************/

struct NotImplemented : PreconditionError {
    Code code;
    NotImplemented(std::string_view s, Code c) : PreconditionError(std::string(s)), code(c) {}
};

/******************************************************************************/

struct WrongReturn : PreconditionError {
    using PreconditionError::PreconditionError;
    // WrongReturn(std::string_view s, )
};

/******************************************************************************/

/// Exception for wrong number of arguments
struct WrongNumber : PreconditionError {
    unsigned int expected, received;

    WrongNumber(unsigned int n0, unsigned int n)
        : PreconditionError("wrong number of arguments"), expected(n0), received(n) {}
};

/******************************************************************************/

/// Exception for wrong type of an argument
struct WrongType : PreconditionError {
    // std::vector<unsigned int> indices;
    std::string source;
    Index dest;
    int index = -1, expected = -1, received = -1;

    using PreconditionError::PreconditionError;
};

/******************************************************************************/

#warning "cleanup"
struct Failure : std::exception {
    Index idx;
    void *ptr;
    Failure(Index i, void *p) : idx(i), ptr(p) {}

    Failure(Failure const &) = delete;
    Failure &operator=(Failure const &) = delete;

    Failure(Failure &&e) noexcept : idx(std::exchange(e.idx, Index())), ptr(e.ptr) {}

    Failure &operator=(Failure &&e) noexcept {idx = std::exchange(e.idx, Index()); return *this;}

    ~Failure() {
        if (idx) Destruct::call(idx, Pointer::from(ptr), Destruct::Heap);
    }
};

/******************************************************************************/

}