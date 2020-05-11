#pragma once
#include "Wrap.h"
#include "Raw.h"
#include "Load.h"
#include <ara/Ref.h>
#include <atomic>

namespace ara::py {

/******************************************************************************/

struct Variable {
    struct Address {
        void* pointer;
        Tag qualifier; // Heap, Const, or Mutable
    };

    union Storage {
        Address address;
        char data[24];
    };

    union Lock {
        PyObject* other;
        // empty: no lock
        // otherwise reference to another Variable
        // may be tuple of Variables? not sure yet.
        std::uintptr_t count;
        // only necessary when lock is empty, otherwise that Variable's lock would be used.
        // 0: no references
        // 1: one const reference
        // 2: two const references
        // <0: one mutable reference
    };

    enum State : ara_tag {Stack, StackAlias, Heap, HeapAlias};

    template <class T>
    static constexpr bool allocated = is_stackable<T>(sizeof(Storage));

    Tagged<State> idx;
    Storage storage; // stack storage
    Lock lock;

    Variable() noexcept = default;
    ~Variable() noexcept {reset();}

    Variable(Variable const &) = delete;
    Variable(Variable &&) = delete; //default;

    void set_stack(Index i) noexcept {
        this->lock.count = 0;
        idx = Tagged<State>(i, Stack);
    }

    void set_heap(Index i, void *ptr, Tag tag) noexcept {
        storage.address.pointer = ptr;
        storage.address.qualifier = tag;
        this->lock.count = 0;
        idx = Tagged<State>(i, Heap);
    }

    void set_lock(Instance<> other) {
        lock.other = +other;
        Py_INCREF(+other);
        reinterpret_cast<std::uintptr_t &>(idx.base) |= 1;//Tagged<State>(i, idx.state() | 1);
        ++(cast_object<Variable>(lock.other).lock.count);
    }

    bool has_value() const noexcept {return idx.has_value();}
    Index index() const noexcept {return Index(idx);}
    void reset() noexcept;
    auto name() const noexcept {return index().name();}

    void *address() const {return idx.tag() < 2 ? const_cast<Storage *>(&storage) : storage.address.pointer;}

    template <class T>
    static Shared from(T value, Shared ward={});

    static Shared from(Ref const &, Shared ward={});

    static Shared new_object();

    Instance<> current_root(Instance<> self) const {
        return (idx.tag() & 0x1) ? instance(lock.other) : self;
    }

    auto & current_lock() {
        return (idx.tag() & 0x1 ? cast_object<Variable>(lock.other).lock.count : lock.count);
    }
};

/******************************************************************************/

static constexpr auto MutateSentinel = std::numeric_limits<std::uintptr_t>::max();

inline Ref begin_acquisition(Variable& v, LockType type) {
    if (!v.has_value()) return Ref();
    auto &count = v.current_lock();

    if (count == MutateSentinel) {
        throw PythonError(type_error("cannot reference object which is being mutated"));
    } else if (count == 0 && type == LockType::Write) {
        DUMP("write lock");
        count = MutateSentinel;
        return Ref(v.index(), Tag::Mutable, Pointer::from(v.address()));
    } else if (type == LockType::Read) {
        DUMP("read lock");
        ++count;
        return Ref(v.index(), Tag::Const, Pointer::from(v.address()));
    } else {
        throw PythonError(type_error("cannot mutate object which is already referenced"));
    }
}

inline void end_acquisition(Variable& v) {
    if (!v.has_value()) return;
    auto &count = v.current_lock();

    if (count == MutateSentinel) count = 0;
    else --count;
    DUMP("restored acquisition!", count);
}

struct AcquiredRef {
    Ref ref;
    Variable &v;
    ~AcquiredRef() noexcept {end_acquisition(v);}
};

inline AcquiredRef acquire_ref(Variable& v, LockType type) {
    return AcquiredRef{begin_acquisition(v, type), v};
}

/******************************************************************************/

inline void Variable::reset() noexcept {
    if (!has_value()) return;
    auto const state = idx.tag();
    if (!(state & 0x1) && lock.count > 0) {
        DUMP("not resetting because a reference is held");
        return;
    }
    if (state & 0x2) {
        if (storage.address.qualifier == Tag::Heap)
            Destruct::call(index(), Pointer::from(storage.address.pointer), Destruct::Heap);
    } else {
        Destruct::call(index(), Pointer::from(&storage), Destruct::Stack);
    }
    if (state & 0x1) {
        --cast_object<Variable>(lock.other).lock.count;
        Py_DECREF(lock.other);
    }
    idx = {};
}

/******************************************************************************/

Lifetime call_to_variable(Index self, Pointer address, Tag qualifier, ArgView &args);

/******************************************************************************/

Ref ref_from_object(Instance<> o, bool move=false);

inline Shared Variable::new_object() {
    return Shared::from(PyObject_CallObject(static_type<Variable>().object(), nullptr));
}

template <class T>
Shared Variable::from(T value, Shared lock) {
    Shared o = new_object();
    auto &v = cast_object<Variable>(+o);
    if constexpr(allocated<T>) {
        v.storage.address.pointer = allocate<T>(std::move(value));
        v.storage.address.qualifier = Tag::Heap;
    } else {
        allocate_in_place<T>(&v.storage.data, std::move(value));
    }
    if (lock) {
        v.lock.other = +lock;
        Py_INCREF(v.lock.other);
        v.idx = Tagged(Index::of<T>(), allocated<T> ? HeapAlias : StackAlias);
    } else {
        v.lock.count = 0;
        v.idx = Tagged(Index::of<T>(), allocated<T> ? Heap : Stack);
    }
    return o;
}

inline Shared Variable::from(Ref const &r, Shared ward) {
    DUMP("making reference Variable object");
    auto o = new_object();
    auto &v = cast_object<Variable>(+o);
    if (r) {
        switch (r.tag()) {
            case Tag::Const: {v.storage.address.qualifier = Tag::Const; break;}
            case Tag::Mutable: {v.storage.address.qualifier = Tag::Mutable; break;}
            default: return type_error("cannot create Variable from temporary reference");
        }
        v.storage.address.pointer = r.pointer().base;
        v.idx = Tagged(r.index(), ward ? HeapAlias : Heap);
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