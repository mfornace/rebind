#include <ara-py/Variable.h>
#include <ara-py/Call.h>
#include <ara-py/Dump.h>
#include <cstdlib>

/******************************************************************************/

namespace ara::py {

void call_to_variable(Variable &out, Index self, Pointer address, Tag qualifier, ArgView &args) {
    DUMP(out.name(), out.address());
    auto target = Target::from(Index(), &out.storage, sizeof(out.storage), Target::Stack);

    auto const stat = Call::call(self, target, address, qualifier, args);
    DUMP("Variable got stat:", stat, target.c.lifetime);
    Shared lock;

    switch (stat) {
        case Call::None:        break;
        case Call::Stack:       {out.set_stack(target.index(), lock); break;}
        case Call::Const:       {out.set_heap(target.index(), target.output(), Tag::Const, lock); break;}
        case Call::Mutable:     {out.set_heap(target.index(), target.output(), Tag::Mutable, lock); break;}
        case Call::Heap:        {out.set_heap(target.index(), target.output(), Tag::Heap, lock); break;}
#warning "implement these"
        case Call::Impossible:  {throw PythonError(type_error("Impossible"));}
        case Call::WrongNumber: {DUMP("hmm"); throw PythonError(type_error("WrongNumber"));}
        case Call::WrongType:   {throw PythonError(type_error("WrongType"));}
        case Call::WrongReturn: {throw PythonError(type_error("WrongReturn"));}
        case Call::Exception:   {throw PythonError(type_error("Exception"));}
        case Call::OutOfMemory: {throw std::bad_alloc();}
    }
    DUMP("output:", out.name(), "address:", out.address());
    // return o;
}

/******************************************************************************/

bool is_subclass(Instance<> o, Instance<PyTypeObject> t) {
    if (t.object() == +o) return true;
    int x = PyObject_IsSubclass(+o, t.object());
    return (x < 0) ? throw PythonError() : x;
}

Shared call_to_output(Instance<> output, Index self, Pointer address, Tag qualifier, ArgView &args) {
    if (auto v = cast_if<Variable>(+output)) {
        DUMP("instance of Variable");
        call_to_variable(*v, self, address, qualifier, args);
        return {Py_None, true};
    }

    if (PyType_CheckExact(+output)) {
        DUMP("is type");
        if (is_subclass(output, static_type<Variable>())) {
            DUMP("is Variable subclass");
            auto type = output.as<PyTypeObject>();
            auto obj = Shared::from(c_new<Variable>(+type, nullptr, nullptr));
            Variable &v = cast_object<Variable>(+obj);
            call_to_variable(v, self, address, qualifier, args);
            return obj;
        }
        return type_error("Output type is not handled: %R", +output);
    }
    return type_error("Bad");
}

/******************************************************************************/

Shared module_call(Index index, Instance<PyTupleObject> args, CallKeywords const &kws) {
    DUMP("module_call", index.name());
    auto const total = PyTuple_GET_SIZE(args.object());
    if (!total) throw PythonError(type_error("ara call: expected at least one argument"));
    ArgAlloc a(total-1, 1);

    std::string_view name;
    if (auto p = get_unicode(instance(PyTuple_GET_ITEM(args.object(), 0)))) {
        name = from_unicode(instance(p));
    } else throw PythonError(type_error("expected str"));
    a.view.tag(0) = Ref::from_existing(name);

    for (Py_ssize_t i = 0; i != total-1; ++i) {
        PyObject* item = PyTuple_GET_ITEM(args.object(), i+1);
        a.view[i] = Ref::from_existing(Index::of<Export>(), Pointer::from(item), true);
    }

    auto lk = std::make_shared<PythonFrame>(!kws.gil);
    Caller caller(lk);
    a.view.c.caller_ptr = &caller;

    return call_to_output(kws.out, index, Pointer::from(nullptr), Tag::Const, a.view);
}

/******************************************************************************/

Shared variable_call(Variable &v, Instance<PyTupleObject> args, CallKeywords const &kws) {
    DUMP("variable_call", v.name());
    auto const total = PyTuple_GET_SIZE(args.object());
    ArgAlloc a(total, 0);

    for (Py_ssize_t i = 0; i != total; ++i) {
        PyObject* item = PyTuple_GET_ITEM(args.object(), i);
        a.view[i] = Ref::from_existing(Index::of<Export>(), Pointer::from(item), true);
    }

    auto lk = std::make_shared<PythonFrame>(!kws.gil);
    Caller caller(lk);
    a.view.c.caller_ptr = &caller;

    return call_to_output(kws.out, v.index(), Pointer::from(v.address()), Tag::Const, a.view);
}

/******************************************************************************/

struct ArgTupleLock {
    ArgView &view;
    Instance<PyTupleObject> args;
    Py_ssize_t start;

