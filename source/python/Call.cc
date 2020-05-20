#include <ara-py/Variable.h>
#include <ara-py/Call.h>
#include <ara-py/Dump.h>
#include <cstdlib>

/******************************************************************************/

namespace ara::py {

Lifetime call_to_variable(Variable &out, Index self, Pointer address, Mode qualifier, ArgView &args) {
    DUMP(out.name(), out.address());
    Target target(Index(), &out.storage, sizeof(out.storage),
        Target::Write | Target::Read | Target::Heap | Target::Trivial |
        Target::Relocatable | Target::MoveNoThrow | Target::Unmovable | Target::MoveThrow
    );

    auto const stat = Call::call(self, target, address, qualifier, args);
    DUMP("Variable got stat and lifetime:", stat, target.c.lifetime);

    switch (stat) {
        case Call::None:        break;
        case Call::Stack:       {out.set_stack(target.index()); break;}
        case Call::Read:        {out.set_heap(target.index(), target.output(), Mode::Read); break;}
        case Call::Write:       {out.set_heap(target.index(), target.output(), Mode::Write); break;}
        case Call::Heap:        {out.set_heap(target.index(), target.output(), Mode::Heap); break;}
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

bool is_subclass(Instance<> o, Instance<PyTypeObject> t) {
    if (t.object() == +o) return true;
    int x = PyObject_IsSubclass(+o, t.object());
    return (x < 0) ? throw PythonError() : x;
}

Lifetime call_to_output(Shared &out, PyObject* maybe_output, Index self, Pointer address, Mode qualifier, ArgView &args) {
    if (!maybe_output) {
        out = Shared::from(c_new<Variable>(+static_type<Variable>(), nullptr, nullptr));
        Variable &v = cast_object<Variable>(+out);
        return call_to_variable(v, self, address, qualifier, args);
    }
    auto output = instance(maybe_output);

    if (auto v = cast_if<Variable>(+output)) {
        DUMP("instance of Variable");
        out = {Py_None, true};
        return call_to_variable(*v, self, address, qualifier, args);
    }

    if (PyType_CheckExact(+output)) {
        DUMP("is type");
        if (is_subclass(output, static_type<Variable>())) {
            DUMP("is Variable subclass");
            auto type = output.as<PyTypeObject>();
            out = Shared::from(c_new<Variable>(+type, nullptr, nullptr));
            Variable &v = cast_object<Variable>(+out);
            return call_to_variable(v, self, address, qualifier, args);
        }
    }
#warning "aliasing is a problem here"
    Variable v;
    call_to_variable(v, self, address, qualifier, args);
    Ref r(v.index(), Mode::Write, Pointer::from(v.address()));
    if (auto o = try_load(r, output, Shared())) {out = o; return {};}
    else throw PythonError(type_error("could not load"));
}


/******************************************************************************/

struct TupleLock {
    ArgView &view;
    Instance<PyTupleObject> args;
    Py_ssize_t start;

    // Prepare a lock on each argument: v[i] <-- a[s+i]
    // The immediate next step after constructor should be either read_lock() or lock()
    TupleLock(ArgView &v, Instance<PyTupleObject> a, Py_ssize_t s=0) noexcept
        : view(v), args(a), start(s) {for (auto &ref : view) ref.c.mode_index = nullptr;}

    void lock_default() {
        auto s = start;
        for (auto &ref : view) {
            PyObject* item = PyTuple_GET_ITEM(args.object(), s++);
            if (auto p = cast_if<Variable>(item)) {
                DUMP("its variable!");
                ref = begin_acquisition(*p, LockType::Read);
            } else {
                ref = Ref(Index::of<Export>(), Mode::Write, Pointer::from(item));
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
            PyObject* item = PyTuple_GET_ITEM(args.object(), s++);
            if (auto p = cast_if<Variable>(item)) {
                DUMP("its variable!", *c);
                ref = begin_acquisition(*p, *c == 'w' ? LockType::Write : LockType::Read);
            } else {
                ref = Ref(Index::of<Export>(), Mode::Write, Pointer::from(item));
            }
            ++c;
        }
    }

    ~TupleLock() noexcept {
        auto s = start;
        for (auto &ref : view) {
            if (ref.has_value())
                if (auto v = cast_if<Variable>(PyTuple_GET_ITEM(args.object(), s)))
                    end_acquisition(*v);
            ++s;
        }
    }
};

// struct SelfTupleLock : TupleLock {
//     Variable &self;
//     SelfTupleLock(Variable &s, ArgView &v, Instance<PyTupleObject> a, std::string_view mode) noexcept : TupleLock(v, a, 1), self(s) {
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

/******************************************************************************/

Shared module_call(Index index, Instance<PyTupleObject> args, CallKeywords const &kws) {
    DUMP("module_call", index.name());
    auto const total = PyTuple_GET_SIZE(args.object());
    if (!total) throw PythonError(type_error("ara call: expected at least one argument"));
    ArgAlloc a(total-1, 1);

    Str name;
    if (auto p = get_unicode(instance(PyTuple_GET_ITEM(args.object(), 0)))) {
        name = from_unicode(instance(p));
    } else throw PythonError(type_error("expected str"));
    a.view.tag(0) = Ref(name);

    for (Py_ssize_t i = 0; i != total-1; ++i) {
        PyObject* item = PyTuple_GET_ITEM(args.object(), i+1);
        a.view[i] = Ref(Index::of<Export>(), Mode::Write, Pointer::from(item));
    }

    auto lk = std::make_shared<PythonFrame>(!kws.gil);
    Caller caller(lk);
    a.view.c.caller_ptr = &caller;

    Shared out;
    auto life = call_to_output(out, kws.out, index, Pointer::from(nullptr), Mode::Read, a.view);
    return out;
}

/******************************************************************************/

Shared variable_call(Variable &v, Instance<PyTupleObject> args, CallKeywords const &kws) {
    DUMP("variable_call", v.name());
    auto const total = PyTuple_GET_SIZE(args.object());
    ArgAlloc a(total, 0);

    for (Py_ssize_t i = 0; i != total; ++i) {
        PyObject* item = PyTuple_GET_ITEM(args.object(), i);
        a.view[i] = Ref(Index::of<Export>(), Mode::Write, Pointer::from(item));
    }

    auto lk = std::make_shared<PythonFrame>(!kws.gil);
    Caller caller(lk);
    a.view.c.caller_ptr = &caller;

    Shared out;
    auto life = call_to_output(out, kws.out, v.index(), Pointer::from(v.address()), Mode::Read, a.view);
    return out;
}

/******************************************************************************/

char remove_mode(std::string_view &mode) {
    char const first = mode.empty() ? 'r' : mode[0];
    mode.remove_prefix(std::min(mode.size(), std::size_t(2)));
    return first;
}

Shared variable_method(Variable &v, Instance<> pyself, Instance<PyTupleObject> args, CallKeywords &&kws) {
    DUMP("variable_method", v.name());
    auto const total = PyTuple_GET_SIZE(args.object());
    ArgAlloc a(total-1, 1);

    Str name;
    if (auto p = get_unicode(instance(PyTuple_GET_ITEM(args.object(), 0)))) {
        name = from_unicode(instance(p));
        // name.data = s.data();
        // name.size = s.size();
    } else throw PythonError(type_error("expected str"));
    a.view.tag(0) = Ref(name);

    DUMP("mode", kws.mode);
    Shared out;
    Lifetime life;
    {
        char self_mode = remove_mode(kws.mode);
        auto self = acquire_ref(v, self_mode == 'w' ? LockType::Write : LockType::Read);
        TupleLock locking(a.view, args, 1);
        locking.lock(kws.mode);

        auto lk = std::make_shared<PythonFrame>(!kws.gil);
        Caller caller(lk);
        a.view.c.caller_ptr = &caller;

        life = call_to_output(out, kws.out, self.ref.index(), self.ref.pointer(), self.ref.tag(), a.view);
    }
    DUMP("lifetime to attach = ", life.value);
    if (life.value) {
        if (auto o = cast_if<Variable>(+out)) {
            for (unsigned i = 0; life.value; ++i) {
                if (life.value & 1) {
                    DUMP("got one", i);
                    if (i) {
                        DUMP("setting root to argument", i-1, PyTuple_GET_SIZE(args.object()));
                        auto arg = instance(PyTuple_GET_ITEM(args.object(), i));
                        o->set_lock(cast_object<Variable>(+arg).current_root(arg));
                    } else {
                        DUMP("setting root to self");
                        o->set_lock(v.current_root(pyself));
                    }
                }
                life.value >>= 1;
            }
        }
    }
    return out;
}

/******************************************************************************/

PyObject* c_variable_call(PyObject* self, PyObject* args, PyObject* kws) noexcept {
    return raw_object([=] {
        return variable_call(cast_object<Variable>(self),
            instance(reinterpret_cast<PyTupleObject *>(args)), CallKeywords(kws));
    });
}

PyObject* c_variable_method(PyObject* self, PyObject* args, PyObject* kws) noexcept {
    return raw_object([=] {
        return variable_method(cast_object<Variable>(self), instance(self),
            instance(reinterpret_cast<PyTupleObject *>(args)), CallKeywords(kws));
    });
}

/******************************************************************************/

}
