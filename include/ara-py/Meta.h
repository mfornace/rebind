#pragma once
#include "Variable.h"
#include <deque>

namespace ara::py {

/******************************************************************************/

struct DynamicType {
    struct Member {
        std::string name;
        std::string doc = "doc string";
        Value<> annotation;
        Member(std::string_view s, Value<> a, Value<Str> doc) : name(s), annotation(std::move(a)) {}
    };
    std::unique_ptr<PyTypeObject> object;
    std::vector<PyGetSetDef> getsets;
    std::string name = "ara.extension.";
    std::deque<Member> members;

    DynamicType() noexcept
        : object(std::make_unique<PyTypeObject>(PyTypeObject{PyVarObject_HEAD_INIT(NULL, 0)})) {
        define_type<Variable>(*object, "ara.DerivedVariable", "low-level base inheriting from ara.Variable");
    }

    void finalize(Always<Tuple> args) {
        auto name = Value<>::from(PyObject_Str(+Value<>::from(PyTuple_GetItem(+args, 0))));
        if (auto s = get_unicode(*name)) {
            this->name += as_string_view(*s);
            object->tp_name = this->name.data();
        } else throw PythonError(type_error("expected str"));

        object->tp_base = +static_type<Variable>();

        if (!getsets.empty()) {
            getsets.emplace_back();
            object->tp_getset = getsets.data();
        }
        if (PyType_Ready(object.get()) < 0) throw PythonError(nullptr);
    }
};

std::unordered_map<Value<>, DynamicType> dynamic_types;

PyObject* get_member(PyObject* self, void* data);
//  {
//     return raw_object([=] {
//         auto annotation = Instance(*reinterpret_cast<PyObject*>(data));
//         DUMP("get_member", Value<>(annotation, true));
//         return Value<>(annotation, true);
//     });
// }

template <class F>
void iterate(Always<Dict> o, F &&f) {
    PyObject *key, *value;
    Py_ssize_t pos = 0;

    while (PyDict_Next(~o, &pos, &key, &value)) {
        f(instance(*key), instance(*value));
    }
}

// PyFunction_GetAnnotations
// PyFunction_GetDefaults
// PyInstanceMethod_New

// Choice 1: implement custom callable class. Unclear how to get documentation/annotations to work well though.
//           this might be most efficient
// OK the documentation for the class is not too bad. just need to provide repr()
// The documentation for the type is not great...extremely long

// Choice 2: use PyInstanceMethod_New(). unclear how to get documentation again... will look but seems not possible

// Choice 3: "eval" based approach. here we'd basically define a function inline:
// def function(self, x, y):
//     return self.method("method_name", x, y)
// advantage ... it's definitely a native function
// disadvantage ... string formatting probably, also not as efficient as Choice 1

// take name, bases, properties
// return a new type
// this is very tricky... we have to go through the properties, find the declarations
// set the rest of the properties as normal
// set these properties specially
PyObject* meta_new(PyTypeObject* type, PyObject* args, PyObject* kwargs) noexcept {
    DUMP("meta_new", Value<>(args, true), Value<>(kwargs, true));

    // static char const* kws[] = {"name", "bases", "props", nullptr};
    // char const *name;
    // PyObject *bases;
    // PyObject *props;
    // DUMP("Defining via metaclass", name);
    // HmmType.tp_hash = c_variable_hash;
    auto &base = dynamic_types[Value<>(instance(*type).object(), true)];

    auto properties = Value<>::from(PyTuple_GetItem(args, 2));
    if (auto as = PyDict_GetItemString(+properties, "__annotations__")) {
        DUMP(Value<>(as, true));
        iterate(instance(*as).as<PyDictObject>(), [&](auto k, auto v) {
            Value<> doc;
            if (auto doc_string = PyDict_GetItem(+properties, +k)) {
                doc = Value<>(doc_string, true);
                if (0 != PyDict_DelItem(+properties, +k)) throw PythonError();
            }

            if (auto s = get_unicode(k)) {
                auto &member = base.members.emplace_back(as_string_view(*s), Value<>(v, true), std::move(doc));
                base.getsets.emplace_back(PyGetSetDef{member.name.data(), get_member, nullptr, member.doc.data(), +member.annotation});
            } else throw PythonError(type_error("expected str"));
        });
    }


    base.finalize(*args);
    DUMP("ok");
    auto bases = Value<>::from(PyTuple_Pack(1, base.object.get()));
    DUMP("ok");
    auto args2 = Value<>::from(PyTuple_Pack(3, PyTuple_GET_ITEM(args, 0), +bases, PyTuple_GET_ITEM(args, 2)));
    DUMP("ok");
    // int ok = PyArg_ParseTupleAndKeywords(args, kwargs, "sOO", const_cast<char**>(kws), &name, &bases, &props);
    // if (!ok) return nullptr;
    // PyObject* out = PyType_GenericNew(type, args, kwargs);
    auto new_type = PyType_Type.tp_new(type, +args2, kwargs);
    DUMP("new type address", new_type, +static_type<Variable>());
    DUMP("new_type", Value<>(new_type, true));
    auto t = ((PyTypeObject *)(new_type));
    DUMP("new_type", t->tp_call, t->tp_call == c_variable_call, bool(t->tp_hash));

    auto method = Value<>::from(PyInstanceMethod_New(Py_None));
    DUMP("hmmmmm", PyFunction_Check(+method));
    PyObject_SetAttrString(new_type, "instance_method", +method);

    return new_type;
    // return out;
    // DUMP(type);
    // return PyObject_Call((PyObject*) &PyType_Type, args, kwargs);
    // // DUMP("making class...", Value<>(out, true));
    // if (!out) return out;
    // auto o = ((PyTypeObject*) out);

    // DUMP("hash?", (+o)->tp_hash);

    // (+o)->tp_name = "blah";
    // (+o)->tp_basicsize = sizeof(Wrap<Variable>);
    // (+o)->tp_dealloc = c_delete<Variable>;
    // (+o)->tp_new = c_new<Variable>;
    // (+o)->tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    // (+o)->tp_doc = "doc";
    // int bad = PyType_Ready(o);
    // DUMP("bad?", bad);
    // return out;
    // return nullptr;
    // name, bases, properties
    // return nullptr;
}

/******************************************************************************/

template <>
void Wrap<Meta>::initialize(Instance<PyTypeObject> o) noexcept {
    define_type<Meta>(o, "ara.Meta", "Object metaclass");
    // (+o)->tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    (+o)->tp_new = meta_new;
    (+o)->tp_base = &PyType_Type;
    // (+o)->tp_call = meta_new;
    // (+o)->tp_alloc = PyType_GenericAlloc;
}

/******************************************************************************/

}