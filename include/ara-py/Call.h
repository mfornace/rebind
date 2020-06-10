#pragma once
#include "Raw.h"
#include "Variable.h"
#include "Dump.h"
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
            throw PythonError::type("ara.Variable: unexpected keyword arguments: %R", +Value<>::from(PyDict_Keys(~kws)));
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

Value<> module_call(ara::Index index, Always<pyTuple> args, CallKeywords const &);

// template <class Module>
// PyObject* c_module_call(PyObject* self, PyObject* args, PyObject* kws) noexcept {
//     return raw_object([args, kws] {
//         return module_call(Switch<Module>::call,
//             instance(*args).as<PyTupleObject>(), CallKeywords(kws));
//     });
// }

/******************************************************************************/

Target variable_target(Variable& out) {
    return {Index(), &out.storage, sizeof(out.storage),
        Target::Write | Target::Read | Target::Heap | Target::Trivial |
        Target::Relocatable | Target::MoveNoThrow | Target::Unmovable | Target::MoveThrow};
}

Lifetime invoke_call(Variable &out, Index self, Pointer address, ArgView &args, Mode mode) {
    Target target = variable_target(out);
    auto const stat = Call::call(self, target, address, mode, args);
    DUMP("Variable got stat and lifetime:", stat, target.c.lifetime);

    switch (stat) {
        case Call::None:        break;
        case Call::Stack:       {out.set_stack(target.index()); break;}
        case Call::Read:        {out.set_pointer(target.index(), target.output(), Mode::Read); break;}
        case Call::Write:       {out.set_pointer(target.index(), target.output(), Mode::Write); break;}
        case Call::Heap:        {out.set_pointer(target.index(), target.output(), Mode::Heap); break;}
        case Call::Impossible:  {throw PythonError(type_error("Impossible"));}
        case Call::WrongNumber: {throw PythonError(type_error("WrongNumber"));}
        case Call::WrongType:   {throw PythonError(type_error("WrongType"));}
        case Call::WrongReturn: {throw PythonError(type_error("WrongReturn"));}
        case Call::Exception:   {throw PythonError(type_error("Exception"));}
        case Call::OutOfMemory: {throw std::bad_alloc();}
    }
    DUMP("output:", out.name(), "address:", out.address(), target.c.lifetime, target.lifetime().value);
    return target.lifetime();
}

/******************************************************************************/

template <class I>
Lifetime invoke_access(Variable &out, Index self, Pointer address, I element, Mode mode) {
    Target target = variable_target(out);
    using A = std::conditional_t<std::is_same_v<I, Integer>, Element, Attribute>;
    auto const stat = A::call(self, target, address, element, mode);
    DUMP("Variable got stat and lifetime:", stat, target.c.lifetime);

    switch (stat) {
        case A::None:        break;
        case A::Stack:       {out.set_stack(target.index()); break;}
        case A::Read:        {out.set_pointer(target.index(), target.output(), Mode::Read); break;}
        case A::Write:       {out.set_pointer(target.index(), target.output(), Mode::Write); break;}
        case A::Heap:        {out.set_pointer(target.index(), target.output(), Mode::Heap); break;}
        case A::Exception:   {throw PythonError(type_error("Exception"));}
        case A::OutOfMemory: {throw std::bad_alloc();}
    }
    DUMP("output:", out.name(), "address:", out.address(), target.c.lifetime, target.lifetime().value);
    return target.lifetime();
}

/******************************************************************************/


template <class F>
Lifetime call_to_output(Value<> &out, Always<> output, F&& fun) {
    if (auto v = Maybe<Variable>(output)) {
        DUMP("instance of Variable");
        out = {Py_None, true};
        return fun(*v);
    }

    if (auto t = Maybe<pyType>(output)) {
        DUMP("is type");
        if (is_subclass(*t, Variable::def())) {
            DUMP("is Variable subclass");
            auto v = Value<Variable>::alloc();
            auto life = fun(*v);
            out = std::move(v);
            return life;
        }
    }
#warning "aliasing is a problem here"
    auto v = Value<Variable>::alloc();
    fun(*v);
    Ref r(v->index(), Mode::Write, Pointer::from(v->address()));
    if (auto o = try_load(r, output, Value<>())) {out = o; return {};}
    else throw PythonError(type_error("could not load"));
}

