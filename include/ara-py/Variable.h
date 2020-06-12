#pragma once
#include "Index.h"
#include "Call.h"
#include "Load.h"
#include <ara/Ref.h>
#include <atomic>

namespace ara::py {

/******************************************************************************/

struct Variable {
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

    void set_stack(Index i) noexcept {
        this->lock.count = 0;
        idx = Tagged<State>(i, Stack);
    }

    void set_pointer(Index i, void *ptr, Mode tag) noexcept {
        storage.address.pointer = ptr;
        storage.address.qualifier = tag;
        this->lock.count = 0;
        idx = Tagged<State>(i, Heap);
    }

    std::uintptr_t & current_lock();

    void set_lock(Always<> other);

    bool has_value() const noexcept {return idx.has_value();}

    Index index() const noexcept {return Index(idx);}

    void reset() noexcept;

    auto name() const noexcept {return index().name();}

    void *address() const {return idx.tag() < 2 ? const_cast<Storage *>(&storage) : storage.address.pointer;}

    static constexpr auto Mutating = std::numeric_limits<std::uintptr_t>::max();

    Ref begin_acquisition(LockType type) {
        if (!has_value()) return Ref();
        auto &count = current_lock();

        if (count == Mutating) {
            throw PythonError::type("cannot reference object which is being mutated");
        } else if (count == 0 && type == LockType::Write) {
            DUMP("write lock");
            count = Mutating;
            return Ref(index(), Mode::Write, Pointer::from(address()));
        } else if (type == LockType::Read) {
            DUMP("read lock");
            ++count;
            return Ref(index(), Mode::Read, Pointer::from(address()));
        } else {
            throw PythonError::type("cannot mutate object which is already referenced");
        }
    }

    void end_acquisition() {
        if (!has_value()) return;
        auto &count = current_lock();

        if (count == Mutating) count = 0;
        else --count;
        DUMP("restored acquisition!", count);
    }

    struct AcquiredRef {
        Ref ref;
        Variable &v;
        ~AcquiredRef() noexcept {v.end_acquisition();}
    };

    AcquiredRef acquire_ref(LockType type) {
        return AcquiredRef{begin_acquisition(type), *this};
    }

};

/******************************************************************************/

struct VariableObject : ObjectBase, Variable {};

/******************************************************************************/

struct pyVariable : StaticType<pyVariable> {
    using type = VariableObject;

    static void placement_new(Variable &v) noexcept {new (&v) Variable();}

    static void initialize_type(Always<pyType>) noexcept;

    /**************************************************************************/

    static Value<> return_self(Always<> self, Ignore) noexcept {return self;}

    static Value<pyNone> reset(Always<pyVariable> self, Ignore) {
        self->reset();
        return Always<pyNone>(*Py_None);
    }

    static Value<pyIndex> index(Always<pyVariable> self, Ignore) {
        DUMP("index", +self, reference_count(self), self, self->index(), self->name());
        auto i = Value<pyIndex>::new_from(self->index());
        DUMP("hmm", i->integer(), bool(i), reference_count(i));
        DUMP("index address", &static_cast<Index&>(*i));
        DUMP("index address", &**i, &static_cast<Index&>(*i), i->integer());
        DUMP("index reference count", reference_count(i));
        return i;
    }

    static Value<pyInt> use_count(Always<pyVariable> self, Ignore) noexcept {
        Integer stat = 0;
        if (self->has_value()) {
            auto i = self->current_lock();
            if (i == Variable::Mutating) stat = -1;
            else stat = i;
        }
        return pyInt::from(stat);
    }

    static int clear(Always<pyVariable> self) noexcept {
        self->reset();
        return 0;
    }

    static int traverse(Always<pyVariable> self, visitproc, void* arg) noexcept {
        return 0;
    }

    /******************************************************************************/

    static int as_bool(Always<pyVariable> self) noexcept {return self->has_value();}

    static Value<pyFloat> as_float(Always<pyVariable> self) {
        return nullptr;
        // return load<Variable>(self, reinterpret_cast<PyObject*>(&PyFloat_Type));
    }


    static Value<pyInt> as_int(Always<pyVariable> self) {
        return nullptr;
        // return load<Variable>(self, reinterpret_cast<PyObject*>(&PyLong_Type));
    }

    static int compare(Always<pyVariable> l, Always<> other, decltype(Py_EQ) op) noexcept;

    /******************************************************************************/

    static Value<> lock(Always<pyVariable> self, Ignore) noexcept {
        if (self->has_value() && self->idx.tag() & 0x1) return self->lock.other;
        else return {Py_None, true};
    }

    /******************************************************************************/

