#pragma once
#include <rebind-python/API.h>

namespace rebind::py {

/******************************************************************************/

template <class Out>
void call_with_gil(Out &out, Overload const &fun, Arguments &&args, bool gil) {
    auto lk = std::make_shared<PythonFrame>(!gil);
    DUMP("calling the args: size=", args.size(), " ", bool(fun));
    fun.call_in_place(out, Caller(lk), std::move(args));
}

/******************************************************************************/

Object call_overload(void *out, Overload const &fun, Arguments &&args, bool is_value, bool gil) {
    // if (auto py = fun.target<PythonFunction>())
    //     return {PyObject_CallObject(+py->function, +args), false};
    DUMP("constructed python args ", args.size());
    for (auto const &p : args) DUMP("argument type: ", p.name(), QualifierSuffixes[p.qualifier()]);

    if (out) {
        if (is_value) {
            call_with_gil(*static_cast<Value *>(out), fun, std::move(args), gil);
        } else {
            call_with_gil(*static_cast<Ref *>(out), fun, std::move(args), gil);
        }
        return {Py_None, true};
    } else {
        Value out;
        call_with_gil(out, fun, std::move(args), gil);
        DUMP("got the output Value ", out.name(), " ", out.has_value());
        if (auto p = out.target<Object>()) return std::move(*p);
        // if (auto p = out.target<PyObject * &>()) return {*p, true};
        // Convert the C++ Value to a rebind.Value
        return value_to_object(std::move(out));
    }
}

/******************************************************************************/

Object function_call_impl(void *out, Function const &fun, Arguments &&args, bool is_value, bool gil, Object tag) {
    DUMP("function_call_impl ", gil, " ", args.size());
    return call_overload(out, fun.first, std::move(args), is_value, gil);
}

    // throw std::runtime_error("not implemented");
    // return {};
    // auto const &overloads = fun.overloads;

    // if (overloads.size() == 1) // only 1 overload
    //     return call_overload(overloads[0].second, args, gil);

    // if (sig && PyLong_Check(sig)) { // signature given as an integer index
    //     auto i = PyLong_AsLongLong(sig);
    //     if (i < 0) i += overloads.size();
    //     if (i <= overloads.size() || i < 0)
    //         return call_overload(overloads[i].second, std::move(args), gil);
    //     PyErr_SetString(PyExc_IndexError, "signature index out of bounds");
    //     return Object();
    // }

    // auto errors = Object::from(PyList_New(0));

    // //  Check for equivalence on the first argument first -- provides short-circuiting for methods
    // for (auto const exact : {true, false}) {
    //     for (auto const &o : overloads) {
    //         bool const match = (o.first.size() < 2) || (!args.empty() && args[0].type().matches(o.first[1]));
    //         if (match != exact) continue;
    //         if (sig) { // check the explicit signature that was passed in
    //             if (PyTuple_Check(sig)) {
    //                 auto const len = PyObject_Length(sig);
    //                 if (len > o.first.size())
    //                     return type_error("C++: too many types given in signature");
    //                 for (Py_ssize_t i = 0; i != len; ++i) {
    //                     PyObject *x = PyTuple_GET_ITEM(sig, i);
    //                     if (x != Py_None && !cast_object<Index>(x).matches(o.first[i])) continue;
    //                 }
    //             } else return type_error("C++: expected 'signature' to be a tuple");
    //         } else {
    //             if (t0 && o.first.size() > 0 && !o.first[0].matches(t0)) continue; // check that the return type matches if specified
    //             if (t1 && o.first.size() > 1 && !o.first[1].matches(t1)) continue; // check that the first argument type matches if specified
    //         }

