#pragma once
#include "Raw.h"
#include "Variable.h"
#include "Methods.h"
#include <ara/Call.h>
#include <mutex>

namespace ara::py {

/******************************************************************************/

template <class T>
PyCFunction c_function(T t) {
    if constexpr(std::is_constructible_v<PyCFunction, T>) return static_cast<PyCFunction>(t);
    else return reinterpret_cast<PyCFunction>(static_cast<PyCFunctionWithKeywords>(t));
}

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

std::size_t args_allocation_size(std::size_t n) {
    static_assert(sizeof(ArgStack<0, 1>) % sizeof(void*) == 0);
    static_assert(sizeof(ara_ref) % sizeof(void*) == 0);
    static_assert(sizeof(ara_ref) % sizeof(void*) == 0);
    static_assert(alignof(ArgStack<0, 1>) <= sizeof(void*));
    return (sizeof(ArgStack<0, 1>) - sizeof(ara_ref) + n * sizeof(ara_ref)) / sizeof(void*);
}

/******************************************************************************/

Object module_call(Index index, PyObject *args) {
    // args[0] is the type
    auto const total = PyTuple_GET_SIZE(args);
    if (total < 2) return type_error("ara call: expected at least two arguments");
    auto const size = total - 1;

    DUMP("create ArgView from args");
    auto alloc = std::make_unique<void*[]>(args_allocation_size(size));
    auto &view = *reinterpret_cast<ArgView *>(alloc.get());
    view.args = size;
    Py_ssize_t i = 1;
    for (auto &arg : view) {arg = Ref(Ptr(PyTuple_GET_ITEM(args, i))); ++i;}

    DUMP("create Caller");
    bool gil = true;
    auto lk = std::make_shared<PythonFrame>(!gil);
    Caller caller(lk);
    view.caller_ptr = &caller;

    DUMP("create Target");
    PyObject *out = PyTuple_GET_ITEM(args, 0);

    return call_to_variable(index, nullptr, Tag::Const, view);
}

template <class Module>
PyObject* c_call(PyObject *self, PyObject *args) noexcept {
    return raw_object([args] {
        return module_call(impl<Module>::call, args);
    });
}

/******************************************************************************/

}