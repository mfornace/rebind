#pragma once
#include "Builtins.h"
#include <ara/Call.h>
#include <mutex>

namespace ara::py {

/******************************************************************************/

/// RAII release of Python GIL
struct PythonFrame final : Frame {
    std::mutex mutex;
    PyThreadState *state = nullptr;
    bool no_gil;

    explicit PythonFrame(bool no_gil) : no_gil(no_gil) {}

    void enter() override { // noexcept
        DUMP("running with nogil=", no_gil);
        // release GIL and save the thread
        if (no_gil && !state) state = PyEval_SaveThread();
    }

    // std::shared_ptr<Frame> new_frame(std::shared_ptr<Frame> t) noexcept override {
    //     DUMP("suspended Python ", bool(t));
    //     // if we already saved the python thread state, return this
    //     if (no_gil || state) return std::move(t);
    //     // return a new frame that can be entered
    //     else return std::make_shared<PythonFrame>(no_gil);
    // }

    // acquire GIL; lock mutex to prevent multiple threads trying to get the thread going
    void acquire() noexcept {if (state) {mutex.lock(); PyEval_RestoreThread(state);}}

    // release GIL; unlock mutex
    void release() noexcept {if (state) {state = PyEval_SaveThread(); mutex.unlock();}}

    // reacquire the GIL and restart the thread, if required
    ~PythonFrame() {if (state) PyEval_RestoreThread(state);}
};

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
    bool value;
    constexpr GIL() : value(true) {}
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