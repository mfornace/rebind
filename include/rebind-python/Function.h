#pragma once
#include <rebind-python/API.h>

namespace rebind::py {

/******************************************************************************/

template <class Out>
bool call_with_gil(Out &out, Ref const &fun, Arguments &&args, bool gil) {
    auto lk = std::make_shared<PythonFrame>(!gil);
    DUMP("calling the args: size=", args.size(), " ", bool(fun));
    return fun.call_to(out, Caller(lk), std::move(args));
}

/******************************************************************************/

Object call_overload(void *out, Ref const &fun, Arguments &&args, bool is_value, bool gil) {
    // if (auto py = fun.target<PythonFunction>())
        // return {PyObject_CallObject(+py->function, +args), false};
    DUMP("constructed python args ", args.size());
    for (auto const &p : args) DUMP("argument type: ", p.name(), QualifierSuffixes[p.qualifier()]);

    if (out) {
        bool ok = is_value ?
            call_with_gil(*static_cast<Value *>(out), fun, std::move(args), gil)
            : call_with_gil(*static_cast<Ref *>(out), fun, std::move(args), gil);
        if (ok) return {Py_None, true};
        else return type_error("callable failed...");
    } else {
        Value out;
        if (!call_with_gil(out, fun, std::move(args), gil))
            return type_error("callable failed 2");

        DUMP("got the output Value ", out.name(), " ", out.has_value());

        if (auto p = out.target<Object>()) return std::move(*p);
        // if (auto p = out.target<PyObject * &>()) return {*p, true};
        // Convert the C++ Value to a rebind.Value
        return value_to_object(std::move(out));
    }
}

/******************************************************************************/

Object function_call_impl(void *out, Ref const &fun, Arguments &&args, bool is_value, bool gil, Object tag) {
    DUMP("function_call_impl ", gil, " ", args.size());
    return call_overload(out, fun, std::move(args), is_value, gil);
}

/******************************************************************************/

auto function_call_keywords(PyObject *kws) {
    bool is_value, gil = true;
    void *out = nullptr;
    Object tag;

    if (kws && PyDict_Check(kws)) {
        PyObject *o = PyDict_GetItemString(kws, "output");
        if (o) {
            if ((out = cast_if<Ref>(o))) {
                is_value = false;
            } else {
                out = &cast_object<Value>(o);
                is_value = true;
            }
        }

        PyObject *g = PyDict_GetItemString(kws, "gil");
        if (g) gil = PyObject_IsTrue(g);

        tag = {not_none(PyDict_GetItemString(kws, "tag")), true};
    }
    return std::make_tuple(tag, out, is_value, gil);
}

/******************************************************************************/

// struct Method {
//     Overload fun;
//     Object self;

//     static PyObject *call(PyObject *self, PyObject *pyargs, PyObject *kws) noexcept {
//         return raw_object([=] {
//             auto const &, outs = cast_object<Method>(self);
//             auto [t0, t1, sig, gil] = function_call_keywords(kws);
//             Arguments args;
//             args.emplace_back(ref_from_object(s.self));
//             args_from_python(args, {pyargs, true});
//             return function_call_impl(s.fun, std::move(args), sig, t0, t1, gil);
//         });
//     }

//     static PyObject *make(PyObject *self, PyObject *object, PyObject *type) {
//         return raw_object([=]() -> Object {
//             if (!object) return {self, true};
//             // capture bound object
//             return default_object(Method{cast_object<Overload>(self), {object, true}});
//         });
//     }
// };

// template <>
// PyTypeObject Wrap<Method>::type = []{
//     auto o = type_definition<Method>("rebind.Method", "Bound method");
//     o.tp_call = Method::call;
//     return o;
// }();

/******************************************************************************/

Vector<Object> objects_from_argument_tuple(PyObject *args) {
    Vector<Object> out;
    auto const size = PyTuple_GET_SIZE(args);
    out.reserve(size);
    for (Py_ssize_t i = 0; i != size; ++i) out.emplace_back(PyTuple_GET_ITEM(args, i), true);
    return out;
}

/******************************************************************************/

Vector<Ref> arguments_from_objects(Vector<Object> &v) {
    Vector<Ref> out;
    out.reserve(v.size());
    for (auto &o : v) out.emplace_back(ref_from_object(o));
    return out;
}

/******************************************************************************/

/* Overload call has effectively the following signature
 * *args: the arguments to be passed to C++
 * gil (bool): whether to keep the gil on (default: True)
 * signature (int, Tuple[Index], or None): manual selection of overload to call
 * return_type (Index or None): manual selection of overload by return type
 * first_type (Index or None): manual selection overload by first type (useful for methods)
 */
PyObject * function_call(PyObject *self, PyObject *args, PyObject *kws) noexcept {
    return raw_object([=] {
        auto const [tag, out, is_value, gil] = function_call_keywords(kws);
        // // DUMP("specified types", bool(t0), bool(t1));
        DUMP("gil = ", gil, ", reference counts = ", Py_REFCNT(self), Py_REFCNT(args));
        DUMP("out = ", bool(out), ", is_value = ", is_value);
        // // DUMP("number of signatures ", cast_object<Overload>(self).overloads.size());
        auto objects = objects_from_argument_tuple(args);

        Ref ref;
        if (auto p = cast_if<Ref>(self)) ref = *p;
        else ref = Ref(cast_object<Value>(self));

        Object o = function_call_impl(out, ref, arguments_from_objects(objects), is_value, gil, tag);

        // if (o) if (auto v = cast_if<Value>(o)) {
        //     DUMP("returning Value to python ", v->has_value(), " ", v->name());
        // }
        return o;
    });
}

/******************************************************************************/

}
