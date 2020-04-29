#pragma once
#include "Wrap.h"
#include "Raw.h"
#include "Load.h"
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
    Ref as_ref() const {return has_value() ? Ref::from_existing(index(), Pointer::from(address()), false) : Ref();}
    Ref as_ref() {return has_value() ? Ref::from_existing(index(), Pointer::from(address()), true) : Ref();}

    template <class T>
    static Shared from(T value, Shared ward={});

    static Shared from(Ref const &, Shared ward={});

    static Shared new_object();
};

inline Variable::~Variable() noexcept {
    if (has_value()) switch (location()) {
        case Const:   return;
        case Mutable: return;
        case Heap:    {Destruct::call(index(), Pointer::from(storage.pointer), Destruct::Heap); break;}
        case Stack:   {Destruct::call(index(), Pointer::from(&storage), Destruct::Stack); break;}
    }
}

/******************************************************************************/

Shared call_to_variable(Index self, Pointer address, Tag qualifier, ArgView &args);

/******************************************************************************/

Ref ref_from_object(Instance<> o, bool move=false);

inline Shared Variable::new_object() {
    return Shared::from(PyObject_CallObject(static_type<Variable>().object(), nullptr));
}

template <class T>
Shared Variable::from(T value, Shared ward) {
    Shared o = new_object();
    auto &v = cast_object<Variable>(+o);
    if constexpr(loc_of<T> == Heap) {
        v.storage.pointer = parts::alloc<T>(std::move(value));
    } else {
        parts::alloc_to<T>(&v.storage.data, std::move(value));
    }
    v.idx = Tagged(Index::of<T>(), loc_of<T>);
    return o;
}

inline Shared Variable::from(Ref const &r, Shared ward) {
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
        v.storage.pointer = r.pointer().base;
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