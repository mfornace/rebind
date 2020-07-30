#pragma once
#include "Index.h"
#include "Call.h"
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
        char data[32];
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

Value<> try_load(Ref &r, Always<> t, Maybe<> root);

/******************************************************************************/

struct py_variable : StaticType<py_variable> {
    using type = VariableObject;

    static void placement_new(Variable &v) noexcept {new (&v) Variable();}
    
    static void placement_new(Variable &v, Always<pyTuple> args, Maybe<pyDict> kws) noexcept {
        new (&v) Variable();
    }

    static void initialize_type(Always<pyType>) noexcept;

    /**************************************************************************/

    static Value<> return_self(Always<> self, Ignore) noexcept {return self;}

    static Value<pyNone> reset(Always<py_variable> self, Ignore) {
        self->reset();
        return Always<pyNone>(*Py_None);
    }

    static Value<pyIndex> index(Always<py_variable> self, Ignore) {
        return Value<pyIndex>::new_from(self->index());
    }

    static Value<pyInt> use_count(Always<py_variable> self, Ignore) noexcept {
        Integer stat = 0;
        if (self->has_value()) {
            auto i = self->current_lock();
            if (i == Variable::Mutating) stat = -1;
            else stat = i;
        }
        return pyInt::from(stat);
    }

    static int clear(Always<py_variable> self) noexcept {
        self->reset();
        return 0;
    }

    static int traverse(Always<py_variable> self, visitproc visit, void* arg) noexcept {
        if (self->has_value() && self->idx.tag() & 0x1) {
            Py_VISIT(~self->lock.other);
        }
        return 0;
    }

    /******************************************************************************/

    static int as_bool(Always<py_variable> self) noexcept {return self->has_value();}

    static Value<> load(Always<py_variable> self, Always<> type);

    static Value<> as_float(Always<py_variable> self) {return load(self, pyFloat::def());}

    static Value<> as_int(Always<py_variable> self) {return load(self, pyInt::def());}

    static Value<> compare(Always<py_variable> l, Always<> other, int op) noexcept;

    static Value<> call(Always<py_variable>, Always<pyTuple>, Modes, Tag, Out, GIL);
    static Value<> fcall(Always<py_variable>, Always<pyTuple>, Maybe<pyDict>);
    
    static Value<> method(Always<py_variable>, Always<pyTuple>, Modes, Tag, Out, GIL);
    static Value<> fmethod(Always<py_variable>, Always<pyTuple>, Maybe<pyDict>);

    static Value<> element(Always<py_variable>, Always<pyTuple>, Maybe<pyDict>);
    static Value<> attribute(Always<py_variable>, Always<pyTuple>, Maybe<pyDict>);
    
    static Value<> getattr(Always<py_variable>, Always<> key);

    static Value<> bind(Always<py_variable>, Always<pyTuple>, Maybe<pyDict> kws);

    /******************************************************************************/

    static Value<> lock(Always<py_variable> self, Ignore) noexcept {
        if (self->has_value() && self->idx.tag() & 0x1) return self->lock.other;
        else return {Py_None, true};
    }

    /******************************************************************************/

    static Value<pyInt> state(Always<py_variable> self, Ignore) {
        if (!self->has_value()) {
            return {Py_None, true};
        } else if (self->idx.tag() < 0x2) {
            DUMP("on stack", int(self->idx.tag()));
            return pyInt::from(static_cast<Integer>(Mode::Stack));
        } else {
            DUMP("I guess not on stack");
            return pyInt::from(static_cast<Integer>(self->storage.address.qualifier));
        }
    }

    /******************************************************************************/

    static Py_hash_t hash(Always<py_variable> self) noexcept;