    ArgTupleLock(ArgView &v, Instance<PyTupleObject> a, Py_ssize_t s=0)
        : view(v), args(a), start(s) {for (auto &ref : view) ref.c.tag_index = nullptr;}

    void read_lock() {
        auto s = start;
        for (auto &ref : view) {
            PyObject* item = PyTuple_GET_ITEM(args.object(), s++);
            if (auto p = cast_if<Variable>(item)) {
                DUMP("its variable!");
                ref = begin_acquisition(*p, false, true);
            } else {
                ref = Ref::from_existing(Index::of<Export>(), Pointer::from(item), true);
            }
        }
    }

    void lock(std::string_view mode) {
        auto s = start;
        auto c = mode.begin();
        for (auto &ref : view) {
            PyObject* item = PyTuple_GET_ITEM(args.object(), s++);
            if (auto p = cast_if<Variable>(item)) {
                DUMP("its variable!", *c);
                ref = begin_acquisition(*p, *c == 'w', *c == 'r');
            } else {
                ref = Ref::from_existing(Index::of<Export>(), Pointer::from(item), true);
            }
            ++c;
        }
    }

    ~ArgTupleLock() noexcept {
        auto s = start;
        for (auto &ref : view) {
            if (ref.has_value())
                if (auto v = cast_if<Variable>(PyTuple_GET_ITEM(args.object(), s)))
                    end_acquisition(*v);
            ++s;
        }
    }
};

Shared variable_method(Variable &v, Instance<PyTupleObject> args, CallKeywords &&kws) {
    DUMP("variable_method", v.name());
    auto const total = PyTuple_GET_SIZE(args.object());
    ArgAlloc a(total-1, 1);

    std::string_view name;
    if (auto p = get_unicode(instance(PyTuple_GET_ITEM(args.object(), 0)))) {
        name = from_unicode(instance(p));
    } else throw PythonError(type_error("expected str"));
    a.view.tag(0) = Ref::from_existing(name);

    DUMP("mode", kws.mode);
    // also need to lock on v and maybe output ...
    char const first = kws.mode.empty() ? 'r' : kws.mode[0];
    auto self = acquire_ref(v, first == 'w', first == 'r');

    ArgTupleLock locks(a.view, args, 1);

    if (kws.mode.size() > 2) {
        kws.mode.remove_prefix(std::min(kws.mode.size(), std::size_t(2)));
        if (kws.mode.size() != a.view.size())
            throw PythonError(type_error("wrong number of modes"));
        locks.lock(kws.mode);
    } else {
        locks.read_lock();
    }

    auto lk = std::make_shared<PythonFrame>(!kws.gil);
    Caller caller(lk);
    a.view.c.caller_ptr = &caller;

    return call_to_output(kws.out, self.ref.index(), self.ref.pointer(), self.ref.tag(), a.view);
}

/******************************************************************************/

PyObject* c_variable_call(PyObject* self, PyObject* args, PyObject* kws) noexcept {
    if (!kws) return type_error("expected keywords");
    return raw_object([=] {
        return variable_call(cast_object<Variable>(self),
            instance(reinterpret_cast<PyTupleObject *>(args)),
            instance(reinterpret_cast<PyDictObject *>(kws)));
    });
}

PyObject* c_variable_method(PyObject* self, PyObject* args, PyObject* kws) noexcept {
    if (!kws) return type_error("expected keywords");
    return raw_object([=] {
        return variable_method(cast_object<Variable>(self),
            instance(reinterpret_cast<PyTupleObject *>(args)),
            instance(reinterpret_cast<PyDictObject *>(kws)));
    });
}

/******************************************************************************/

}
