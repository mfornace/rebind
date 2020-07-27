#include <ara-py/Module.h>
#include <ara-py/Meta.h>


extern "C" {
    PyMODINIT_FUNC PyInit_sfb(void) {return ara::py::init_module();}
}

namespace ara::py {

PyNumberMethods pyIndex::number_methods = {
    .nb_bool = reinterpret<as_bool, Always<pyIndex>>,
    .nb_int = reinterpret<as_int, Always<pyIndex>>
};

void pyIndex::initialize_type(Always<pyType> o) noexcept {
    define_type<pyIndex>(o, "ara.Index", "Index type");
    o->tp_repr = reinterpret<repr, Always<pyIndex>>;
    o->tp_hash = reinterpret<hash, Always<pyIndex>>;
    o->tp_str = reinterpret<str, Always<pyIndex>>;
    o->tp_as_number = &number_methods;
    o->tp_richcompare = reinterpret<compare<pyIndex>, Always<pyIndex>, Always<>, int>;
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

Py_hash_t pyVariable::hash(Always<pyVariable> self) noexcept {
    DUMP("hash variable", reference_count(self));
    if (!self->has_value()) {
        DUMP("return 0 because it is null");
        return 0;
    }
    std::size_t out;
    auto stat = Hash::call(self->index(), out, Pointer::from(self->address()));
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
    auto const total = size(args);
    ArgAlloc a(total, 0);

    for (Py_ssize_t i = 0; i != total; ++i)
        a.view[i] = Ref(Index::of<Export>(), Mode::Write, Pointer::from(+item(args, i)));

    return call_with_caller(v->index(), Pointer::from(v->address()), Mode::Read, a.view, kws).first;
}

/******************************************************************************/

template <class I>
Value<> variable_access(Always<pyVariable> v, Always<pyTuple> args, CallKeywords&& kws) {
    DUMP("variable_attr", v->name());
    if (size(args) != 1) throw PythonError::type("expected 1 positional argument");
    auto element = view_underlying(Always<I>::from(item(args, 0)));

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

/******************************************************************************/

Value<> pyVariable::attribute(Always<pyVariable> v, Always<pyTuple> args, CallKeywords&& kws) {
    return variable_access<pyStr>(v, args, std::move(kws));
}

/******************************************************************************/

Value<> pyVariable::element(Always<pyVariable> v, Always<pyTuple> args, CallKeywords&& kws) {
    return variable_access<pyInt>(v, args, std::move(kws));
}

/******************************************************************************/

PyNumberMethods pyVariable::number_methods = {
    .nb_bool = reinterpret<as_bool, Always<pyVariable>>,
    .nb_int = reinterpret<as_int, Always<pyVariable>>,
    .nb_float = reinterpret<as_float, Always<pyVariable>>
};

/******************************************************************************/

PyMethodDef pyVariable::methods[] = {
    // {"copy_from", c_function(c_copy_from<Value>),
    //     METH_O, "assign from other using C++ copy assignment"},

    // {"move_from", c_function(c_move_from<Value>),
    //     METH_O, "assign from other using C++ move assignment"},
    // copy
    // swap

    {"lock", reinterpret<lock, Always<pyVariable>, Ignore>, METH_NOARGS,
        "lock(self)\n--\n\nget lock object"},

    {"__enter__", reinterpret<return_self, Always<>, Ignore>, METH_NOARGS,
        "__enter__(self)\n--\n\nreturn self"},

    {"__exit__", reinterpret<reset, Always<pyVariable>, Ignore>, METH_VARARGS,
        "__exit__(self, *args)\n--\n\nalias for Variable.reset"},

    {"reset", reinterpret<reset, Always<pyVariable>, Ignore>, METH_NOARGS,
        "reset(self)\n--\n\nreset the Variable"},

    {"use_count", reinterpret<use_count, Always<pyVariable>, Ignore>, METH_NOARGS,
        "use_count(self)\n--\n\nuse count or -1"},

    {"state", reinterpret<state, Always<pyVariable>, Ignore>, METH_NOARGS,
        "state(self)\n--\n\nreturn state of object"},

    {"index", reinterpret<index, Always<pyVariable>, Ignore>, METH_NOARGS,
        "index(self)\n--\n\nreturn Index of the held C++ object"},

    // {"as_value", c_function(c_as_value<Variable>),
    //     METH_NOARGS, "return an equivalent non-reference object"},

    {"load", reinterpret<load, Always<pyVariable>, Always<>>, METH_O,
        "load(self, type)\n--\n\ncast to a given Python type"},

    {"method", reinterpret<method, Always<pyVariable>, Always<pyTuple>, CallKeywords>, METH_VARARGS | METH_KEYWORDS,
        "method(self, name, out=None)\n--\n\ncall a method given a name and arguments"},

    {"attribute", reinterpret<attribute, Always<pyVariable>, Always<pyTuple>, CallKeywords>, METH_VARARGS | METH_KEYWORDS,
        "attribute(self, name, out=None)\n--\n\nget a reference to a member variable"},

    {"element", reinterpret<element, Always<pyVariable>, Always<pyTuple>, CallKeywords>, METH_VARARGS | METH_KEYWORDS,
        "element(self, name, out=None)\n--\n\nget a reference to a member variable"},

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
    o->tp_call = reinterpret<pyVariable::call, Always<pyVariable>, Always<pyTuple>, Maybe<pyDict>>;
    // o->tp_clear = reinterpret<clear, Always<pyVariable>>;
    // o->tp_traverse = reinterpret<traverse, Always<pyVariable>, visitproc, void*>;
    // o->tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC;
    // o->tp_getattro = c_variable_getattr;
    // o->tp_setattro = c_variable_setattr;
    o->tp_getset = nullptr;
    // o->tp_hash = reinterpret<c_variable_hash, Always<pyVariable>>;
    // o->tp_richcompare = c_variable_compare;
    // no init (just use default constructor)
    // PyMemberDef, tp_members
};

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

/******************************************************************************/

void DynamicType::finalize(Always<pyTuple> args) {
    DUMP("finalizing dynamic class");
    auto s = Value<pyStr>::take(PyObject_Str(+item_at(args, 0)));
    this->name += as_string_view(*s);
    object->tp_name = this->name.data();
    object->tp_base = +pyVariable::def();

    if (!getsets.empty()) {
        getsets.emplace_back();
        object->tp_getset = getsets.data();
    }
    if (PyType_Ready(object.get()) < 0) throw PythonError();
}

/******************************************************************************/

void DynamicType::add_members(Always<pyDict> as, Always<pyDict> properties) {
    DUMP("working on annotations");
    iterate(as, [&](Ignore, auto k, auto v) {
        auto key = Always<pyStr>::from(k);

        Value<pyStr> doc;
        if (auto doc_string = item(properties, key)) {
            doc = Always<pyStr>::from(*doc_string);
            if (0 != PyDict_DelItem(~properties, ~key)) throw PythonError();
        }

        auto &member = members.emplace_back(as_string_view(key), v, std::move(doc));
        // DUMP("ok", as_string_view(*key), member.name, member.doc, +member.annotation);
        getsets.emplace_back(PyGetSetDef{member.name.data(),
            reinterpret<get_member, Always<>, Always<>>, nullptr, member.doc.data(), +member.annotation});
    });
}

/******************************************************************************/

Value<pyType> pyMeta::new_type(Ignore, Always<pyTuple> args, Maybe<pyDict> kwargs) {
    DUMP("new_type", args);
    if (size(args) != 3) throw PythonError::type("Meta.__new__ takes 3 positional arguments");

    auto& base = dynamic_types.emplace_back();

    auto properties = Always<pyDict>::from(item(args, 2));

    if (auto as = item(properties, "__annotations__"))
        base.add_members(Always<pyDict>::from(*as), properties);

    base.finalize(args);

    auto bases = Value<pyTuple>::take(PyTuple_Pack(1, base.object.get()));
    // auto bases = Value<pyTuple>::take(PyTuple_Pack(1, +pyVariable::def()));

    auto args2 = Value<pyTuple>::take(PyTuple_Pack(3, +item(args, 0), +bases, +item(args, 2)));
    DUMP("Calling type()", args2, kwargs);
    auto out = Value<pyType>::take(PyObject_Call(~pyType::def(), ~args2, +kwargs));

    auto method = Value<>::take(PyInstanceMethod_New(Py_None));
    // PyObject_SetAttrString(~out, "instance_method", +method);
    DUMP("done");
    return out;
    // return PyObject_Call((PyObject*) &PyType_Type, args, kwargs);
    // // DUMP("making class...", Value<>(out, true));
    // DUMP("hash?", (+o)->tp_hash);
}

/******************************************************************************/

void pyMeta::initialize_type(Always<pyType> o) noexcept {
    o->tp_name = "ara.Meta";
    o->tp_basicsize = sizeof(PyTypeObject);
    o->tp_doc = "Object metaclass";
    o->tp_new = reinterpret<new_type, Always<pyType>, Always<pyTuple>, Maybe<pyDict>>;
    o->tp_base = +pyType::def();
    o->tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
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

std::deque<DynamicType> dynamic_types;

}