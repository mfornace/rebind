#include <cpy/CxxPython.h>
#include <cpy/Suite.h>
#include <chrono>
#include <iostream>
#include <vector>

namespace cpy {

/******************************************************************************/

StreamSync cout_sync{std::cout, std::cout.rdbuf()};
StreamSync cerr_sync{std::cerr, std::cerr.rdbuf()};

/******************************************************************************/

PythonError python_error() noexcept {
    PyObject *type, *value, *traceback;
    PyErr_Fetch(&type, &value, &traceback);
    PyObject *str = PyObject_Str(value);
    char const *c = nullptr;
    if (str) {
#       if PY_MAJOR_VERSION > 2
            c = PyUnicode_AsUTF8(str); // PyErr_Clear
#       else
            c = PyString_AsString(str);
#       endif
        Py_DECREF(str);
    }
    PyErr_Restore(type, value, traceback);
    return PythonError(c ? c : "Python error with failed str()");
}

/******************************************************************************/

bool from_python(Value &v, Object o) {
    if (+o == Py_None) {
        v = std::monostate();
    } else if (PyBool_Check(+o)) {
        v = (+o == Py_True) ? true : false;
    } else if (PyLong_Check(+o)) {
        long long i = PyLong_AsLongLong(+o);
        if (i < 0) v = static_cast<std::ptrdiff_t>(i);
        else v = static_cast<std::size_t>(i);
    } else if (PyFloat_Check(+o)) {
        double d = PyFloat_AsDouble(+o);
        v = d;
    } else if (PyComplex_Check(+o)) {
        v = std::complex<double>{PyComplex_RealAsDouble(+o), PyComplex_ImagAsDouble(+o)};
    } else if (PyBytes_Check(+o)) {
        char *c;
        Py_ssize_t size;
        PyBytes_AsStringAndSize(+o, &c, &size);
        v = std::string_view(c, size);
    } else if (PyUnicode_Check(+o)) { // no use of wstring for now.
        Py_ssize_t size;
#if PY_MAJOR_VERSION > 2
        char const *c = PyUnicode_AsUTF8AndSize(+o, &size);
#else
        char *c;
        if (PyString_AsStringAndSize(+o, &c, &size)) return false;
#endif
        if (c) v = std::string_view(static_cast<char const *>(c), size);
        else return false;
    } else {
        PyErr_SetString(PyExc_TypeError, "Invalid type for conversion to C++");
    }
    return !PyErr_Occurred();
};

/******************************************************************************/

bool build_argpack(ArgPack &pack, Object pypack) {
    return cpy::build_vector(pack, pypack, [](cpy::Object &&o, bool &ok) {
        cpy::Value v;
        ok = ok && cpy::from_python(v, std::move(o));
        return v;
    });
}

/******************************************************************************/

bool build_callbacks(std::vector<Callback> &v, Object calls) {
    return build_vector(v, calls, [](Object &&o, bool) -> Callback {
        if (o.ptr == Py_None) return {};
        return PyCallback{std::move(o)};
    });
}

/******************************************************************************/

bool run_test(Value &v, double &time, TestCase const &test, bool no_gil,
              std::vector<Counter> &counts, std::vector<Callback> callbacks, ArgPack pack) {
    no_gil = no_gil && !test.function.target<PyTestCase>();
    ReleaseGIL lk(no_gil);
    if (no_gil) for (auto &c : callbacks)
        if (c) c.target<PyCallback>()->unlock = &lk;

    for (auto &c : counts) c.store(0u);

    Context ctx({test.name}, std::move(callbacks), &counts, &lk);
    Timer t(time);
    return test.function(v, std::move(ctx), std::move(pack));
}

/******************************************************************************/

TestCase *get_test(Py_ssize_t i) {
    if (i >= cpy::suite().size()) {
        PyErr_SetString(PyExc_IndexError, "Unit test index out of range");
        return nullptr;
    }
    return &cpy::suite()[i];
}

Object run_test(Py_ssize_t i, Object calls, Object pypack, bool cout, bool cerr, bool no_gil) {
    auto const test = cpy::get_test(i);
    if (!test) return {};

    std::vector<cpy::Callback> callbacks;
    if (!cpy::build_callbacks(callbacks, std::move(calls))) return {};

    cpy::ArgPack pack;
    if (+pypack == Py_None) {}
#if PY_MAJOR_VERSION > 2
    else if (PyLong_Check(+pypack)) {
        auto n = PyLong_AsSize_t(+pypack);
#else
    else if (PyInt_Check(+pypack)) {
        auto n = PyInt_AsSsize_t(+pypack);
#endif
        if (PyErr_Occurred()) return {};
        if (n >= test->parameters.size()) {
            PyErr_SetString(PyExc_IndexError, "Parameter pack index out of range");
            return {};
        }
        pack = test->parameters[n];
    } else if (!build_argpack(pack, std::move(pypack))) return {};

    std::stringstream out, err;

    cpy::Value v;
    double test_time = 0;
    std::vector<cpy::Counter> counters(callbacks.size());

    {
        cpy::RedirectStream o(cpy::cout_sync, cout ? out.rdbuf() : nullptr);
        cpy::RedirectStream e(cpy::cerr_sync, cerr ? err.rdbuf() : nullptr);
        if (!cpy::run_test(v, test_time, *test, no_gil, counters,
                           std::move(callbacks), std::move(pack))) {
            if (!PyErr_Occurred())
                PyErr_SetString(PyExc_TypeError, "Invalid unit test argument types");
            return {};
        }
    }

    auto value = cpy::to_python(v);
    if (!value) return {};
    auto timed = cpy::to_python(test_time);
    if (!timed) return {};
    auto counts = cpy::to_python(counters, [](auto const &c) {return c.load();});
    if (!counts) return {};
    auto pyout = cpy::to_python(out.str());
    if (!pyout) return {};
    auto pyerr = cpy::to_python(err.str());
    if (!pyerr) return {};
    return {PyTuple_Pack(5u, +value, +timed, +counts, +pyout, +pyerr), false};
}

/******************************************************************************/

template <class F>
PyObject * return_object(F &&f) noexcept {
    try {
        Object o = static_cast<F &&>(f)();
        Py_XINCREF(+o);
        return +o;
    } catch (PythonError const &) {
        return nullptr;
    } catch (std::bad_alloc const &e) {
        PyErr_Format(PyExc_MemoryError, "C++ out of memory with message %s", e.what());
    } catch (std::exception const &e) {
        if (!PyErr_Occurred())
            PyErr_Format(PyExc_RuntimeError, "C++ exception with message %s", e.what());
    } catch (...) {
        if (!PyErr_Occurred())
            PyErr_SetString(PyExc_RuntimeError, "Unknown C++ exception");
    }
    return nullptr;
}

/******************************************************************************/

}

extern "C" {

/******************************************************************************/

PyObject *cpy_run_test(PyObject *self, PyObject *args) {
    Py_ssize_t i;
    PyObject *calls, *pack, *cout, *cerr, *gil;
    if (!PyArg_ParseTuple(args, "nOOOOO", &i, &calls, &pack, &gil, &cout, &cerr))
        return nullptr;
    return cpy::return_object([&] {
        auto ret = cpy::run_test(i, {calls, true}, {pack, true},
            PyObject_IsTrue(+cout), PyObject_IsTrue(+cerr), PyObject_Not(+gil));
        return ret;
    });
}

/******************************************************************************/

PyObject *cpy_n_tests(PyObject *, PyObject *args) {
    Py_ssize_t n = cpy::suite().size();
    return Py_BuildValue("n", n);
}

/******************************************************************************/

PyObject *cpy_add_test(PyObject *, PyObject *args) {
    char const *s;
    PyObject *fun, *pypacks = nullptr;
    if (!PyArg_ParseTuple(args, "sO|O", &s, &fun, &pypacks)) return nullptr;

    return cpy::return_object([=] {
        std::vector<cpy::ArgPack> packs;
        if (pypacks) {
            cpy::build_vector(packs, {pypacks, true}, [](cpy::Object &&o, bool &ok) {
                cpy::ArgPack pack;
                ok &= cpy::build_argpack(pack, std::move(o));
                return pack;
            });
        }
        cpy::suite().emplace_back(cpy::TestCase{s, {}, cpy::PyTestCase(fun, true), std::move(packs)});
        return cpy::Object(Py_None, true);
    });
}

PyObject *cpy_add_value(PyObject *, PyObject *args) {
    char const *s;
    PyObject *obj;
    if (!PyArg_ParseTuple(args, "sO", &s, &obj)) return nullptr;

    return cpy::return_object([=] {
        cpy::Value val;
        if (!cpy::from_python(val, cpy::Object(obj, true))) return cpy::Object();
        cpy::suite().emplace_back(cpy::TestCase{s, {}, cpy::ValueAdaptor{std::move(val)}});
        return cpy::Object(Py_None, true);
    });
}

/******************************************************************************/

PyObject *cpy_compile_info(PyObject *, PyObject *) {
    auto v = cpy::to_python(__VERSION__ "");
    auto d = cpy::to_python(__DATE__ "");
    auto t = cpy::to_python(__TIME__ "");
    return (v && d && t) ? PyTuple_Pack(3u, +v, +d, +t) : nullptr;
}

/******************************************************************************/

PyObject *cpy_test_names(PyObject *, PyObject *) {
    return cpy::return_object([] {
        return cpy::to_python(cpy::suite(),
            [](auto const &c) -> decltype(c.name) {return c.name;}
        );
    });
}

/******************************************************************************/

PyObject *cpy_find_test(PyObject *self, PyObject *args) {
    char const *s;
    if (!PyArg_ParseTuple(args, "s", &s)) return nullptr;
    return cpy::return_object([s] {
        std::string_view name{s};
        auto const &cases = cpy::suite();
        for (std::size_t i = 0; i != cases.size(); ++i)
            if (cases[i].name == name) return cpy::to_python(i);
        PyErr_SetString(PyExc_KeyError, "Test name not found");
        return cpy::Object();
    });
}

/******************************************************************************/

PyObject *cpy_n_parameters(PyObject *, PyObject *args) {
    Py_ssize_t i;
    if (!PyArg_ParseTuple(args, "n", &i)) return nullptr;
    auto c = cpy::get_test(i);
    if (!c) return nullptr;
    return Py_BuildValue("n", static_cast<Py_ssize_t>(c->parameters.size()));
}

/******************************************************************************/

PyObject *cpy_test_info(PyObject *self, PyObject *args) {
    Py_ssize_t i;
    if (!PyArg_ParseTuple(args, "n", &i)) return nullptr;
    auto c = cpy::get_test(i);
    if (!c) return nullptr;
    auto n = cpy::to_python(c->name);
    if (!n) return nullptr;
    auto f = cpy::to_python(c->comment.location.file);
    if (!f) return nullptr;
    auto l = cpy::to_python(static_cast<std::size_t>(c->comment.location.line));
    if (!l) return nullptr;
    auto o = cpy::to_python(c->comment.comment);
    if (!o) return nullptr;
    return PyTuple_Pack(4u, +n, +f, +l, +o);
}

/******************************************************************************/

}

