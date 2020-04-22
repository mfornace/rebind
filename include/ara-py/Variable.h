#pragma once
#include "Wrap.h"
#include "Raw.h"
#include <ara/Ref.h>

namespace ara::py {

/******************************************************************************/

struct Variable {
    union Storage {
        void* pointer;
        char data[24];
    };

    enum Loc : ara_tag {Stack, Heap, Mutable, Const};

    template <class T>
    static constexpr Loc loc_of = Heap;

    Storage storage;
    Tagged<Loc> idx;
    // Object ward;

    Variable() noexcept = default;
    ~Variable() noexcept;

    Variable(Variable const &) = delete;
    Variable(Variable &&) = delete; //default;

    bool has_value() const noexcept {return idx.has_value();}
    Index index() const noexcept {return Index(idx);}
    Loc location() const noexcept {return static_cast<Loc>(idx.tag());}
    auto name() const noexcept {return index().name();}

    void *address() const {return idx.tag() == Stack ? const_cast<Storage *>(&storage) : storage.pointer;}
    Ref as_ref() const {return Ref(index(), Tag::Const, address());}
    Ref as_ref() {return Ref(index(), Tag::Mutable, address());}

    template <class T>
    static Object from(T value, Object ward={});

    static Object from(Ref const &, Object ward={});

    static Object new_object();
};


inline Variable::~Variable() noexcept {
    if (has_value()) switch (location()) {
        case Const:   return;
        case Mutable: return;
        case Heap:    {Destruct::call(index(), storage.pointer, Destruct::Heap); break;}
        case Stack:   {Destruct::call(index(), &storage, Destruct::Stack); break;}
    }
}

/******************************************************************************/

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

/******************************************************************************/

Ref ref_from_object(Object &o, bool move=false);

inline Object Variable::new_object() {
    return Object::from(PyObject_CallObject(TypePtr::from<Variable>(), nullptr));
}

template <class T>
Object Variable::from(T value, Object ward) {
    auto o = new_object();
    auto &v = cast_object<Variable>(+o);
    if constexpr(loc_of<T> == Heap) {
        v.storage.pointer = parts::alloc<T>(std::move(value));
    } else {
        parts::alloc_to<T>(&v.storage.data, std::move(value));
    }
    v.idx = Tagged(Index::of<T>(), loc_of<T>);
    return o;
}

inline Object Variable::from(Ref const &r, Object ward) {
    Loc loc;
    if (r) switch (r.tag()) {
        case Tag::Const: {loc = Const; break;}
        case Tag::Mutable: {loc = Mutable; break;}
        default: return type_error("cannot create Variable from temporary reference");
    }

    DUMP("making reference Variable object");
    auto o = new_object();
    auto &v = cast_object<Variable>(+o);
    if (r) {
        v.storage.pointer = r.ptr.base;
        v.idx = Tagged(r.index(), loc);
    }
    return o;
}
// void args_from_python(ArgView &s, Object const &pypack);

// bool object_response(Value &v, Index t, Object o);

// Object ref_to_object(Ref &&v, Object const &t={});

// inline Object args_to_python(ArgView &s, Object const &sig={}) {
//     if (sig && !PyTuple_Check(+sig))
//         throw python_error(type_error("expected tuple but got %R", (+sig)->ob_type));
//     std::size_t len = sig ? PyTuple_GET_SIZE(+sig) : 0;
//     auto const n = s.size();
//     auto out = Object::from(PyTuple_New(n));
//     Py_ssize_t i = 0u;
//     for (auto &r : s) {
//         if (i < len) {
//             PyObject *t = PyTuple_GET_ITEM(+sig, i);
//             throw python_error(type_error("conversion to python signature not implemented yet"));
//         } else {
//             // special case: if given an rvalue reference, make it into a value (not sure if really desirable or not)
//             // if (r.qualifier() == Rvalue) {
//             //     if (!set_tuple_item(out, i, value_to_object(r))) return {};
//             // } else {
//             //     if (!set_tuple_item(out, i, ref_to_object(r))) return {};
//             // }
//         }
//         ++i;
//     }
//     return out;
// }

}