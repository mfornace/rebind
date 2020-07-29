#pragma once
#include "Variable.h"
#include <deque>
#include <structmember.h>

namespace ara::py {

/******************************************************************************/

// PySequenceMethods VariableSequenceMethods = {
//     .sq_item = c_variable_element
// };

// struct DynamicType {
//     struct Member {
//         std::string name;
//         std::string doc = "doc string";
//         Value<> annotation;
//         Member(std::string_view s, Value<> a, Value<pyStr> doc)
//             : name(s), annotation(std::move(a)) {}
//     };
//     std::unique_ptr<PyTypeObject> object;
//     std::optional<PySequenceMethods> sequence;
//     std::optional<PyNumberMethods> number;
//     std::optional<PyMappingMethods> mapping;

//     std::vector<PyGetSetDef> getsets;
//     std::vector<PyMethodDef> methods;
//     std::vector<PyMemberDef> member_defs;

//     std::string name = "ara.extension.";
//     std::deque<Member> members;

//     void add_members(Always<pyDict> annotations, Always<pyDict> properties);

//     DynamicType()
//         : object(std::unique_ptr<PyTypeObject>(new PyTypeObject{PyVarObject_HEAD_INIT(NULL, 0)})) {
//         define_type<pyVariable>(*object, "ara.DerivedVariable", "low-level base inheriting from ara.Variable");
//     }

//     void finalize(Always<pyTuple> args);
// };

// extern std::deque<DynamicType> dynamic_types;

/******************************************************************************/

struct Method {
    Value<> tag;       // optional
    Value<pyStr> mode; // optional
    Value<> out;       // optional
    // Value<> signature;
    // Value<> docstring;
    // Value<> doc;
    // std::string name;
};

struct pyMethod : StaticType<pyMethod> {
    using type = Subtype<Method>;

    // static PyMemberDef members[];

    // static Value<> repr(Always<pyMethod> self) {return Always<>(*self->doc);}

    // static Value<> str(Always<pyMethod> self) {return Always<>(*self->docstring);}

    static Value<> get(Always<pyMethod> self, Maybe<> instance, Ignore);

    static Value<> call(Always<pyMethod> self, Always<pyTuple> args, Maybe<pyDict> kws) {
        return {};
    }

    static void initialize_type(Always<pyType> o) noexcept {
        define_type<pyMethod>(o, "ara.Method", "ara Method type");
        // o->tp_repr = reinterpret<repr, Always<pyMethod>>;
        // o->tp_str = reinterpret<str, Always<pyMethod>>;
        o->tp_call = reinterpret_kws<call, Always<pyMethod>>;
        o->tp_descr_get = reinterpret<get, nullptr, Always<pyMethod>, Maybe<>, Ignore>;
        // o->tp_members = members;
        // tp_traverse, tp_clear
        // PyMemberDef, tp_members
    }

    static void placement_new(Method &m, Maybe<> tag, Maybe<pyStr> mode, Maybe<> out) {
        new(&m) Method{tag, mode, out};
    }

    static void placement_new(Method &m, Always<pyTuple> args, Maybe<pyDict> kws) {
        auto [tag, mode, out] = parse<0, Object, pyStr, Object>(args, kws, {"tag", "mode", "out"});
        placement_new(m, tag, mode, out);
    }
};

/******************************************************************************/

struct BoundMethod {
    Bound<pyMethod> method;
    Bound<pyVariable> self;
};

struct pyBoundMethod : StaticType<pyBoundMethod> {
    using type = Subtype<BoundMethod>;

    static void initialize_type(Always<pyType> t) {
        define_type<pyBoundMethod>(t, "ara.BoundMethod", "ara BoundMethod type");
        t->tp_call = reinterpret_kws<call, Always<pyBoundMethod>>;
    }

    static Value<> call(Always<pyBoundMethod>, Always<pyTuple>, Maybe<pyDict>) {
        return {};
    }
    