    template <class T>
    static Value<py_variable> from(T value, Value<> lock) {
        DUMP("making non reference object", Variable::allocated<T>, sizeof(T), type_name<T>());
        auto v = Value<py_variable>::new_from();
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

    static Value<py_variable> from(Ref const &r, Value<> ward) {
        DUMP("making reference py_variable object");
        auto v = Value<py_variable>::new_from();
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

    static PyNumberMethods number_methods;
    static PyMethodDef methods[];
};

/******************************************************************************/

inline void Variable::set_lock(Always<> other) {
    lock.other = other;
    Py_INCREF(+other);
    reinterpret_cast<std::uintptr_t &>(idx.base) |= 1;//Tagged<State>(i, idx.state() | 1);
    ++(Always<py_variable>::from(other)->lock.count);
}

inline Always<> current_root(Always<py_variable> v) {return (v->idx.tag() & 0x1) ? v->lock.other : v;}

inline std::uintptr_t & Variable::current_lock() {
    return (idx.tag() & 0x1 ? Always<py_variable>::from(lock.other)->lock.count : lock.count);
}

Value<> load(Always<py_variable> self, Always<> type);

/******************************************************************************/

Lifetime call_to_variable(Index self, Pointer address, Mode qualifier, ArgView &args);

/******************************************************************************/

template <class F>
Value<> map_output(Always<> t, F &&f) {
    // Type objects
    if (auto type = Maybe<pyType>(t)) {
        if (pyNone::matches(*type))  return f(pyNone());
        if (pyBool::matches(*type))  return f(pyBool());
        if (pyInt::matches(*type))   return f(pyInt());
        if (pyFloat::matches(*type)) return f(pyFloat());
        if (pyStr::matches(*type))   return f(pyStr());
        if (pyBytes::matches(*type)) return f(pyBytes());
        if (pyIndex::matches(*type)) return f(pyIndex());
// //         else if (*type == &PyBaseObject_Type)                  return as_deduced_object(std::move(r));        // object
        // if (*type == static_type<VariableType>()) return f(VariableType());           // Value
    }

    // Non type objects
    DUMP("Not a type");
    // if (auto p = instance<Index>(t)) return f(Index());  // Index

    if (pyUnion::matches(t)) return f(pyUnion());
    if (pyList::matches(t))  return f(pyList());       // pyList[T] for some T (compound def)
    if (pyTuple::matches(t)) return f(pyTuple());      // pyTuple[Ts...] for some Ts... (compound def)
    if (pyOption::matches(t)) return f(pyOption());      // pyTuple[Ts...] for some Ts... (compound def)
    if (pyDict::matches(t))  return f(pyDict());       // pyDict[K, V] for some K, V (compound def)
    DUMP("Not one of the structure types");
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

Target variable_target(Variable& out) {
    return {Index(), &out.storage, sizeof(out.storage),
        Target::Write | Target::Read | Target::Heap | Target::Trivial |
        Target::Relocatable | Target::MoveNoThrow | Target::Unmovable | Target::MoveThrow};
}

Lifetime invoke_call(Variable &out, Index self, Pointer address, ArgView &args, Mode mode) {
    Target target = variable_target(out);
    auto const stat = Method::invoke(self, target, address, mode, args);
    DUMP("Variable got stat and lifetime:", stat, target.c.lifetime);

    switch (stat) {
        case Method::None:        break;
        case Method::Stack:       {out.set_stack(target.index()); break;}
        case Method::Read:        {out.set_pointer(target.index(), target.output(), Mode::Read); break;}
        case Method::Write:       {out.set_pointer(target.index(), target.output(), Mode::Write); break;}
        case Method::Heap:        {out.set_pointer(target.index(), target.output(), Mode::Heap); break;}
        case Method::Impossible:  {throw PythonError(type_error("Impossible"));}
        case Method::WrongNumber: {throw PythonError(type_error("WrongNumber"));}
        case Method::WrongType:   {throw PythonError(type_error("WrongType"));}
        case Method::WrongReturn: {throw PythonError(type_error("WrongReturn"));}
        case Method::Exception:   {throw PythonError(type_error("Exception"));}
        case Method::OutOfMemory: {throw std::bad_alloc();}
    }
    DUMP("output:", out.name(), "stat:", stat, "address:", out.address(), "lifetime:", target.c.lifetime, target.lifetime().value);
    return target.lifetime();
}

/******************************************************************************/

// Fills the variable out *if* present, otherwise does nothing
template <class I>
Lifetime invoke_access(Variable &out, Index self, Pointer address, I element, Mode mode) {
    Target target = variable_target(out);
    using A = std::conditional_t<std::is_same_v<I, Integer>, Element, Attribute>;
    auto const stat = A::invoke(self, target, address, element, mode);
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

Value<py_variable> new_variable_subtype(Always<pyType> t) {
    auto out = Value<py_variable>::take(t->tp_alloc(+t, 0)); // allocate the object; 0 unused
    py_variable::placement_new(*out); 
    DUMP(bool(out), reference_count(out));
    return out;
}

/******************************************************************************/

template <class F>
Lifetime call_to_output(Value<> &out, Always<> const output, F&& fun) {
    DUMP("wtf", output);
    if (auto v = Maybe<py_variable>(output)) {
        DUMP("instance of Variable");
        out = {Py_None, true};
        return fun(*v);
    }

    if (auto t = Maybe<pyType>(output)) {
        DUMP("is type");
        if (is_subclass(*t, py_variable::def())) {
            DUMP("is Variable subclass");
            auto v = new_variable_subtype(*t);
            auto life = fun(*v);
            DUMP("set to variable subclass", v->name(), reference_count(v));
            out = std::move(v);
            DUMP("set to variable subclass 2", reference_count(v), reference_count(out));
            return life;
        }
    }
#warning "aliasing is a problem here"
    auto v = Value<py_variable>::new_from();
    fun(*v);
    Ref r(v->index(), Mode::Write, Pointer::from(v->address()));
    if (auto o = try_load(r, output, Value<>())) {out = o; return {};}
    else throw PythonError(type_error("could not load"));
}

/******************************************************************************/

template <class F>
Lifetime call_to_output(Value<> &out, Out output, F&& fun) {
    DUMP("the output", output.value);
    if (output.value) {
        return call_to_output(out, *output.value, fun);
    } else {
        auto v = Value<py_variable>::new_from();
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
            if (auto p = Maybe<py_variable>(it)) {
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
            if (auto p = Maybe<py_variable>(it)) {
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
                if (auto v = Maybe<py_variable>(item(args, s)))
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

/******************************************************************************/

auto call_with_caller(Index self, Pointer address, Mode mode, ArgView& args, Out out, GIL gil) {
    auto lk = std::make_shared<PythonFrame>(!gil.value);
    Caller caller(lk);
    args.c.caller_ptr = &caller;

    Value<> o;
    auto life = call_to_output(o, out, [&](Variable& v){
        return invoke_call(v, self, address, args, mode);
    });
    DUMP("call_with_caller output", reference_count(o), bool(o));
    return std::make_pair(o, life);
}

/******************************************************************************/

template <class I>
auto try_access_with_caller(Index self, Pointer address, I element, Mode mode, Out out) {
    Value<> o;
    bool worked;
    auto life = call_to_output(o, out, [&](Variable& v) {
        auto life = invoke_access(v, self, address, element, mode);
        worked = v.has_value();
        return life;
    });
    if (!worked) o = {};
    return std::make_pair(std::move(o), life);
}

/******************************************************************************/

Value<> load_address(Ignore, Always<> addr) {
    std::uintptr_t i = view_underlying(Always<pyInt>::from(addr));
    Index idx = *reinterpret_cast<ara_index*>(i);
    // auto const total = size(args);
    ArgAlloc a(0, 0);

    // for (Py_ssize_t i = 0; i != total; ++i)
    //     a.view[i] = Ref(Index::of<Export>(), Mode::Write, Pointer::from(+item(args, i)));

    Value<> out = call_with_caller(idx, Pointer::from(nullptr), Mode::Read, a.view, {}, {}).first;
    auto v = Always<py_variable>::from(*out);
    // v.load();
    // if (PyObject_SetAttrString(~v, "blah", ~v)) return {};

    return out;
    // Method::call(idx);
}

Value<> load_library(Ignore, Always<pyTuple> args, Maybe<pyDict> kws) {
    auto [file, name] = parse<2, pyStr, pyStr>(args, kws, {"file", "function"});
    auto ctypes = Value<>::take(PyImport_ImportModule("ctypes"));
    auto CDLL = Value<>::take(PyObject_GetAttrString(~ctypes, "CDLL"));
    auto addressof = Value<>::take(PyObject_GetAttrString(~ctypes, "addressof"));
    
    auto lib = Value<>::take(PyObject_CallFunctionObjArgs(~CDLL, ~file, nullptr));
    auto fun = Value<>::take(PyObject_GetAttr(~lib, ~name));
    auto address = Value<>::take(PyObject_CallFunctionObjArgs(~addressof, ~fun));
    return load_address({}, *address);
}


}