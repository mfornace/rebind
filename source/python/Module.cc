#include <ara-py/Module.h>
#include <ara-py/Meta.h>


extern "C" {
    PyMODINIT_FUNC PyInit_sfb(void) {return ara::py::init_module();}
}

namespace ara::py {


PyMethodDef pyIndex::methods[] = {
    {"forward", reinterpret_kws<forward, Always<pyIndex>>, METH_VARARGS | METH_KEYWORDS,
        "forward(self, function)\n--\n\ndeclare function"}
};

PyNumberMethods pyIndex::number_methods = {
    .nb_bool = reinterpret<as_bool, nullptr, Always<pyIndex>>,
    .nb_int = reinterpret<as_int, nullptr, Always<pyIndex>>
};

void pyIndex::initialize_type(Always<pyType> o) noexcept {
    define_type<pyIndex>(o, "ara.Index", "Index type");
    o->tp_repr = reinterpret<repr, nullptr, Always<pyIndex>>;
    o->tp_hash = reinterpret<hash, -1, Always<pyIndex>>;
    o->tp_str = reinterpret<str, nullptr, Always<pyIndex>>;
    o->tp_as_number = &number_methods;
    o->tp_methods = methods;
    o->tp_richcompare = reinterpret<compare<pyIndex>, nullptr, Always<pyIndex>, Always<>, int>;
};

/******************************************************************************/

Value<> pyIndex::call(Index index, Always<pyTuple> args, CallKeywords&& kws) {
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

CallKeywords::CallKeywords(Maybe<pyDict> kws) {
    if (!kws) return;
    out = PyDict_GetItemString(~kws, "out");
    tags = PyDict_GetItemString(~kws, "tags");

    Py_ssize_t n = 0;
    if (tags) ++n;
    if (out) ++n;

    if (auto g = Maybe<>(PyDict_GetItemString(~kws, "gil"))) {
        gil = PyObject_IsTrue(+g);
        ++n;
    }

    if (auto r = Maybe<>(PyDict_GetItemString(~kws, "mode"))) {
        mode = as_string_view(*r);
        ++n;
    }

    if (n != PyDict_Size(~kws)) {
        PyDict_DelItemString(~kws, "tag");
        PyDict_DelItemString(~kws, "out");
        PyDict_DelItemString(~kws, "mode");
        PyDict_DelItemString(~kws, "gil");
        throw PythonError::type("ara.Variable: unexpected keyword arguments: %R", +Value<>::take(PyDict_Keys(~kws)));
    }
    // DUMP("made keywords!", mode.size());
}

/******************************************************************************/

void Variable::reset() noexcept {
    if (!has_value()) return;
    auto const state = idx.tag();
    DUMP("state", state);
    
    if (!(state & 0x1) && lock.count > 0) { // Read or Write
        DUMP("not resetting because a reference is held");
        return;
    }
    
    if (state & 0x2) { // Heap
        if (storage.address.qualifier == Mode::Heap)
            Deallocate::invoke(index(), Pointer::from(storage.address.pointer));
    } else {
        Destruct::invoke(index(), Pointer::from(&storage));
    }
    if (state & 0x1) { // Stack
        --Always<pyVariable>::from(lock.other)->lock.count;
        Py_DECREF(+lock.other);
    }
    idx = {};
}

/******************************************************************************/

Py_hash_t pyVariable::hash(Always<pyVariable> self) noexcept {
    DUMP("hash variable", reference_count(self));
    if (!self->has_value()) {
        DUMP("return 0 because it is null");
        return 0;
    }
    std::size_t out;
    auto stat = Hash::invoke(self->index(), out, Pointer::from(self->address()));
    switch (stat) {
        case Hash::OK: {
            return combine_hash(std::hash<Index>()(self->index()), out);
        }
        case Hash::Impossible: return -1;
    }
}

/******************************************************************************/

Value<> pyVariable::method(Always<pyVariable> v, Always<pyTuple> args, CallKeywords &&kws) {
    DUMP("pyVariable::method", v->name());
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

Value<> pyVariable::call(Always<pyVariable> v, Always<pyTuple> args, CallKeywords&& kws) {
    DUMP("variable_call", v->name());
    if (!v->has_value()) throw PythonError::type("Calling method on empty Variable");
    auto const total = size(args);
    ArgAlloc a(total, 0);

    for (Py_ssize_t i = 0; i != total; ++i)
        a.view[i] = Ref(Index::of<Export>(), Mode::Write, Pointer::from(+item(args, i)));

    return call_with_caller(v->index(), Pointer::from(v->address()), Mode::Read, a.view, kws).first;
}

/******************************************************************************/

template <class I>
Value<> try_variable_access(Always<pyVariable> v, I arg, CallKeywords&& kws) {
    if (!v->has_value()) return {};
    DUMP("variable_attr", v->name());
    DUMP("mode", kws.mode);
    Value<> out;
    Lifetime life;
    {
        char self_mode = remove_mode(kws.mode);
        auto self = v->acquire_ref(self_mode == 'w' ? LockType::Write : LockType::Read);
        std::tie(out, life) = try_access_with_caller(self.ref.index(), self.ref.pointer(), arg, self.ref.mode(), kws);
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

/******************************************************************************/

Value<> pyVariable::attribute(Always<pyVariable> v, Always<pyTuple> args, CallKeywords&& kws) {
    if (size(args) != 1) throw PythonError::type("expected 1 positional argument");

    if (auto out = try_variable_access(v, view_underlying(Always<pyStr>::from(item(args, 0))), std::move(kws)))
        return out;
    throw PythonError::attribute(~item(args, 0));
}

/******************************************************************************/

Value<> pyVariable::getattr(Always<pyVariable> v, Always<> key) {
    DUMP("getting member...", key);

    if (auto p = PyObject_GenericGetAttr(~v, ~key); p && !PyErr_Occurred()) {
        return Value<>::take(p);
    } else {
        if (PyErr_Occurred()) {
            if (PyErr_ExceptionMatches(PyExc_AttributeError)) {
                PyErr_Clear();
            } else {
                throw PythonError();
            }
        }
    }

    if (v->has_value()) {
        DUMP("looking");
        if (auto out = try_variable_access(v, view_underlying(Always<pyStr>::from(key)), CallKeywords())) {
            DUMP("got it", out);

            return out;
        }
    }

    throw PythonError::attribute(~key);
}

/******************************************************************************/

Value<> pyVariable::element(Always<pyVariable> v, Always<pyTuple> args, CallKeywords&& kws) {
    if (size(args) != 1) throw PythonError::type("expected 1 positional argument");

    if (auto out = try_variable_access(v, view_underlying(Always<pyInt>::from(item(args, 0))), std::move(kws)))
        return out;
    throw PythonError::index(~item(args, 0));
}

/******************************************************************************/

PyNumberMethods pyVariable::number_methods = {
    .nb_bool = reinterpret<as_bool, nullptr, Always<pyVariable>>,
    .nb_int = reinterpret<as_int, nullptr, Always<pyVariable>>,
    .nb_float = reinterpret<as_float, nullptr, Always<pyVariable>>
};

/******************************************************************************/

PyMethodDef pyVariable::methods[] = {
    // {"copy_from", c_function(c_copy_from<Value>),
    //     METH_O, "assign from other using C++ copy assignment"},

    // {"move_from", c_function(c_move_from<Value>),
    //     METH_O, "assign from other using C++ move assignment"},
    // copy
    // swap

    {"lock", reinterpret<lock, nullptr, Always<pyVariable>, Ignore>, METH_NOARGS,
        "lock(self)\n--\n\nget lock object"},

    {"__enter__", reinterpret<return_self, nullptr, Always<>, Ignore>, METH_NOARGS,
        "__enter__(self)\n--\n\nreturn self"},

    {"__exit__", reinterpret<reset, nullptr, Always<pyVariable>, Ignore>, METH_VARARGS,
        "__exit__(self, *args)\n--\n\nalias for Variable.reset"},

    {"reset", reinterpret<reset, nullptr, Always<pyVariable>, Ignore>, METH_NOARGS,
        "reset(self)\n--\n\nreset the Variable"},

    {"use_count", reinterpret<use_count, nullptr, Always<pyVariable>, Ignore>, METH_NOARGS,
        "use_count(self)\n--\n\nuse count or -1"},

    {"state", reinterpret<state, nullptr, Always<pyVariable>, Ignore>, METH_NOARGS,
        "state(self)\n--\n\nreturn state of object"},

    {"index", reinterpret<index, nullptr, Always<pyVariable>, Ignore>, METH_NOARGS,
        "index(self)\n--\n\nreturn Index of the held C++ object"},

    // {"as_value", c_function(c_as_value<Variable>),
    //     METH_NOARGS, "return an equivalent non-reference object"},

    {"load", reinterpret<load, nullptr, Always<pyVariable>, Always<>>, METH_O,
        "load(self, type)\n--\n\ncast to a given Python type"},

    {"forward", reinterpret_kws<forward, Always<pyVariable>>, METH_VARARGS | METH_KEYWORDS,
        "forward(self)\n--\n\nforward a method"},

    {"method", reinterpret<method, nullptr, Always<pyVariable>, Always<pyTuple>, CallKeywords>, METH_VARARGS | METH_KEYWORDS,
        "method(self, name, out=None)\n--\n\ncall a method given a name and arguments"},

    {"attribute", reinterpret<attribute, nullptr, Always<pyVariable>, Always<pyTuple>, CallKeywords>, METH_VARARGS | METH_KEYWORDS,
        "attribute(self, name, out=None)\n--\n\nget a reference to a member variable"},

    {"element", reinterpret<element, nullptr, Always<pyVariable>, Always<pyTuple>, CallKeywords>, METH_VARARGS | METH_KEYWORDS,
        "element(self, name, out=None)\n--\n\nget a reference to a member variable"},

    {"from_library", reinterpret_kws<from_library, Always<pyType>>, METH_CLASS | METH_VARARGS | METH_KEYWORDS,
        "from_library(cls, file_name, function_name)\n--\n\nload index from a DLL"},
    // {"from_object", c_function(c_value_from),
        // METH_CLASS | METH_O, "cast an object to a given Python type"},
    {nullptr, nullptr, 0, nullptr}
};


/******************************************************************************/

void pyVariable::initialize_type(Always<pyType> o) noexcept {
    DUMP("defining Variable");
    define_type<pyVariable>(o, "ara.Variable", "Object class");
    o->tp_as_number = &pyVariable::number_methods;
    o->tp_methods = pyVariable::methods;
    o->tp_call = reinterpret_kws<pyVariable::call, Always<pyVariable>>;
    // o->tp_clear = reinterpret<clear, Always<pyVariable>>;
    // o->tp_traverse = reinterpret<traverse, Always<pyVariable>, visitproc, void*>;
    // o->tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;// | Py_TPFLAGS_HAVE_GC;
    o->tp_getattro = reinterpret<pyVariable::getattr, nullptr, Always<pyVariable>, Always<>>;
    // o->tp_setattro = c_variable_setattr;
    // o->tp_getset = nullptr;
    o->tp_hash = reinterpret<hash, -1, Always<pyVariable>>;
    o->tp_richcompare = reinterpret<compare, nullptr, Always<pyVariable>, Always<>, int>;
    // no init (just use default constructor)
    // PyMemberDef, tp_members
};

/******************************************************************************/


int compare_impl(Always<pyVariable> l, Always<> other, int op) noexcept {
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
            switch (Equal::invoke(l->index(), Pointer::from(l->address()), Pointer::from(r->address()))) {
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
            switch (Compare::invoke(l->index(), Pointer::from(l->address()), Pointer::from(r->address()))) {
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

Value<> pyVariable::compare(Always<pyVariable> l, Always<> other, int op) noexcept {
    switch (compare_impl(l, other, op)) {
        case 0: return Always<>(*Py_False);
        case 1: return Always<>(*Py_True);
        default: return Always<>(*Py_NotImplemented);
    }
}

/******************************************************************************/

template <class F>
bool list_or_tuple(Always<> o, F &&f) {
    if (auto t = Maybe<pyTuple>(o)) return f(*t), true;
    if (auto t = Maybe<pyList>(o)) return f(*t), true;
    return false;
}

bool dump_span(Target &target, Always<> o) {
    DUMP("dump_span");
    // depends on the guarantee below...hmmm...probably better to get rid of this approach
    return list_or_tuple(o, [&target](auto t) {
        std::size_t n = size(t);
        PyObject** data = (+t)->ob_item;
        DUMP("dumping tuple/list into span", data, n);
        return target.emplace<Span>(reinterpret_cast<Export**>(data), n);
    });
}


bool dump_array(Target &target, Always<> o) {
    DUMP("dump_array");
    if (list_or_tuple(o, [&target](auto t) {
        std::size_t n = size(t);
        PyObject** start = (+t)->ob_item;
        return target.emplace<Array>(Span(reinterpret_cast<Export**>(start), n), ObjectGuard::make_unique(t));
    })) return true;

    if (auto t = Maybe<pyDict>(o)) {
        std::size_t const len = PyDict_Size(+o);
        auto a = Value<pyTuple>::take(PyTuple_New(len));
        iterate(*t, [&a](auto i, Always<> key, Always<> value) noexcept {
            DUMP("set elements", i, reference_count(key), reference_count(value));
            set_new_item(*a, 2 * i, key);
            set_new_item(*a, 2 * i + 1, value);
            DUMP("set elements", i, reference_count(key), reference_count(value));
        });
        DUMP("make array", +a, &((+a)->ob_item));
        return target.emplace<Array>(
            Span(reinterpret_cast<Export**>((+a)->ob_item), {len, 2}),
            ObjectGuard::make_unique(*a));
    }
    return false;
}

/******************************************************************************/

std::optional<View> get_view(Always<> o) {
    std::optional<View> v;
    list_or_tuple(o, [&](auto t) {
        auto const n = PyList_GET_SIZE(+o);
        v.emplace(n, [&](auto &p, Ignore) {
            for (Py_ssize_t i = 0; i != n; ++i) {
                new (p) Ref(reinterpret_cast<Export&>(*PyList_GET_ITEM(+o, i)));
                ++p;
            }
        });
    });

    if (!v) {
        Value<> iter(PyObject_GetIter(+o), true);
        if (!iter) {PyErr_Clear(); return v;}

        std::vector<Ref> vec;
        Value<> item;
        while ((item = {PyIter_Next(+iter), true}))
            vec.emplace_back(reinterpret_cast<Export&>(*(+o)));

        v.emplace(vec.size(), [&](auto &p, Ignore) {
            for (auto &r : vec) {
                new (p) Ref(std::move(r));
                ++p;
            }
        });
    }

    return v;
}

/******************************************************************************/

bool dump_view(Target& target, Always<> o) {
    if (auto v = get_view(o))
        return target.emplace<View>(std::move(*v));
    return false;
}

bool dump_tuple(Target& target, Always<> o) {
    if (auto v = get_view(o))
        return target.emplace<Tuple>(std::move(*v), ObjectGuard::make_unique(o));
    return false;
}

/******************************************************************************/

bool dump_object(Target &target, Always<> o) {
    DUMP("dumping object", target.name(), o);

    if (auto v = Maybe<pyVariable>(o)) {
        auto acquired = v->acquire_ref(LockType::Read);
        return acquired.ref.get_to(target);
    }

    if (target.accepts<Str>()) {
        if (auto p = Maybe<pyStr>(o)) return target.emplace<Str>(as_string_view(*p));
        if (auto p = Maybe<pyBytes>(o)) return target.emplace<Str>(as_string_view(*p));
        return false;
    }

    if (target.accepts<String>()) {
        if (auto p = Maybe<pyStr>(o)) return target.emplace<String>(as_string_view(*p));
        if (auto p = Maybe<pyBytes>(o)) return target.emplace<String>(as_string_view(*p));
        return false;
    }

    if (target.accepts<Index>()) {
        if (auto p = Maybe<pyIndex>(o)) return target.emplace<Index>(*p);
        else return false;
    }

    if (target.accepts<Float>())
        return dump_arithmetic<Float>(target, o);

    if (target.accepts<Integer>())
        return dump_arithmetic<Integer>(target, o);

    if (target.accepts<Span>())
        return dump_span(target, o);

    if (target.accepts<Array>())
        return dump_array(target, o);

    if (target.accepts<View>())
        return dump_view(target, o);

    if (target.accepts<Tuple>())
        return dump_tuple(target, o);

    if (target.accepts<Bool>()) {
        return target.emplace<Bool>(Bool{static_cast<bool>(PyObject_IsTrue(+o))});
    }

    return false;
}

/******************************************************************************/

Value<> pyVariable::load(Always<pyVariable> self, Always<> type) {
    DUMP("load");
    auto acquired = self->acquire_ref(LockType::Read);
    if (auto out = try_load(acquired.ref, type, Value<>())) return out;
    return type_error("cannot convert value to type %R from %R", +type,
        +Value<pyIndex>::new_from(acquired.ref.index()));
}

/******************************************************************************/

// std::deque<DynamicType> dynamic_types;

Value<> pyVariable::forward(Always<pyVariable> v, Always<pyTuple> args, Maybe<pyDict> kws) {
    auto [tag, mode, out] = parse<1, Object, pyStr, Object>(args, kws, {"tag", "mode", "out"});
    auto method = pyVariable::getattr(v, tag);
    return Value<pyForward>::new_from(Always<pyVariable>::from(*method), mode, out);
}

}