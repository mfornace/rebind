#pragma once
#include "Raw.h"
#include "Variable.h"
#include "Methods.h"
#include "Dump.h"
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

struct CallKeywords {
    std::string_view mode;
    Instance<> out;
    PyObject* tags;
    bool gil = true;

    CallKeywords(Instance<PyDictObject> kws) :
        out(PyDict_GetItemString(kws.object(), "out")),
        tags(PyDict_GetItemString(kws.object(), "tags")) {
        if (auto g = PyDict_GetItemString(kws.object(), "gil")) {
            gil = PyObject_IsTrue(g);
        }

        if (auto r = PyDict_GetItemString(kws.object(), "mode")) {
            if (auto p = get_unicode(instance(r))) mode = from_unicode(instance(p));
            else throw PythonError(type_error("Expected str"));
        }
    }
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

Shared module_call(Index index, Instance<PyTupleObject> args, CallKeywords const &);

template <class Module>
PyObject* c_module_call(PyObject* self, PyObject* args, PyObject* kws) noexcept {
    if (!kws) return type_error("expected keywords");
    return raw_object([args, kws] {
        return module_call(impl<Module>::call,
            instance(reinterpret_cast<PyTupleObject *>(args)),
            instance(reinterpret_cast<PyDictObject *>(kws)));
    });
}

PyObject* c_variable_call(PyObject* self, PyObject* args, PyObject* kws) noexcept;

PyObject* c_variable_method(PyObject* self, PyObject* args, PyObject* kws) noexcept;

/******************************************************************************/

}