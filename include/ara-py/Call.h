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

Shared module_call(Index index, Instance<PyTupleObject> args);

template <class Module>
PyObject* c_module_call(PyObject* self, PyObject* args) noexcept {
    return raw_object([args] {
        return module_call(impl<Module>::call, instance(reinterpret_cast<PyTupleObject *>(args)));
    });
}

PyObject* c_variable_call(PyObject* self, PyObject* args, PyObject* kws) noexcept;

/******************************************************************************/

}