#include <cpy/PythonAPI.h>
#include <cpytest/Suite.h>
#include <cpytest/Binding.h>
#include <chrono>
#include <iostream>
#include <vector>

namespace cpy {

/******************************************************************************/

bool build_handlers(Vector<Handler> &v, Object calls) {
    return map_iterable(calls, [&v](Object o) {
        if (o.ptr == Py_None) v.emplace_back();
        v.emplace_back(PyHandler{std::move(o)});
        return true;
    });
}

/******************************************************************************/

Value run_test(double &time, TestCase const &test, bool no_gil,
               Vector<Counter> &counts, Vector<Handler> handlers, ArgPack pack) {
    no_gil = no_gil && !test.function.target<PyTestCase>();
    ReleaseGIL lk(no_gil);
    if (no_gil) for (auto &c : handlers)
        if (c) c.target<PyHandler>()->unlock = &lk;

    for (auto &c : counts) c.store(0u);

    Context ctx({test.name}, std::move(handlers), &counts, &lk);
    Timer t(time);

    if (!test.function) throw std::runtime_error("Test case has empty std::function");
    try {return test.function(ctx, std::move(pack));}
    catch (ClientError const &e) {throw e;}
    catch (...) {return {};} // Silence any other exceptions from inside the test
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

    Vector<cpy::Handler> handlers;
    if (!cpy::build_handlers(handlers, std::move(calls))) return {};

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
    } else if (!put_argpack(pack, std::move(pypack))) return {};

    std::stringstream out, err;

    cpy::Value v;
    double test_time = 0;
    Vector<cpy::Counter> counters(handlers.size());

    {
        cpy::RedirectStream o(cpy::cout_sync, cout ? out.rdbuf() : nullptr);
        cpy::RedirectStream e(cpy::cerr_sync, cerr ? err.rdbuf() : nullptr);
        v = cpy::run_test(test_time, *test, no_gil, counters, std::move(handlers), std::move(pack));
    }

    auto value = cpy::to_python(v);
    if (!value) return {};
    auto timed = cpy::to_python(test_time);
    if (!timed) return {};
    auto counts = cpy::to_tuple(counters, [](auto const &c) {return c.load(std::memory_order_relaxed);});
    if (!counts) return {};
    auto pyout = cpy::to_python(out.str());
    if (!pyout) return {};
    auto pyerr = cpy::to_python(err.str());
    if (!pyerr) return {};
    return {PyTuple_Pack(5u, +value, +timed, +counts, +pyout, +pyerr), false};
}

/******************************************************************************/

}

// the goal would be for all of these functions below to be wrapped.
// even if they have calls to python api it would still be nice to type erase the API

extern "C" {

/******************************************************************************/

// can be wrapped
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

// can be wrapped
PyObject *cpy_n_tests(PyObject *, PyObject *args) {
    Py_ssize_t n = cpy::suite().size();
    return Py_BuildValue("n", n);
}

/******************************************************************************/

// can be wrapped
PyObject *cpy_add_test(PyObject *, PyObject *args) {
    char const *s;
    PyObject *fun, *pypacks = nullptr;
    if (!PyArg_ParseTuple(args, "sO|O", &s, &fun, &pypacks)) return nullptr;

    return cpy::return_object([=] {
        cpy::Vector<cpy::ArgPack> packs;
        if (pypacks) {
            cpy::map_iterable({pypacks, true}, [](cpy::Object o) {
                packs.emplace_back();
                return cpy::put_argpack(packs.back(), std::move(o));
            });
        }
        cpy::add_test(cpy::TestCase{s, {}, cpy::PyTestCase(fun, true), std::move(packs)});
        return cpy::Object(Py_None, true);
    });
}

// can be wrapped
PyObject *cpy_add_value(PyObject *, PyObject *args) {
    char const *s;
    PyObject *obj;
    if (!PyArg_ParseTuple(args, "sO", &s, &obj)) return nullptr;

    return cpy::return_object([=] {
        cpy::Value val;
        if (!cpy::from_python(val, cpy::Object(obj, true))) return cpy::Object();
        cpy::add_test(cpy::TestCase{s, {}, cpy::ValueAdaptor{std::move(val)}});
        return cpy::Object(Py_None, true);
    });
}

/******************************************************************************/

// can be wrapped
PyObject *cpy_compile_info(PyObject *, PyObject *) {
    auto v = cpy::to_python(__VERSION__ "");
    auto d = cpy::to_python(__DATE__ "");
    auto t = cpy::to_python(__TIME__ "");
    return (v && d && t) ? PyTuple_Pack(3u, +v, +d, +t) : nullptr;
}

/******************************************************************************/

// can be wrapped
PyObject *cpy_test_names(PyObject *, PyObject *) {
    return cpy::return_object([] {
        return cpy::to_tuple(cpy::suite(),
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
    auto l = cpy::to_python(static_cast<cpy::Integer>(c->comment.location.line));
    if (!l) return nullptr;
    auto o = cpy::to_python(c->comment.comment);
    if (!o) return nullptr;
    return PyTuple_Pack(4u, +n, +f, +l, +o);
}

/******************************************************************************/

}