    static Value<pyInt> state(Always<pyVariable> self, Ignore) {
        if (!self->has_value()) {
            return {Py_None, true};
        } else if (self->idx.tag() < 0x2) {
            return pyInt::from(static_cast<Integer>(Mode::Stack));
        } else {
            return pyInt::from(static_cast<Integer>(self->storage.address.qualifier));
        }
    }

    /******************************************************************************/

    static Value<> load(Always<pyVariable> self, Always<> type) {
        DUMP("load");
        auto acquired = self->acquire_ref(LockType::Read);
        if (auto out = try_load(acquired.ref, type, Value<>())) return out;
        return type_error("cannot convert value to type %R from %R", +type,
            +Value<pyIndex>::new_from(acquired.ref.index()));
    }

    static Py_hash_t hash(Always<pyVariable> self) noexcept {
        DUMP("hash variable", reference_count(self));
        if (!self->has_value()) {
            DUMP("return 0 because it is null");
            return 0;
        }
        std::size_t out;
        DUMP("hashing", self->index().name());
        auto stat = Hash::call(self->index(), out, Pointer::from(self->address()));
        DUMP("hashed");
        switch (stat) {
            case Hash::OK: {
                DUMP("ok");
                return combine_hash(std::hash<Index>()(self->index()), out);
            }
            case Hash::Impossible: return -1;
        }
    }

    template <class T>
    static Value<pyVariable> from(T value, Value<> lock) {
        auto v = Value<pyVariable>::new_from();
        if constexpr(Variable::allocated<T>) {
            v->storage.address.pointer = Allocator<T>::heap(std::move(value));
            v->storage.address.qualifier = Mode::Heap;
        } else {
            Allocator<T>::stack(&v->storage.data, std::move(value));
        }
        if (lock) {
            v->lock.other = *lock;
            Py_INCREF(+v->lock.other);
            v->idx = Tagged(Index::of<T>(), Variable::allocated<T> ? Variable::HeapAlias : Variable::StackAlias);
        } else {
            v->lock.count = 0;
            v->idx = Tagged(Index::of<T>(), Variable::allocated<T> ? Variable::Heap : Variable::Stack);
        }
        return v;
    }

    static Value<pyVariable> from(Ref const &r, Value<> ward) {
        DUMP("making reference pyVariable object");
        auto v = Value<pyVariable>::new_from();
        if (r) {
            switch (r.mode()) {
                case Mode::Read: {v->storage.address.qualifier = Mode::Read; break;}
                case Mode::Write: {v->storage.address.qualifier = Mode::Write; break;}
                default: return type_error("cannot create Variable from temporary reference");
            }
            v->storage.address.pointer = r.pointer().base;
            v->idx = Tagged(r.index(), ward ? Variable::HeapAlias : Variable::Heap);
        }
        return v;
    }

    static Value<> method(Always<pyVariable> v, Always<pyTuple> args, CallKeywords &&kws);

    static PyNumberMethods number_methods;
    static PyMethodDef methods[];
};

PyNumberMethods pyVariable::number_methods = {
    .nb_bool = api<as_bool, Always<pyVariable>>,
    .nb_int = api<as_int, Always<pyVariable>>,
    .nb_float = api<as_float, Always<pyVariable>>
};

/******************************************************************************/

PyMethodDef pyVariable::methods[] = {
    // {"copy_from", c_function(c_copy_from<Value>),
    //     METH_O, "assign from other using C++ copy assignment"},

    // {"move_from", c_function(c_move_from<Value>),
    //     METH_O, "assign from other using C++ move assignment"},
    // copy
    // swap

    {"lock", api<lock, Always<pyVariable>, Ignore>, METH_NOARGS,
        "lock(self)\n--\n\nget lock object"},

    {"__enter__", api<return_self, Always<>, Ignore>, METH_NOARGS,
        "__enter__(self)\n--\n\nreturn self"},

    {"__exit__", api<reset, Always<pyVariable>, Ignore>, METH_VARARGS,
        "__exit__(self, *args)\n--\n\nalias for Variable.reset"},

    {"reset", api<reset, Always<pyVariable>, Ignore>, METH_NOARGS,
        "reset(self)\n--\n\nreset the Variable"},

    {"use_count", api<use_count, Always<pyVariable>, Ignore>, METH_NOARGS,
        "use_count(self)\n--\n\nuse count or -1"},

    {"state", api<state, Always<pyVariable>, Ignore>, METH_NOARGS,
        "state(self)\n--\n\nreturn state of object"},

    {"index", api<index, Always<pyVariable>, Ignore>, METH_NOARGS,
        "index(self)\n--\n\nreturn Index of the held C++ object"},

    // {"as_value", c_function(c_as_value<Variable>),
    //     METH_NOARGS, "return an equivalent non-reference object"},

    // {"load", c_function(c_load<Variable>),
    //     METH_O, "load(self, type)\n--\n\ncast to a given Python type"},

    // {"method", c_function(c_variable_method),
    //     METH_VARARGS | METH_KEYWORDS, "method(self, name, out=None)\n--\n\ncall a method given a name and arguments"},

    // {"attribute", attribute, METH_VARARGS | METH_KEYWORDS,
    //     "attribute(self, name, out=None)\n--\n\nget a reference to a member variable"},

    // {"from_object", c_function(c_value_from),
        // METH_CLASS | METH_O, "cast an object to a given Python type"},
    {nullptr, nullptr, 0, nullptr}
};

/******************************************************************************/

inline void Variable::set_lock(Always<> other) {
    lock.other = other;
    Py_INCREF(+other);
    reinterpret_cast<std::uintptr_t &>(idx.base) |= 1;//Tagged<State>(i, idx.state() | 1);
    ++(Always<pyVariable>::from(other)->lock.count);
}

inline Always<> current_root(Always<pyVariable> v) {return (v->idx.tag() & 0x1) ? v->lock.other : v;}

inline std::uintptr_t & Variable::current_lock() {
    return (idx.tag() & 0x1 ? Always<pyVariable>::from(lock.other)->lock.count : lock.count);
}

Value<> load(Always<pyVariable> self, Always<> type);

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
        --Always<pyVariable>::from(lock.other)->lock.count;
        Py_DECREF(+lock.other);
    }
    idx = {};
}

