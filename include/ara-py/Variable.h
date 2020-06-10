#pragma once
#include "Index.h"
#include "Load.h"
#include <ara/Ref.h>
#include <atomic>

namespace ara::py {

/******************************************************************************/

struct Meta : StaticType<Meta> {
    PyTypeObject base;

    Meta() noexcept {
        DUMP("Meta constructor");
        base.tp_name = "blah";
    }
    // std::string whatever;
};

/******************************************************************************/

struct pyVariable;

struct Variable : StaticType<Variable> {
    using type = pyVariable;

    struct Address {
        void* pointer;
        Mode qualifier; // Heap, Read, or Write
    };

    union Storage {
        Address address;
        char data[24];
    };

    union Lock {
        std::uintptr_t count;
        // only necessary when lock is empty, otherwise that Variable's lock would be used.
        // 0: no references
        // 1: one const reference
        // 2: two const references
        // <0: one mutable reference
        Always<> other;
        // empty: no lock
        // otherwise reference to another Variable
        // may be tuple of Variables? not sure yet.
        constexpr Lock() noexcept : count(0) {}
    };

    enum State : ara_mode {Stack, StackAlias, Heap, HeapAlias};

    template <class T>
    static constexpr bool allocated = is_stackable<T>(sizeof(Storage));

    Tagged<State> idx;
    Storage storage; // stack storage
    Lock lock;

    Variable() noexcept = default;
    ~Variable() noexcept {reset();}

    Variable(Variable const &) = delete;
    Variable(Variable &&) = delete; //default;

    void set_stack(ara::Index i) noexcept {
        this->lock.count = 0;
        idx = Tagged<State>(i, Stack);
    }

    void set_pointer(ara::Index i, void *ptr, Mode tag) noexcept {
        storage.address.pointer = ptr;
        storage.address.qualifier = tag;
        this->lock.count = 0;
        idx = Tagged<State>(i, Heap);
    }

    auto & current_lock();
    void set_lock(Always<> other);

    bool has_value() const noexcept {return idx.has_value();}
    ara::Index index() const noexcept {return ara::Index(idx);}
    void reset() noexcept;
    auto name() const noexcept {return index().name();}

    void *address() const {return idx.tag() < 2 ? const_cast<Storage *>(&storage) : storage.address.pointer;}

    template <class T>
    static Value<Variable> from(T value, Value<> ward={});

    static Value<Variable> from(Ref const &, Value<> ward={});

    //  {
    //     DUMP("load");
    //     auto acquired = acquire_ref(self, LockType::Read);
    //     if (auto out = try_load(acquired.ref, *type, Value<>())) return out;
    //     return type_error("cannot convert value to type %R from %R", +type, +Index::from(acquired.ref.index()));
    // }

    static void initialize(Always<pyType>) noexcept;
};

/******************************************************************************/

struct ObjectHead {
    PyObject_HEAD
};

struct pyVariable : ObjectHead, Variable {};

inline void Variable::set_lock(Always<> other) {
    lock.other = other;
    Py_INCREF(+other);
    reinterpret_cast<std::uintptr_t &>(idx.base) |= 1;//Tagged<State>(i, idx.state() | 1);
    ++(Always<Variable>::from(other)->lock.count);
}

inline Always<> current_root(Always<Variable> v) {return (v->idx.tag() & 0x1) ? v->lock.other : v;}

inline auto & Variable::current_lock() {
    return (idx.tag() & 0x1 ? Always<Variable>::from(lock.other)->lock.count : lock.count);
}

Value<> load(Always<Variable> self, Always<> type);

// struct Variable : Object {
//     static bool matches(Always<Type> p) {return false;}

//     static Value<> load(Ref &ref, Ignore, Ignore) {
//         return {};
//     }
// };

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
        return Ref(v.index(), Mode::Write, Pointer::from(v.address()));
    } else if (type == LockType::Read) {
        DUMP("read lock");
        ++count;
        return Ref(v.index(), Mode::Read, Pointer::from(v.address()));
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
        if (storage.address.qualifier == Mode::Heap)
            Deallocate::call(index(), Pointer::from(storage.address.pointer));
    } else {
        Destruct::call(index(), Pointer::from(&storage));
    }
    if (state & 0x1) {
        --Always<Variable>::from(lock.other)->lock.count;
        Py_DECREF(+lock.other);
    }
    idx = {};
}

/******************************************************************************/

Lifetime call_to_variable(Index self, Pointer address, Mode qualifier, ArgView &args);

/******************************************************************************/

