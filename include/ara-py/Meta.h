#pragma once
#include "Variable.h"
#include <deque>

namespace ara::py {


/******************************************************************************/

struct pyMeta : StaticType<pyMeta> {
    using type = PyTypeObject;

    static void initialize_type(Always<pyType>) noexcept;

    // static void placement_new(type& t) noexcept {
    //     DUMP("init meta?");
    //     // t = type();
    //     // t.tp_name = "blah";
    // }
};

/******************************************************************************/

struct DynamicType {
    struct Member {
        std::string name;
        std::string doc = "doc string";
        Value<> annotation;
        Member(std::string_view s, Value<> a, Value<pyStr> doc)
            : name(s), annotation(std::move(a)) {}
    };
    std::unique_ptr<PyTypeObject> object;
    std::vector<PyGetSetDef> getsets;
    std::string name = "ara.extension.";
    std::deque<Member> members;

    DynamicType() noexcept
        : object(std::make_unique<PyTypeObject>(PyTypeObject{PyVarObject_HEAD_INIT(NULL, 0)})) {
        define_type<pyVariable>(*object, "ara.DerivedVariable", "low-level base inheriting from ara.Variable");
        // reinterpret_cast<PyObject*>(object.get())->ob_type = +pyMeta::def();
    }

    void finalize(Always<pyTuple> args) {
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
};

std::deque<DynamicType> dynamic_types;

Value<> get_member(Always<> self, Always<> annotation) {
    return {};
    // DUMP("get_member", Value<>(annotation, true));
    // return Value<>(annotation, true);
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

/******************************************************************************/

Value<pyType> meta_new(Ignore, Always<pyTuple> args, Maybe<pyDict> kwargs) {
    DUMP("meta_new", args);
    if (size(args) != 3) throw PythonError::type("Meta.__new__ takes 3 positional arguments");

    auto& base = dynamic_types.emplace_back();

    auto properties = Always<pyDict>::from(item(args, 2));
    if (auto as = item(properties, "__annotations__")) {
        DUMP("working on annotations");
        iterate(Always<pyDict>::from(*as), [&](auto k, auto v) {
            auto key = Always<pyStr>::from(k);

            Value<pyStr> doc;
            if (auto doc_string = item(properties, key)) {
                doc = Always<pyStr>::from(*doc_string);
                if (0 != PyDict_DelItem(~properties, ~key)) throw PythonError();
            }

            auto &member = base.members.emplace_back(as_string_view(key), v, std::move(doc));
            // DUMP("ok", as_string_view(*key), member.name, member.doc, +member.annotation);
            base.getsets.emplace_back(PyGetSetDef{member.name.data(),
                api<get_member, Always<>, Always<>>, nullptr, member.doc.data(), +member.annotation});
        });
    }

    base.finalize(args);

    // auto bases = Value<pyTuple>::take(PyTuple_Pack(1, base.object.get()));
    auto bases = Value<pyTuple>::take(PyTuple_Pack(1, +pyVariable::def()));

    auto args2 = Value<pyTuple>::take(PyTuple_Pack(3, +item(args, 0), +bases, +item(args, 2)));
    DUMP("Calling type()", args2, kwargs);
    auto out = Value<pyType>::take(PyObject_Call(~pyType::def(), +args2, +kwargs));

    auto method = Value<>::take(PyInstanceMethod_New(Py_None));
    // PyObject_SetAttrString(~out, "instance_method", +method);
    DUMP("done");
    return out;
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

void pyMeta::initialize_type(Always<pyType> o) noexcept {
    o->tp_name = "ara.Meta";
    o->tp_basicsize = sizeof(PyTypeObject);
    o->tp_doc = "Object metaclass";
    o->tp_new = api<meta_new, Always<pyType>, Always<pyTuple>, Maybe<pyDict>>;
    o->tp_base = +pyType::def();
    // o->tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
}

/******************************************************************************/

}