/******************************************************************************/

Lifetime call_to_variable(Index self, Pointer address, Mode qualifier, ArgView &args);

/******************************************************************************/

template <class F>
Value<> map_output(Always<> t, F &&f) {
    // Type objects
    // return {};
    if (auto type = Maybe<pyType>(t)) {
        if (pyNone::matches(*type))  return f(pyNone());
        if (pyBool::matches(*type))  return f(pyBool());
        if (pyInt::matches(*type))   return f(pyInt());
        if (pyFloat::matches(*type)) return f(pyFloat());
        if (pyStr::matches(*type))   return f(pyStr());
        if (pyBytes::matches(*type)) return f(pyBytes());
        if (pyIndex::matches(*type)) return f(pyIndex());
// //         else if (*type == &PyBaseObject_Type)                  return as_deduced_object(std::move(r));        // object
//         // if (*type == static_type<VariableType>()) return f(VariableType());           // Value
    }
    // Non type objects
    DUMP("Not a type");
//     // if (auto p = instance<Index>(t)) return f(Index());  // Index

//     if (pyUnion::matches(t)) return f(pyUnion());
//     if (pyList::matches(t))  return f(pyList());       // pyList[T] for some T (compound def)
//     if (pyTuple::matches(t)) return f(pyTuple());      // pyTuple[Ts...] for some Ts... (compound def)
//     if (pyDict::matches(t))  return f(pyDict());       // pyDict[K, V] for some K, V (compound def)
//     DUMP("Not one of the structure types");
    return {};
//     DUMP("custom convert ", output_conversions.size());
//     if (auto p = output_conversions.find(Value<>{t, true}); p != output_conversions.end()) {
//         DUMP(" conversion ");
//         Value<> o;// = ref_to_object(std::move(r), {});
//         if (!o) return type_error("could not cast value to Python object");
//         DUMP("calling function");
// //         // auto &obj = cast_object<Variable>(o).ward;
// //         // if (!obj) obj = root;
//         return Value<>::from(PyObject_CallFunctionObjArgs(+p->second, +o, nullptr));
//     }
}

/******************************************************************************/

Value<> try_load(Ref &r, Always<> t, Maybe<> root) {
    DUMP("try_load", r.name(), "to type", t, "with root", root);
    return map_output(t, [&](auto T) {
        return decltype(T)::load(r, t, root);
    });
}

/******************************************************************************/