    //         try {
    //             return call_overload(o.second, args, gil);
    //         } catch (WrongType const &e) {
    //             if (PyList_Append(+errors, +as_object(wrong_type_message(e)))) return {};
    //         } catch (WrongNumber const &e) {
    //             unsigned int n0 = e.expected, n = e.received;
    //             auto s = Object::from(PyUnicode_FromFormat("C++: wrong number of arguments (expected %u, got %u)", n0, n));
    //             if (PyList_Append(+errors, +s)) return {};
    //         } catch (DispatchError const &e) {
    //             if (PyList_Append(+errors, +as_object(std::string_view(e.what())))) return {};
    //         }
    //     }
    // }
    // // Raise an exception with a list of the messages
    // return PyErr_SetObject(TypeError, +errors), nullptr;

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
         // either int or Tuple[Index] or None
    //     auto r = not_none(PyDict_GetItemString(kws, "return_type")); // either Index or None
    //     auto f = not_none(PyDict_GetItemString(kws, "first_type")); // either Index or None
    //     // std::cout << PyObject_Length(kws) << bool(g) << bool(sig) << bool(f) << bool(r) << std::endl;
    //     // if (PyObject_Length(kws) != unsigned(bool(g)) + (sig || r || f))
    //         // return type_error("C++: unexpected extra keywords");
    //     if (f) t1 = cast_object<Index>(f);
    }
    return std::make_tuple(tag, out, is_value, gil);
}

/******************************************************************************/

// struct AnnotatedFunction {
//     struct Annotation {
//         std::string name;
//         std::vector<Object> callback;
//         Object type;
//     };
//     Overload function;
//     std::string format;
//     std::vector<Annotation> annotations;
//     std::size_t max_positional=0;
//     Object return_type;
// };

// struct AnnotatedMethod {
//     AnnotatedFunction function;
//     Object self;
// };// args, then parse keywords

// PyObject *function_annotated_impl(PyObject *data, PyObject *args, PyObject *kws) noexcept {
//     return raw_object([=]() -> Object {
//         auto const &v = *cast_object<Value>(data).target<AnnotatedFunction>();
//         if (PyTuple_GET_SIZE(args) > v.max_positional) return nullptr;
//         std::vector<PyObject *> args;
//         std::vector<char const *> names;
//         // PyArg_VaParseTupleAndKeywords(args, kws, v.format.data(), va_list); with defaults

//         Arguments arguments;
//         for (auto const &p : v.annotations) {
//             arguments.emplace_back(); // make_callback = make function that wraps it but casts to the given argument types
//         }
//         Value out;// = function(arguments);

//         if (!v.return_type) return value_to_object(std::move(out)); // no cast
//         else if (v.return_type == Py_None || +v.return_type == reinterpret_cast<PyObject const *>(Py_None->ob_type)) return {Py_None, true}; // cast to None
//         else return cast_to_object(Ref::from(std::move(out)), v.return_type, Object());
//     });
// }

// // take Overload, argument names, argument defaults, annotations, # keyword arguments, return_type
// // return function which casts arguments into correct types and order and calls self with the resultant *args
// PyMethodDef function_annotated_ml = {"annotated",
//     reinterpret_cast<PyCFunction>(&function_annotated_impl),
//     METH_VARARGS|METH_KEYWORDS,
//     "annotated(a, b)\nannotated function wrapper"
// };

// PyObject *function_annotated(PyObject *self, PyObject *args) noexcept {
//     return raw_object([=] {
//         AnnotatedFunction a;
//         a.function = cast_object<Overload>(self);
//         // std::cout << function_annotated_ml.ml_name << function_annotated_ml.ml_doc << std::endl;
//         return Object::from(PyCFunction_New(&function_annotated_ml, value_to_object(std::move(a))));
//         // auto c = _PyType_GetTextSignatureFromInternalDoc(function_annotated_ml.ml_name, function_annotated_ml.ml_doc);
//         // if (c) print(c);
//         // return o;
//     });
// }

/******************************************************************************/

// struct DelegatingMethod {
//     Object function, wrapping, captured_self;

//     static PyObject *call(PyObject *self, PyObject *args, PyObject *kws) noexcept {
//         return raw_object([=] {
//             auto const &s = cast_object<DelegatingMethod>(self);
//             std::size_t const n = PyTuple_GET_SIZE(+args) + 1;
//             auto args2 = Object::from(PyTuple_New(n));
//             if (!set_tuple_item(args2, 0, s.captured_self)) return Object();
//             for (std::size_t i = 1; i != n; ++i) if (!set_tuple_item(args2, i, PyTuple_GET_ITEM(+args, i-1))) return Object();
//             auto kws2 = Object::from(kws ? PyDict_Copy(kws) : PyDict_New());
//             if (PyDict_SetItemString(kws2, "_fun_", +s.function)) return Object();
//             return Object::from(PyObject_Call(s.wrapping, args2, kws2));
//         });
//     };
// };