template <class F>
Lifetime call_to_output(Value<> &out, Maybe<> output, F&& fun) {
    if (output) {
        return call_to_output(out, *output, fun);
    } else {
        auto v = Value<Variable>::alloc();
        auto life = fun(*v);
        out = std::move(v);
        return life;
    }
}

/******************************************************************************/

struct TupleLock {
    ArgView &view;
    Always<pyTuple> args;
    Py_ssize_t start;

    // Prepare a lock on each argument: v[i] <-- a[s+i]
    // The immediate next step after constructor should be either read_lock() or lock()
    TupleLock(ArgView &v, Always<pyTuple> a, Py_ssize_t s=0) noexcept
        : view(v), args(a), start(s) {for (auto &ref : view) ref.c.mode_index = nullptr;}

    void lock_default() {
        auto s = start;
        for (auto &ref : view) {
            Always<> it = item(args, s++);
            if (auto p = Maybe<Variable>(it)) {
                DUMP("its variable!");
                ref = begin_acquisition(*p, LockType::Read);
            } else {
                ref = Ref(Index::of<Export>(), Mode::Write, Pointer::from(+it));
            }
        }
    }

    void lock(std::string_view mode) {
        if (mode.empty())
            return lock_default();
        if (mode.size() != view.size())
            throw PythonError(type_error("wrong number of modes"));

        auto s = start;
        auto c = mode.begin();
        for (auto &ref : view) {
            Always<> it = item(args, s++);
            if (auto p = Maybe<Variable>(it)) {
                DUMP("its variable!", *c);
                ref = begin_acquisition(*p, *c == 'w' ? LockType::Write : LockType::Read);
            } else {
                ref = Ref(Index::of<Export>(), Mode::Write, Pointer::from(+it));
            }
            ++c;
        }
    }

    ~TupleLock() noexcept {
        auto s = start;
        for (auto &ref : view) {
            if (ref.has_value())
                if (auto v = Maybe<Variable>(item(args, s)))
                    end_acquisition(*v);
            ++s;
        }
    }
};

// struct SelfTupleLock : TupleLock {
//     Variable &self;
//     SelfTupleLock(Variable &s, ArgView &v, Always<pyTuple> a, std::string_view mode) noexcept : TupleLock(v, a, 1), self(s) {
//         char const first = mode.empty() ? 'r' : mode[0];
//         begin_acquisition(self, first == 'w', first == 'r');
//     }

//     void lock(std::string_view mode) {
//         if (mode.size() > 2) {
//            mode.remove_prefix(std::min(mode.size(), std::size_t(2)));
//             if (mode.size() != view.size())
//                 throw PythonError(type_error("wrong number of modes"));
//             lock_arguments(mode);
//         } else {
//             lock_arguments();
//         }
//     }

//     ~SelfTupleLock() noexcept {end_acquisition(self);}
// };

char remove_mode(std::string_view &mode) {
    char const first = mode.empty() ? 'r' : mode[0];
    mode.remove_prefix(std::min(mode.size(), std::size_t(2)));
    return first;
}

/******************************************************************************/

auto call_with_caller(Index self, Pointer address, Mode mode, ArgView& args, CallKeywords const& kws) {
    auto lk = std::make_shared<PythonFrame>(!kws.gil);
    Caller caller(lk);
    args.c.caller_ptr = &caller;

    Value<> out;
    auto life = call_to_output(out, kws.out, [&](Variable& v){
        return invoke_call(v, self, address, args, mode);
    });
    return std::make_pair(out, life);
}

/******************************************************************************/

Value<> module_call(Index index, Always<pyTuple> args, CallKeywords const& kws) {
    DUMP("module_call", index.name());
    auto const total = size(args);
    if (!total) throw PythonError(type_error("ara call: expected at least one argument"));
    ArgAlloc a(total-1, 1);

    Str name = as_string_view(item(args, 0));
    a.view.tag(0) = Ref(name);

    for (Py_ssize_t i = 0; i != total-1; ++i)
        a.view[i] = Ref(Index::of<Export>(), Mode::Write, Pointer::from(+item(args, i+1)));

    return call_with_caller(index, Pointer::from(nullptr), Mode::Read, a.view, kws).first;
}

/******************************************************************************/

