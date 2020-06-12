#pragma once
#include "Load.h"
#include <ara/Call.h>
#include <mutex>

namespace ara::py {

/******************************************************************************/

// template <class T>
// PyCFunction c_function(T t) {
//     if constexpr(std::is_constructible_v<PyCFunction, T>) return static_cast<PyCFunction>(t);
//     else return reinterpret_cast<PyCFunction>(static_cast<PyCFunctionWithKeywords>(t));
// }

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
    Maybe<> out = nullptr;
    Maybe<> tags = nullptr;
    bool gil = true;

    explicit CallKeywords(Maybe<pyDict> kws) {
        if (!kws) return;
        out = PyDict_GetItemString(~kws, "out");
        tags = PyDict_GetItemString(~kws, "tags");

        Py_ssize_t n = 0;
        if (tags) ++n;
        if (out) ++n;

        if (auto g = PyDict_GetItemString(~kws, "gil")) {
            gil = PyObject_IsTrue(g);
            ++n;
        }

        if (auto r = PyDict_GetItemString(~kws, "mode")) {
            mode = as_string_view(*r);
            ++n;
        }

        if (n != PyDict_Size(~kws)) {
            PyDict_DelItemString(~kws, "tag");
            PyDict_DelItemString(~kws, "out");
            PyDict_DelItemString(~kws, "mode");
            PyDict_DelItemString(~kws, "gil");
            throw PythonError::type("ara.Variable: unexpected keyword arguments: %R", +Value<>::take(PyDict_Keys(~kws)));
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


/******************************************************************************/

// PyObject* c_variable_attribute(PyObject* self, PyObject*args, PyObject* kws) noexcept {
//     return nullptr;
//     // return raw_object([=] {
//     //     return variable_access<Str>(cast_object<Variable>(self), *self,
//     //         *reinterpret_cast<PyTupleObject *>(args), CallKeywords(kws));
//     // });
// }

// PyObject* c_variable_element(PyObject* self, PyObject*args, PyObject* kws) noexcept {
//     return nullptr;
//     // return raw_object([=] {
//     //     return variable_access<Integer>(cast_object<Variable>(self), *self,
//     //         *reinterpret_cast<PyTupleObject *>(args), CallKeywords(kws));
//     // });
// }

// PyObject* c_variable_call(PyObject* self, PyObject* args, PyObject* kws) noexcept {
//     return nullptr;
//     // return raw_object([=] {
//     //     return variable_call(cast_object<Variable>(self),
//     //         *reinterpret_cast<PyTupleObject *>(args), CallKeywords(kws));
//     // });
// }

// PyObject* c_variable_method(PyObject* self, PyObject* args, PyObject* kws) noexcept {
//     return nullptr;
//     // return raw_object([=] {
//     //     return variable_method(cast_object<Variable>(self), *self,
//     //         *reinterpret_cast<PyTupleObject *>(args), CallKeywords(kws));
//     // });
// }

/******************************************************************************/


}