// template <>
// PyTypeObject Wrap<DelegatingMethod>::type = []{
//     auto t = type_definition<DelegatingMethod>("rebind.DelegatingMethod", "C++ delegating method");
//     t.tp_call = DelegatingMethod::call;
//     return t;
// }();

/******************************************************************************/

// struct DelegatingFunction {
//     Object function, wrapping;

//     static PyObject *call(PyObject *self, PyObject *args, PyObject *kws) noexcept {
//         return raw_object([=] {
//             auto const &s = cast_object<DelegatingFunction>(self);
//             auto kws2 = Object::from(kws ? PyDict_Copy(kws) : PyDict_New());
//             if (PyDict_SetItemString(kws2, "_fun_", +s.function)) return Object();
//             return Object::from(PyObject_Call(s.wrapping, args, kws2));
//         });
//     };

//     static PyObject *get(PyObject *self, PyObject *object, PyObject *type) noexcept {
//         return raw_object([=] {
//             if (!object) return Object(self, true);
//             auto const &s = cast_object<DelegatingFunction>(self);
//             return default_object(DelegatingMethod{s.function, s.wrapping, {object, true}});
//         });
//     };

//     // take Overload, old function
//     // return a function that takes *args, **kwargs and calls old with new
//     // PyMethodDef DelegatingMethod = {"delegating_function", reinterpret_cast<PyCFunction>(&function_delegating_impl), METH_VARARGS|METH_KEYWORDS, "delegating function wrapper"};
//     static PyObject *make(PyObject *self, PyObject *old) noexcept {
//         return raw_object([=] {return default_object(DelegatingFunction{{self, true}, {old, true}});});
//     }
// };

// template <>
// PyTypeObject Wrap<DelegatingFunction>::type = []{
//     auto t = type_definition<DelegatingFunction>("rebind.DelegatingFunction", "C++ delegating function");
//     t.tp_call = DelegatingFunction::call;
//     t.tp_descr_get = DelegatingFunction::get;
//     return t;
// }();

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

Arguments arguments_from_objects(Vector<Object> &v) {
    Arguments out;
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
        // DUMP("specified types", bool(t0), bool(t1));
        DUMP("gil = ", gil, ", reference counts = ", Py_REFCNT(self), Py_REFCNT(args));
        DUMP("out = ", bool(out), ", is_value = ", is_value);
        // DUMP("number of signatures ", cast_object<Overload>(self).overloads.size());
        auto objects = objects_from_argument_tuple(args);

        Object o = function_call_impl(out, cast_object<Function>(self),
            arguments_from_objects(objects), is_value, gil, tag);
        // call_with_gil(out, fun, std::move(args), gil);
        if (auto v = cast_if<Value>(o)) {
            DUMP("returning Value to python ", v->has_value(), " ", v->name());
        }
        return o;
    });
}

/******************************************************************************/

// PyObject * function_signatures(PyObject *self, PyObject *) noexcept {
//     return raw_object([=] {
//         return Object();
//         // return map_as_tuple(cast_object<Overload>(self).overloads, [](auto const &p) -> Object {
//         //     if (!p.first) return {Py_None, true};
//         //     return map_as_tuple(p.first, [](auto const &o) {return as_object(o);});
//         // });
//     });
// }

/******************************************************************************/

int function_init(PyObject *self, PyObject *args, PyObject *kws) noexcept {
    static char const * keys[] = {"function", "signature", nullptr};
    PyObject *fun = nullptr;
    PyObject *sig = nullptr;
    // if (!PyArg_ParseTupleAndKeywords(args, kws, "|OO", const_cast<char **>(keys), &fun, &sig))
    //     return -1;
    // if (!fun || +fun == Py_None) return 0;

    // if (!PyCallable_Check(fun))
    //     return type_error("Expected callable type but got %R", fun->ob_type), -1;
    // if (sig && sig != Py_None && !PyTuple_Check(sig))
    //     return type_error("Expected signature to be tuple or None but got %R", sig->ob_type), -1;
    // cast_object<Overload>(self).emplace(PythonFunction(Object(fun, true), Object(sig ? sig : Py_None, true)), {});
    return 0;
}

/******************************************************************************/

}