Value<> variable_call(Variable &v, Always<pyTuple> args, CallKeywords const& kws) {
    DUMP("variable_call", v.name());
    auto const total = size(args);
    ArgAlloc a(total, 0);

    for (Py_ssize_t i = 0; i != total; ++i)
        a.view[i] = Ref(Index::of<Export>(), Mode::Write, Pointer::from(+item(args, i)));

    return call_with_caller(v.index(), Pointer::from(v.address()), Mode::Read, a.view, kws).first;
}

/******************************************************************************/

Value<> variable_method(Always<Variable> v, Always<pyTuple> args, CallKeywords &&kws) {
    DUMP("variable_method", v->name());
    auto const total = size(args);
    ArgAlloc a(total-1, 1);

    Str name = as_string_view(item(args, 0));
    a.view.tag(0) = Ref(name);

    DUMP("mode", kws.mode);
    Value<> out;
    Lifetime life;
    {
        char self_mode = remove_mode(kws.mode);
        auto self = acquire_ref(v, self_mode == 'w' ? LockType::Write : LockType::Read);
        TupleLock locking(a.view, args, 1);
        locking.lock(kws.mode);

        std::tie(out, life) = call_with_caller(self.ref.index(), self.ref.pointer(), self.ref.mode(), a.view, kws);
    }
    DUMP("lifetime to attach = ", life.value);
    if (life.value) {
        if (auto o = Maybe<Variable>(out)) {
            for (unsigned i = 0; life.value; ++i) {
                if (life.value & 1) {
                    DUMP("got one", i);
                    if (i) {
                        DUMP("setting root to argument", i-1, size(args));
                        if (auto arg = Maybe<Variable>(item(args, i))) {
                            o->set_lock(current_root(*arg));
                        } else throw PythonError::type("Expected instance of Variable");
                    } else {
                        DUMP("setting root to self");
                        o->set_lock(current_root(v));
                    }
                }
                life.value >>= 1;
            }
        }
    }
    return out;
}

/******************************************************************************/

template <class I>
auto access_with_caller(Index self, Pointer address, I element, Mode mode, CallKeywords const& kws) {
    Value<> out;
    auto life = call_to_output(out, kws.out, [&](Variable& v) {
        return invoke_access(v, self, address, element, mode);
    });
    return std::make_pair(out, life);
}

template <class I>
Value<> variable_access(Always<Variable> v, Always<pyTuple> args, CallKeywords &&kws) {
    DUMP("variable_attr", v->name());
    auto element = exact_cast(item(args, 0), Type<I>());

    DUMP("mode", kws.mode);
    Value<> out;
    Lifetime life;
    {
        char self_mode = remove_mode(kws.mode);
        auto self = acquire_ref(v, self_mode == 'w' ? LockType::Write : LockType::Read);
        std::tie(out, life) = access_with_caller(self.ref.index(), self.ref.pointer(), element, self.ref.mode(), kws);
    }
    DUMP("lifetime to attach = ", life.value);
    if (life.value) {
        if (auto o = Maybe<Variable>(out)) {
            DUMP("setting root to self");
            o->set_lock(current_root(v));
        }
    }
    return out;
}

/******************************************************************************/

PyObject* c_variable_attribute(PyObject* self, PyObject*args, PyObject* kws) noexcept {
    return nullptr;
    // return raw_object([=] {
    //     return variable_access<Str>(cast_object<Variable>(self), *self,
    //         *reinterpret_cast<PyTupleObject *>(args), CallKeywords(kws));
    // });
}

PyObject* c_variable_element(PyObject* self, PyObject*args, PyObject* kws) noexcept {
    return nullptr;
    // return raw_object([=] {
    //     return variable_access<Integer>(cast_object<Variable>(self), *self,
    //         *reinterpret_cast<PyTupleObject *>(args), CallKeywords(kws));
    // });
}

PyObject* c_variable_call(PyObject* self, PyObject* args, PyObject* kws) noexcept {
    return nullptr;
    // return raw_object([=] {
    //     return variable_call(cast_object<Variable>(self),
    //         *reinterpret_cast<PyTupleObject *>(args), CallKeywords(kws));
    // });
}

PyObject* c_variable_method(PyObject* self, PyObject* args, PyObject* kws) noexcept {
    return nullptr;
    // return raw_object([=] {
    //     return variable_method(cast_object<Variable>(self), *self,
    //         *reinterpret_cast<PyTupleObject *>(args), CallKeywords(kws));
    // });
}

/******************************************************************************/


}