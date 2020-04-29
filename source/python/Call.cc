#include <ara-py/Variable.h>
#include <ara-py/Call.h>
#include <ara-py/Dump.h>

/******************************************************************************/

namespace ara::py {

Object call_to_variable(Index self, Pointer address, Tag qualifier, ArgView &args) {
    auto o = Variable::new_object();
    Variable &out = cast_object<Variable>(+o);
    Target target{Index(), &out.storage, sizeof(out.storage), Target::Stack};

    auto const stat = Call::call(self, target, address, qualifier, args);
    DUMP("Variable got stat: ", stat);

    switch (stat) {
        case Call::None:        break;
        case Call::Const:       {out.idx = Tagged(target.idx, Variable::Const);   break;}
        case Call::Mutable:     {out.idx = Tagged(target.idx, Variable::Mutable); break;}
        case Call::Stack:       {out.idx = Tagged(target.idx, Variable::Stack);   break;}
        case Call::Heap:        {out.idx = Tagged(target.idx, Variable::Heap);    break;}
#warning "implement these"
        case Call::Impossible:  {throw PythonError(type_error("Impossible"));}
        case Call::WrongNumber: {throw PythonError(type_error("WrongNumber"));}
        case Call::WrongType:   {throw PythonError(type_error("WrongType"));}
        case Call::WrongReturn: {throw PythonError(type_error("WrongReturn"));}
        case Call::Exception:   {throw PythonError(type_error("Exception"));}
        case Call::OutOfMemory: {throw std::bad_alloc();}
    }
    return o;
}

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

    DUMP("create ArgView from args");
    auto alloc = std::make_unique<void*[]>(args_allocation_size(total - 1));
    auto &view = *reinterpret_cast<ArgView *>(alloc.get());
    view.args = total - 1;
    Py_ssize_t i = 1;
    for (auto &arg : view) {arg = Ref(Ptr(PyTuple_GET_ITEM(args, i))); ++i;}
    view.args = total - 2;
    view.tags = 1;

    DUMP("create Caller");
    bool gil = true;
    auto lk = std::make_shared<PythonFrame>(!gil);
    Caller caller(lk);
    view.caller_ptr = &caller;

    DUMP("create Target");
    PyObject *out = PyTuple_GET_ITEM(args, 0);

    return call_to_variable(index, nullptr, Tag::Const, view);
}

/******************************************************************************/

}
