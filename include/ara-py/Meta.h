#pragma once
#include "Variable.h"
#include <deque>
#include <structmember.h>

namespace ara::py {

/******************************************************************************/

struct pyMeta : StaticType<pyMeta> {
    using type = PyTypeObject;

    static void initialize_type(Always<pyType>) noexcept;

    static Value<pyType> new_type(Ignore, Always<pyTuple> args, Maybe<pyDict> kwargs);
};

/******************************************************************************/

// PySequenceMethods VariableSequenceMethods = {
//     .sq_item = c_variable_element
// };

struct DynamicType {
    struct Member {
        std::string name;
        std::string doc = "doc string";
        Value<> annotation;
        Member(std::string_view s, Value<> a, Value<pyStr> doc)
            : name(s), annotation(std::move(a)) {}
    };
    std::unique_ptr<PyTypeObject> object;
    std::optional<PySequenceMethods> sequence;
    std::optional<PyNumberMethods> number;
    std::optional<PyMappingMethods> mapping;

    std::vector<PyGetSetDef> getsets;
    std::vector<PyMethodDef> methods;
    std::vector<PyMemberDef> member_defs;

    std::string name = "ara.extension.";
    std::deque<Member> members;

    void add_members(Always<pyDict> annotations, Always<pyDict> properties);

    DynamicType()
        : object(std::unique_ptr<PyTypeObject>(new PyTypeObject{PyVarObject_HEAD_INIT(NULL, 0)})) {
        define_type<pyVariable>(*object, "ara.DerivedVariable", "low-level base inheriting from ara.Variable");
    }

    void finalize(Always<pyTuple> args);
};

extern std::deque<DynamicType> dynamic_types;

/******************************************************************************/

struct BoundMethod {
    PyObject *method;
    PyObject *self;
    BoundMethod(Value<> m, Value<> s) noexcept : method(m.leak()), self(s.leak()) {}
    ~BoundMethod() noexcept {Py_DECREF(method); Py_DECREF(self);}
};

/******************************************************************************/

struct Method {
    PyObject* signature;
    PyObject* docstring;
    PyObject* doc;
    std::string name;

    ~Method() noexcept {Py_DECREF(signature); Py_DECREF(docstring); Py_DECREF(doc);}
};

struct MethodObject : ObjectBase, Method {};

struct pyMethod : StaticType<pyMethod> {
    using type = MethodObject;

    static PyMemberDef members[];

    static Value<> repr(Always<pyMethod> self) {return Always<>(*self->doc);}

    static Value<> str(Always<pyMethod> self) {return Always<>(*self->docstring);}

    static Value<> get(Always<pyMethod> self, Maybe<> instance, Ignore) {
        if (instance) {
            BoundMethod(self, *instance);
            return {};
        } else return self;
    }

    static Value<> call(Always<pyMethod> self, Always<pyTuple> args, Maybe<pyDict> kws) {
        return {};
    }

    static void initialize_type(Always<pyType> o) noexcept;

    static void placement_new(MethodObject &) noexcept {}
};


#define ARA_WRAP_OFFSET(type, member) offsetof(Wrap< type >, value) + offsetof(Method, member)

PyMemberDef pyMethod::members[] = {
    // const_cast<char*>("__signature__"), T_OBJECT_EX, ARA_WRAP_OFFSET(Method, signature), READONLY, const_cast<char*>("method signature"),
    // const_cast<char*>("docstring"), T_OBJECT_EX, ARA_WRAP_OFFSET(Method, docstring), READONLY, const_cast<char*>("doc string"),
    // const_cast<char*>("doc"), T_OBJECT_EX, ARA_WRAP_OFFSET(Method, doc), READONLY, const_cast<char*>("doc"),
    nullptr
};


void pyMethod::initialize_type(Always<pyType> o) noexcept {
    define_type<pyMethod>(o, "ara.Method", "ara Method type");
    o->tp_repr = reinterpret<repr, Always<pyMethod>>;
    o->tp_str = reinterpret<str, Always<pyMethod>>;
    o->tp_descr_get = reinterpret<get, Always<pyMethod>, Maybe<>, Ignore>;
    o->tp_call = reinterpret<call, Always<pyMethod>, Always<pyTuple>, Maybe<pyDict>>;
    o->tp_members = members;
    // tp_traverse, tp_clear
    // PyMemberDef, tp_members
};


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

}