#pragma once
#include "Builtins.h"
#include <ara/Call.h>
#include <mutex>

namespace ara::py {

/******************************************************************************/

struct Modes {
    std::string_view value;

    explicit Modes(Maybe<pyStr> s) {
        if (s) value = view_underlying(*s);
    }

    char pop_first() {
        if (value.empty()) return 'r';
        char const first = value[0];
        value.remove_prefix(std::min(value.size(), std::size_t(2)));
        return first;
    }
};

struct Out {
    Maybe<> value;
};

struct GIL {
    bool value = true;
    constexpr GIL() = default;
    explicit constexpr GIL(bool v) : value(v) {};
    explicit GIL(Maybe<> s) : value(s ? PyObject_IsTrue(~s) : true) {}
};

struct Tag {
    Maybe<> value;
};

/******************************************************************************/

struct ArgAlloc {
    ArgView &view;

    static void* allocate(std::size_t n) {
        using namespace std;
        static_assert(alignof(ArgStack<0, 1>) <= alignof(ara_ref));
        std::size_t const size = sizeof(ArgStack<0, 1>) - sizeof(ara_ref) + n * sizeof(ara_ref);
        return aligned_alloc(alignof(ArgStack<0, 1>), size);
    }

    ArgAlloc(std::uint32_t args, std::uint32_t tags)
        : view(*static_cast<ArgView *>(allocate(args + tags))) {
        view.c.args = args;
        view.c.tags = tags;
    }

    ~ArgAlloc() noexcept {std::free(&view);}
};

/******************************************************************************/

}