    static void placement_new(BoundMethod &f, Always<pyMethod> m, Always<pyVariable> s) noexcept {
        new(&f) BoundMethod{m, s};
    }

    static void placement_new(BoundMethod &f, Always<pyTuple> args, Maybe<pyDict> kws) {
        auto [m, s] = parse<2, pyMethod, pyVariable>(args, kws, {"method", "instance"});
        placement_new(f, m, s);
    }
};

/******************************************************************************/

Value<> pyMethod::get(Always<pyMethod> self, Maybe<> instance, Ignore) {
    if (instance) {
        return Value<pyBoundMethod>::new_from(self, Always<pyVariable>::from(*instance));
    } else return self;
}

/******************************************************************************/

struct Forward {
    Value<pyVariable> impl; // optional
    Value<> tag; // optional
    Value<pyStr> mode; // optional
    Value<> out; // optional
};

struct pyForward : StaticType<Forward> {
    using type = Subtype<Forward>;

    static void initialize_type(Always<pyType> t) {
        define_type<pyForward>(t, "ara.Forward", "ara Forward type");
        t->tp_call = reinterpret_kws<call, Always<pyForward>>;
    }

    static void placement_new(Forward &f, Always<pyTuple> args, Maybe<pyDict> kws) {
        auto [mode, tag, out] = parse<1, pyStr, Object, Object>(args, kws, {"mode", "tag", "out"});
        new(&f) Forward{{}, tag, mode, out};
    }

    static void placement_new(Forward &f, Always<pyVariable> v, Maybe<pyStr> mode, Maybe<> out) {
        new(&f) Forward{v, {}, mode, out};
    }

    static Value<pyMethod> call(Always<pyForward> f, Always<pyTuple> args, Maybe<pyDict> kws) {
        auto [fun] = parse<1, Object>(args, kws, {"function"});
        Value<> tag = f->tag ? f->tag : Value<>::take(PyObject_GetAttrString(~fun, "__name__"));
        Value<> out;

        if (f->out) {
            out = f->out;
        } else {
            auto a = Value<>::take(PyObject_GetAttrString(~fun, "__annotations__"));
            out = item(Always<pyDict>::from(*a), "return");
        }
        
        return Value<pyMethod>::new_from(std::move(tag), f->mode, std::move(out));
    }
};

/******************************************************************************/

struct Function {

};

struct pyFunction : StaticType<Function> {
    using type = Subtype<Function>;

    static void initialize_type(Always<pyType> t) {
        define_type<pyFunction>(t, "ara.Function", "ara Function type");
        t->tp_call = reinterpret_kws<call, Always<pyFunction>>;
    }

    static void placement_new(Function &f, Always<pyTuple> args, Maybe<pyDict> kws) {
        new(&f) Function();
    }

    static Value<> call(Always<pyFunction> f, Always<pyTuple> args, Maybe<pyDict> kws) {
        return {};
    }
};

/******************************************************************************/

struct Member {
    Bound<pyStr> name;
    Value<> out; // optional
    Value<pyStr> doc; // optional
};

struct pyMember : StaticType<Member> {
    using type = Subtype<Member>;

    static void initialize_type(Always<pyType> t) {
        define_type<pyMember>(t, "ara.Member", "ara Member type");
        t->tp_descr_get = reinterpret<get, nullptr, Always<pyMember>, Maybe<>, Ignore>;
    }

    static Value<> get(Always<pyMember> self, Maybe<> instance, Ignore) {
        if (!instance) return self;
        return {};
        // CallKeywords kws;
        // return variable_access(Always<pyVariable>::from(*instance), self->name, kws);
    }

    static void placement_new(Member &f, Always<pyTuple> args, Maybe<pyDict> kws) {
        auto [name, out, doc] = parse<1, pyStr, Object, pyStr>(args, kws, {"name", "out", "doc"});
        new(&f) Member{name, out, doc};
    }
};

/******************************************************************************/

}