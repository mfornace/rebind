#pragma once
#include "Common.h"
#include "Variable.h"
#include <cstdlib>

namespace rebind::py {

/******************************************************************************/

Object function_call_impl(Variable *out, Variable &self, ArgView &&args) {
//     // if (auto py = self.target<PythonFunction>())
//         // return {PyObject_CallObject(+py->function, +args), false};
    DUMP("constructed python args ", args.size());
    for (auto const &p : args) DUMP("argument type: ", p.name());//, " ", p.qualifier());

    if (out) {
        if (self.call_to(*out, std::move(args))) {
            return {Py_None, true};
        } else return type_error("callable failed to return in place");
    } else {
        auto obj = Variable::new_object();
        auto &out = cast_object<Variable>(obj);
        if (!self.call_to(out, std::move(args)))
            return type_error("callable failed to return");

        DUMP("got the output Value ", out.name(), " ", out.has_value());

        // if (auto p = out.target<Object>()) return std::move(*p);
        // if (auto p = out.target<PyObject * &>()) return {*p, true};
        // Convert the C++ Value to a rebind.Value
        return obj;
    }
}

/******************************************************************************/

auto function_call_keywords(PyObject *kws) {
    bool gil = true;
    Variable *out = nullptr;
    Object tag;

    if (kws && PyDict_Check(kws)) {
        PyObject *o = PyDict_GetItemString(kws, "output");
        if (o) out = cast_if<Variable>(o);

        PyObject *g = PyDict_GetItemString(kws, "gil");
        if (g) gil = PyObject_IsTrue(g);

        tag = {not_none(PyDict_GetItemString(kws, "tag")), true};
    }
    return std::make_tuple(tag, out, gil);
}

/******************************************************************************/

Vector<Object> objects_from_argument_tuple(PyObject *args) {
    Vector<Object> out;
    auto const size = PyTuple_GET_SIZE(args);
    out.reserve(size);
    for (Py_ssize_t i = 0; i != size; ++i) out.emplace_back(PyTuple_GET_ITEM(args, i), true);
    return out;
}

/******************************************************************************/

template <class Iter>
Vector<Ref> arguments_from_objects(Caller &c, std::string_view name, Ref tag, Iter b, Iter e) {
    Vector<Ref> out;
    out.reserve(e + 3 - b);
    out.emplace_back(c);
    rebind_str str{name.data(), name.size()};
    out.emplace_back(reinterpret_cast<Ref &&>(str));
    // out.emplace_back(tag);
    for (; b != e; ++b) out.emplace_back(ref_from_object(*b));
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
        auto const [tag, out, gil] = function_call_keywords(kws);
        // // DUMP("specified types", bool(t0), bool(t1));
        DUMP("gil = ", gil, ", reference counts = ", Py_REFCNT(self), Py_REFCNT(args));
        DUMP("out = ", bool(out));
        // // DUMP("number of signatures ", cast_object<Overload>(self).overloads.size());
        // auto argv = objects_from_argument_tuple(args);

        auto lk = std::make_shared<PythonFrame>(!gil);
        Caller c(lk);

        // auto vec = arguments_from_objects(c, "", Ref(tag), argv.begin(), argv.end());
        // auto vec = arguments_from_objects(c, argv.begin(), argv.end());

        if (auto p = cast_if<Variable>(self)) {
            DUMP("calling variable");
            auto const size = PyTuple_GET_SIZE(args);
            static_assert(sizeof(ArgStack<1>) % sizeof(void*) == 0);
            static_assert(sizeof(rebind_ref) % sizeof(void*) == 0);
            static_assert(sizeof(rebind_ref) % sizeof(void*) == 0);
            static_assert(alignof(ArgStack<1>) <= sizeof(void*));
            auto alloc = std::make_unique<void*[]>((sizeof(ArgStack<1>) - sizeof(rebind_ref) + size * sizeof(rebind_ref)) / sizeof(void*));//, std::align_val_t(alignof(rebind_args)));
            DUMP("ok...");
            auto &view = *reinterpret_cast<ArgView *>(alloc.get());
            DUMP("ok...");
            view.caller_ptr = &c;
            view.args = size;

            Py_ssize_t i = 0;
            for (auto &arg : view) {arg = Ref(*reinterpret_cast<VersionedObject *>(PyTuple_GET_ITEM(args, i))); ++i;}
            return function_call_impl(out, *p, std::move(view));
        }
        return Object();
    });
}

/******************************************************************************/

}
