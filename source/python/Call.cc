#include <ara-py/Variable.h>
#include <ara-py/Call.h>
#include <ara-py/Dump.h>

/******************************************************************************/

namespace ara::py {

void call_to_variable(Variable &out, Index self, Pointer address, Tag qualifier, ArgView &args) {
    // Shared o = Variable::new_object();
    // Variable &out = cast_object<Variable>(+o);
    DUMP(out.name(), out.address());
    auto target = Target::from(Index(), &out.storage, sizeof(out.storage), Target::Stack);

    auto const stat = Call::call(self, target, address, qualifier, args);
    DUMP("Variable got stat:", stat);

    switch (stat) {
        case Call::None:        break;
        case Call::Stack:       {out.idx = Tagged(target.index(), Variable::Stack);    break;}
        case Call::Const:       {out.idx = Tagged(target.index(), Variable::Const);   out.storage.pointer = target.output(); break;}
        case Call::Mutable:     {out.idx = Tagged(target.index(), Variable::Mutable); out.storage.pointer = target.output(); break;}
        case Call::Heap:        {out.idx = Tagged(target.index(), Variable::Heap);    out.storage.pointer = target.output(); break;}
#warning "implement these"
        case Call::Impossible:  {throw PythonError(type_error("Impossible"));}
        case Call::WrongNumber: {throw PythonError(type_error("WrongNumber"));}
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
    int x = PyObject_TypeCheck(+o, +t);
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
            auto type = output.reinterpret<PyTypeObject>();
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

std::size_t args_allocation_size(std::size_t n) {
    static_assert(sizeof(ArgStack<0, 1>) % sizeof(void*) == 0);
    static_assert(sizeof(ara_ref) % sizeof(void*) == 0);
    static_assert(sizeof(ara_ref) % sizeof(void*) == 0);
    static_assert(alignof(ArgStack<0, 1>) <= sizeof(void*));
    return (sizeof(ArgStack<0, 1>) - sizeof(ara_ref) + n * sizeof(ara_ref)) / sizeof(void*);
}


/******************************************************************************/

Shared module_call(Index index, Instance<PyTupleObject> args) {
    // args[0] is the type
    auto const total = PyTuple_GET_SIZE(args.object());
    if (total < 2) return type_error("ara call: expected at least two arguments");

    DUMP("create ArgView from args");
    auto alloc = std::make_unique<void*[]>(args_allocation_size(total - 1));
    auto &view = *reinterpret_cast<ArgView *>(alloc.get());
    view.c.args = total - 1;
    Py_ssize_t i = 1;
    for (auto &arg : view) {
        PyObject* item = PyTuple_GET_ITEM(args.object(), i);
        arg = Ref::from_existing(Index::of<Export>(), Pointer::from(item), true);
        ++i;
    }
    view.c.args = total - 2;
    view.c.tags = 1;

    DUMP("create Caller");
    bool gil = true;
    auto lk = std::make_shared<PythonFrame>(!gil);
    Caller caller(lk);
    view.c.caller_ptr = &caller;

    DUMP("create Target");
    PyObject *out = PyTuple_GET_ITEM(args.object(), 0);

    auto output = instance(PyTuple_GET_ITEM(args.object(), 0));
    return call_to_output(output, index, Pointer::from(nullptr), Tag::Const, view);
}

Shared variable_call(Variable &v, Instance<PyTupleObject> args) {
    auto const total = PyTuple_GET_SIZE(args.object());

    DUMP("create ArgView from args");
    auto alloc = std::make_unique<void*[]>(args_allocation_size(total - 1));
    auto &view = *reinterpret_cast<ArgView *>(alloc.get());
    view.c.args = total - 1;
    Py_ssize_t i = 1;
    for (auto &arg : view) {
        PyObject* item = PyTuple_GET_ITEM(args.object(), i);
        arg = Ref::from_existing(Index::of<Export>(), Pointer::from(item), true);
        ++i;
    }
    view.c.args = total - 2;
    view.c.tags = 1;

    DUMP("create Caller");
    bool gil = true;
    auto lk = std::make_shared<PythonFrame>(!gil);
    Caller caller(lk);
    view.c.caller_ptr = &caller;

    DUMP("create Target");
    PyObject *out = PyTuple_GET_ITEM(args.object(), 0);

    auto output = instance(PyTuple_GET_ITEM(args.object(), 0));
    return call_to_output(output, v.index(), Pointer::from(v.address()), Tag::Const, view);
}

/******************************************************************************/

PyObject* c_variable_call(PyObject* self, PyObject* args, PyObject* kws) noexcept {
    return raw_object([=] {
        return variable_call(cast_object<Variable>(self),
            instance(reinterpret_cast<PyTupleObject *>(args)));
    });
}

/******************************************************************************/

}