Value<> pyVariable::method(Always<pyVariable> v, Always<pyTuple> args, CallKeywords &&kws) {
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
        auto self = v->acquire_ref(self_mode == 'w' ? LockType::Write : LockType::Read);
        TupleLock locking(a.view, args, 1);
        locking.lock(kws.mode);

        std::tie(out, life) = call_with_caller(self.ref.index(), self.ref.pointer(), self.ref.mode(), a.view, kws);
    }
    DUMP("lifetime to attach = ", life.value);
    if (life.value) {
        if (auto o = Maybe<pyVariable>(out)) {
            for (unsigned i = 0; life.value; ++i) {
                if (life.value & 1) {
                    DUMP("got one", i);
                    if (i) {
                        DUMP("setting root to argument", i-1, size(args));
                        if (auto arg = Maybe<pyVariable>(item(args, i))) {
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

int pyVariable::compare(Always<pyVariable> l, Always<> other, decltype(Py_EQ) op) noexcept {
    if (auto r = Maybe<pyVariable>(other)) {
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
    if (auto v = Maybe<pyVariable>(output)) {
        DUMP("instance of Variable");
        out = {Py_None, true};
        return fun(*v);
    }

    if (auto t = Maybe<pyType>(output)) {
        DUMP("is type");
        if (is_subclass(*t, pyVariable::def())) {
            DUMP("is Variable subclass");
            auto v = Value<pyVariable>::new_from();
            auto life = fun(*v);
            out = std::move(v);
            return life;
        }
    }
#warning "aliasing is a problem here"
    auto v = Value<pyVariable>::new_from();
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
        auto v = Value<pyVariable>::new_from();
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
            if (auto p = Maybe<pyVariable>(it)) {
                DUMP("its variable!");
                ref = p->begin_acquisition(LockType::Read);
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
            if (auto p = Maybe<pyVariable>(it)) {
                DUMP("its variable!", *c);
                ref = p->begin_acquisition(*c == 'w' ? LockType::Write : LockType::Read);
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
                if (auto v = Maybe<pyVariable>(item(args, s)))
                    v->end_acquisition();
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

Value<> index_call(Index index, Always<pyTuple> args, CallKeywords const& kws) {
    DUMP("index_call", index.name());
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

template <class Module>
Value<> module_call(Ignore, Always<pyTuple> args, CallKeywords const &kws) {
    return index_call(Switch<Module>::call, args, kws);
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

template <class I>
auto access_with_caller(Index self, Pointer address, I element, Mode mode, CallKeywords const& kws) {
    Value<> out;
    auto life = call_to_output(out, kws.out, [&](Variable& v) {
        return invoke_access(v, self, address, element, mode);
    });
    return std::make_pair(out, life);
}

template <class I>
Value<> variable_access(Always<pyVariable> v, Always<pyTuple> args, CallKeywords &&kws) {
    DUMP("variable_attr", v->name());
    auto element = Always<I>::from(item(args, 0));

    DUMP("mode", kws.mode);
    Value<> out;
    Lifetime life;
    {
        char self_mode = remove_mode(kws.mode);
        auto self = v->acquire_ref(self_mode == 'w' ? LockType::Write : LockType::Read);
        std::tie(out, life) = access_with_caller(self.ref.index(), self.ref.pointer(), element, self.ref.mode(), kws);
    }
    DUMP("lifetime to attach = ", life.value);
    if (life.value) {
        if (auto o = Maybe<pyVariable>(out)) {
            DUMP("setting root to self");
            o->set_lock(current_root(v));
        }
    }
    return out;
}

// PyObject* c_variable_compare(PyObject* self, PyObject* other, decltype(Py_EQ) op) noexcept {
//     switch (c_variable_compare_int(self, other, op)) {
//         case 0: {Py_INCREF(Py_False); return Py_False;}
//         case 1: {Py_INCREF(Py_True); return Py_True;}
//         default: return nullptr;
//     }
// }


/******************************************************************************/

// PyObject* c_variable_element(PyObject* self, Py_ssize_t) noexcept;
// {
//     return c_load<Variable>(self, reinterpret_cast<PyObject*>(&PyLong_Type));
// }

// PySequenceMethods VariableSequenceMethods = {
//     .sq_item = c_variable_element
// };

/******************************************************************************/

void pyVariable::initialize_type(Always<pyType> o) noexcept {
    DUMP("defining Variable");
    define_type<pyVariable>(o, "ara.Variable", "Object class");
    // (~o)->ob_type = +pyMeta::def();
    o->tp_as_number = &pyVariable::number_methods;
    o->tp_methods = pyVariable::methods;
    // o->tp_call = c_variable_call;
    // o->tp_clear = api<clear, Always<pyVariable>>;
    // o->tp_traverse = api<traverse, Always<pyVariable>, visitproc, void*>;
    // o->tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC;
    // o->tp_getattro = c_variable_getattr;
    // o->tp_setattro = c_variable_setattr;
    o->tp_getset = nullptr;
    // o->tp_hash = api<c_variable_hash, Always<pyVariable>>;
    // o->tp_richcompare = c_variable_compare;
    // no init (just use default constructor)
    // PyMemberDef, tp_members
};

/******************************************************************************/

}