template <class T>
Value<Variable> Variable::from(T value, Value<> lock) {
    auto v = Value<Variable>::alloc();
    if constexpr(allocated<T>) {
        v->storage.address.pointer = Allocator<T>::heap(std::move(value));
        v->storage.address.qualifier = Mode::Heap;
    } else {
        Allocator<T>::stack(&v->storage.data, std::move(value));
    }
    if (lock) {
        v->lock.other = *lock;
        Py_INCREF(+v->lock.other);
        v->idx = Tagged(Index::of<T>(), allocated<T> ? HeapAlias : StackAlias);
    } else {
        v->lock.count = 0;
        v->idx = Tagged(Index::of<T>(), allocated<T> ? Heap : Stack);
    }
    return v;
}

/******************************************************************************/

inline Value<Variable> Variable::from(Ref const &r, Value<> ward) {
    DUMP("making reference Variable object");
    auto v = Value<Variable>::alloc();
    if (r) {
        switch (r.mode()) {
            case Mode::Read: {v->storage.address.qualifier = Mode::Read; break;}
            case Mode::Write: {v->storage.address.qualifier = Mode::Write; break;}
            default: return type_error("cannot create Variable from temporary reference");
        }
        v->storage.address.pointer = r.pointer().base;
        v->idx = Tagged(r.index(), ward ? HeapAlias : Heap);
    }
    return v;
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


/******************************************************************************/

Value<> variable_lock(Always<Variable> self, Ignore) {
    if (self->has_value() && self->idx.tag() & 0x1) return {self->lock.other, true};
    else return {Py_None, true};
}

/******************************************************************************/

Value<pyInt> variable_state(Always<Variable> self, Ignore) {
    if (!self->has_value()) {
        return {Py_None, true};
    } else if (self->idx.tag() < 0x2) {
        return pyInt::from(static_cast<Integer>(Mode::Stack));
    } else {
        return pyInt::from(static_cast<Integer>(self->storage.address.qualifier));
    }
}

/******************************************************************************/

Value<> return_self(Always<> self, Ignore) noexcept {return {self, true};}

Value<> variable_reset(Always<Variable> self, Ignore) {
    self->reset();
    return {Py_None, true};
}

Value<pyInt> variable_use_count(Always<Variable> self, Ignore) noexcept {
    Integer stat = 0;
    if (self->has_value()) {
        auto i = self->current_lock();
        if (i == MutateSentinel) stat = -1;
        else stat = i;
    }
    return pyInt::from(stat);
}


int c_variable_clear(Always<Variable> self) noexcept {
    self->reset();
    return 0;
}

int c_variable_traverse(PyObject* self, visitproc, void* arg) noexcept {
    return 0;
}

/******************************************************************************/

PyObject* c_variable_float(PyObject* self) noexcept {
    return nullptr;
    // return c_load<Variable>(self, reinterpret_cast<PyObject*>(&PyFloat_Type));
}


PyObject* c_variable_int(PyObject* self) noexcept {
    return nullptr;
    // return c_load<Variable>(self, reinterpret_cast<PyObject*>(&PyLong_Type));
}

constexpr std::size_t combine_hash(std::size_t t, std::size_t u) noexcept {
    return t ^ (u + 0x9e3779b9 + (t << 6) + (t >> 2));
}

Py_hash_t c_variable_hash(Always<Variable> self) noexcept {
    DUMP("hash variable");
    if (!self->has_value()) {
        DUMP("return 0");
        return 0;
    }
    std::size_t out;
    DUMP("hashing", self->index().name());
    auto stat = Hash::call(self->index(), out, Pointer::from(self->address()));
    DUMP("hashed");
    switch (stat) {
        case Hash::OK: {
            DUMP("ok");
            return combine_hash(std::hash<ara::Index>()(self->index()), out);
        }
        case Hash::Impossible: return -1;
    }
}

int c_variable_compare_int(Always<Variable> l, Always<> other, decltype(Py_EQ) op) noexcept {
    if (auto r = Maybe<Variable>(other)) {
        if (l->index() == r->index()) {
            switch(op) {
                case(Py_EQ): return false;
                case(Py_NE): return true;
                case(Py_LT): return l->index() < r->index();
                case(Py_GT): return l->index() > r->index();
                case(Py_LE): return l->index() <= r->index();
                case(Py_GE): return l->index() >= r->index();
            }
        } else {
            bool equal;
            switch (Equal::call(l->index(), Pointer::from(l->address()), Pointer::from(r->address()))) {
                case Equal::Impossible: return -1;
                case Equal::False: {equal = false; break;}
                case Equal::True: {equal = true; break;}
            };
            if (equal) {
                switch(op) {
                    case(Py_EQ): return true;
                    case(Py_NE): return false;
                    case(Py_LT): return false;
                    case(Py_GT): return false;
                    case(Py_LE): return true;
                    case(Py_GE): return true;
                }
            } else {
                switch(op) {
                    case(Py_EQ): return false;
                    case(Py_NE): return true;
                    default: break;
                }
            }
            switch (Compare::call(l->index(), Pointer::from(l->address()), Pointer::from(r->address()))) {
                case Compare::Unordered: return -1;
                case Compare::Less: {
                    switch(op) {
                        case(Py_LT): return true;
                        case(Py_GT): return false;
                        case(Py_LE): return true;
                        case(Py_GE): return false;
                    }
                }
                case Compare::Greater: {
                    switch(op) {
                        case(Py_LT): return false;
                        case(Py_GT): return true;
                        case(Py_LE): return false;
                        case(Py_GE): return true;
                    }
                }
                default: {
                    switch(op) {
                        case(Py_LT): return false;
                        case(Py_GT): return false;
                        case(Py_LE): return true;
                        case(Py_GE): return true;
                    }
                }
            };
        }
    }
    return -1;
}


// PyObject* c_variable_compare(PyObject* self, PyObject* other, decltype(Py_EQ) op) noexcept {
//     switch (c_variable_compare_int(self, other, op)) {
//         case 0: {Py_INCREF(Py_False); return Py_False;}
//         case 1: {Py_INCREF(Py_True); return Py_True;}
//         default: return nullptr;
//     }
// }

PyNumberMethods VariableNumberMethods = {
    // .nb_bool = static_cast<inquiry>(c_operator_has_value<Variable>),
    // .nb_int = c_variable_int,
    // .nb_float = c_variable_float
};

/******************************************************************************/

// PyObject* c_variable_element(PyObject* self, Py_ssize_t) noexcept;
// {
//     return c_load<Variable>(self, reinterpret_cast<PyObject*>(&PyLong_Type));
// }

// PySequenceMethods VariableSequenceMethods = {
//     .sq_item = c_variable_element
// };

/******************************************************************************/

PyMethodDef VariableMethods[] = {
    // {"copy_from", c_function(c_copy_from<Value>),
    //     METH_O, "assign from other using C++ copy assignment"},

    // {"move_from", c_function(c_move_from<Value>),
    //     METH_O, "assign from other using C++ move assignment"},
    // copy
    // swap

    {"lock", api<variable_lock, Always<Variable>, Ignore>,
        METH_NOARGS, "lock(self)\n--\n\nget lock object"},

    {"__enter__", api<return_self, Always<>, Ignore>,
        METH_NOARGS, "__enter__(self)\n--\n\nreturn self"},

    {"__exit__", api<variable_reset, Always<Variable>, Ignore>,
        METH_VARARGS, "__exit__(self, *args)\n--\n\nalias for Variable.reset"},

    {"reset", api<variable_reset, Always<Variable>, Ignore>,
        METH_NOARGS, "reset(self)\n--\n\nreset the Variable"},

    {"use_count", api<variable_use_count, Always<Variable>, Ignore>,
        METH_NOARGS, "use_count(self)\n--\n\nuse count or -1"},

    {"state", api<variable_state, Always<Variable>, Ignore>,
        METH_NOARGS, "state(self)\n--\n\nreturn state of object"},

    // {"index", c_function(c_get_index<Variable>),
    //     METH_NOARGS, "index(self)\n--\n\nreturn Index of the held C++ object"},

    // {"as_value", c_function(c_as_value<Variable>),
    //     METH_NOARGS, "return an equivalent non-reference object"},

    // {"load", c_function(c_load<Variable>),
    //     METH_O, "load(self, type)\n--\n\ncast to a given Python type"},

    // {"method", c_function(c_variable_method),
    //     METH_VARARGS | METH_KEYWORDS, "method(self, name, out=None)\n--\n\ncall a method given a name and arguments"},

    // {"attribute", c_function(c_variable_attribute),
    //     METH_VARARGS | METH_KEYWORDS, "attribute(self, name, out=None)\n--\n\nget a reference to a member variable"},

    // {"from_object", c_function(c_value_from),
        // METH_CLASS | METH_O, "cast an object to a given Python type"},
    {nullptr, nullptr, 0, nullptr}
};

/******************************************************************************/

void Variable::initialize(Always<pyType> o) noexcept {
    DUMP("defining Variable");
    define_type<Variable>(o, "ara.Variable", "Object class");
    (~o)->ob_type = +Meta::def();
    (+o)->tp_as_number = &VariableNumberMethods;
    (+o)->tp_methods = VariableMethods;
    // (+o)->tp_call = c_variable_call;
    // (+o)->tp_clear = c_variable_clear;
    (+o)->tp_traverse = c_variable_traverse;
    (+o)->tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC;
    // (+o)->tp_getattro = c_variable_getattr;
    // (+o)->tp_setattro = c_variable_setattr;
    (+o)->tp_getset = nullptr;
    (+o)->tp_hash = PyBaseObject_Type.tp_hash;//c_variable_hash;
    // o->tp_richcompare = c_variable_compare;
    // no init (just use default constructor)
    // PyMemberDef, tp_members
};

/******************************************************************************/

}