#include "Test.h"
#include <Python.h>
#include <chrono>
#include <iostream>
#include <vector>

namespace cpy {

/******************************************************************************/

Value::Value(Value &&v) noexcept : var(std::move(v.var)) {}
Value::Value(Value const &v) noexcept : var(v.var) {}
Value & Value::operator=(Value const &v) noexcept {var = v.var; return *this;}
Value & Value::operator=(Value &&v) noexcept {var = std::move(v.var); return *this;}

Value::Value(std::monostate v) : var(v) {}
Value::Value(bool v) : var(v) {}
Value::Value(std::size_t v) : var(v) {}
Value::Value(std::ptrdiff_t v) : var(v) {}
Value::Value(double v) : var(v) {}
Value::Value(std::complex<double> v) : var(v) {}
Value::Value(std::string v) : var(std::move(v)) {}
Value::Value(std::string_view v) : var(std::move(v)) {}

// Value::Value(std::vector<bool> v) : var(std::move(v)) {}
// Value::Value(std::vector<std::size_t> v) : var(std::move(v)) {}
// Value::Value(std::vector<std::ptrdiff_t> v) : var(std::move(v)) {}
// Value::Value(std::vector<double> v) : var(std::move(v)) {}
// Value::Value(std::vector<std::complex<double>> v) : var(std::move(v)) {}
// Value::Value(std::vector<std::string> v) : var(std::move(v)) {}
// Value::Value(std::vector<std::string_view> v) : var(std::move(v)) {}

Value::~Value() = default;

/******************************************************************************/

Suite & suite() {
    static Suite static_suite;
    return static_suite;
}

double current_time() noexcept {
    auto t = std::chrono::high_resolution_clock::now().time_since_epoch();
    return std::chrono::duration<double>{t}.count();
}

/******************************************************************************/

struct Object {
    PyObject *ptr = nullptr;
    Object() = default;
    Object(PyObject *o, bool incref) : ptr(o) {if (incref) Py_INCREF(ptr);}
    Object(Object const &o) : ptr(o.ptr) {Py_XINCREF(ptr);}
    Object(Object &&o) noexcept : ptr(std::exchange(o.ptr, nullptr)) {}
    explicit operator bool() const {return ptr;}
    PyObject * operator+() const {return ptr;}
    ~Object() {Py_XDECREF(ptr);}
};

/// RAII release of Python GIL
struct ReleaseGIL {
    PyThreadState * state;
    std::mutex mutex;
    ReleaseGIL(bool no_gil) : state(no_gil ? PyEval_SaveThread() : nullptr) {}
    void acquire() noexcept {if (state) {mutex.lock(); PyEval_RestoreThread(state);}}
    void release() noexcept {if (state) {state = PyEval_SaveThread(); mutex.unlock();}}
    ~ReleaseGIL() {if (state) PyEval_RestoreThread(state);}
};

/// RAII reacquisition of Python GIL
struct AcquireGIL {
    ReleaseGIL * const lock; //< ReleaseGIL object; can be nullptr
    AcquireGIL(ReleaseGIL *u) : lock(u) {if (lock) lock->acquire();}
    ~AcquireGIL() {if (lock) lock->release();}
};

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
        char const *c = PyUnicode_AsUTF8AndSize(+o, &size);
        if (c) v = std::string_view(c, size);
        else return false;
    } else {
        PyErr_SetString(PyExc_TypeError, "Invalid type for conversion to C++");
    }
    return !PyErr_Occurred();
};

/******************************************************************************/

Object to_python(std::monostate const &) noexcept {
    return {Py_None, true};
}

Object to_python(bool b) noexcept {
    return {b ? Py_True : Py_False, true};
}

Object to_python(char const *s) noexcept {
    return {PyUnicode_FromString(s ? s : ""), false};
}

Object to_python(std::size_t t) noexcept {
    return {PyLong_FromSsize_t(static_cast<Py_ssize_t>(t)), false};
}

Object to_python(std::ptrdiff_t t) noexcept {
    return {PyLong_FromLongLong(static_cast<long long>(t)), false};
}

Object to_python(double t) noexcept {
    return {PyFloat_FromDouble(t), false};
}

Object to_python(std::string const &s) noexcept {
    return {PyUnicode_FromStringAndSize(s.data(), static_cast<Py_ssize_t>(s.size())), false};
}

Object to_python(std::string_view const &s) noexcept {
    return {PyUnicode_FromStringAndSize(s.data(), static_cast<Py_ssize_t>(s.size())), false};
}

Object to_python(std::wstring const &s) noexcept {
    return {PyUnicode_FromWideChar(s.data(), static_cast<Py_ssize_t>(s.size())), false};
}

Object to_python(std::wstring_view const &s) noexcept {
    return {PyUnicode_FromWideChar(s.data(), static_cast<Py_ssize_t>(s.size())), false};
}

Object to_python(Value const &s) noexcept {
    return std::visit([](auto const &x) {return to_python(x);}, s.var);
}

/******************************************************************************/

Object to_python(KeyPair const &p) noexcept {
    Object key = to_python(p.key);
    if (!key) return key;
    Object value = to_python(p.value);
    if (!value) return value;
    return {PyTuple_Pack(2u, +key, +value), false};
}

struct Identity {
    template <class T>
    T const & operator()(T const &t) const {return t;}
};

template <class T, class F=Identity>
Object to_python(std::vector<T> const &v, F const &f={}) noexcept {
    Object out = {PyTuple_New(v.size()), false};
    if (!out) return out;
    for (Py_ssize_t i = 0u; i != v.size(); ++i) {
        Object item = to_python(f(v[i]));
        if (!item) return item;
        Py_INCREF(+item);
        if (PyTuple_SetItem(+out, i, +item)) {
            Py_DECREF(+item);
            return Object();
        }
    }
    return out;
}

/******************************************************************************/

struct PyCallback {
    Object object;
    ReleaseGIL *unlock = nullptr;

    bool operator()(Event event, Scopes const &scopes, Logs &&logs) noexcept {
        if (!+object) return false;
        AcquireGIL lk(unlock); // reacquire the GIL (if it was released)

        Object pyevent = to_python(static_cast<std::size_t>(event));
        if (!pyevent) return false;

        Object pyscopes = to_python(scopes);
        if (!pyscopes) return false;

        Object pylogs = to_python(logs);
        if (!pylogs) return false;

        Object out = {PyObject_CallFunctionObjArgs(+object, +pyevent, +pyscopes, +pylogs, nullptr), false};
        return bool(out);
    }
};

/******************************************************************************/

template <class V, class F>
bool build_vector(V &v, Object iterable, F &&f) {
    Object iter = {PyObject_GetIter(+iterable), false};
    if (!iter) return false;

    bool ok = true;
    while (ok) {
        auto it = Object(PyIter_Next(+iter), false);
        if (!+it) break;
        v.emplace_back(f(std::move(it), ok));
    }
    return ok;
}

/******************************************************************************/

/// std::ostream synchronizer for redirection from multiple threads
struct StreamSync {
    std::ostream &stream;
    std::streambuf *original;
    std::mutex mutex;
    std::vector<std::streambuf *> queue;
};

/// RAII acquisition of cout or cerr
struct RedirectStream {
    StreamSync &sync;
    std::streambuf * const buf;

    RedirectStream(StreamSync &s, std::streambuf *b) : sync(s), buf(b) {
        if (!buf) return;
        std::lock_guard<std::mutex> lk(sync.mutex);
        if (sync.queue.empty()) sync.stream.rdbuf(buf); // take over the stream
        else sync.queue.push_back(buf); // or add to queue
    }

    ~RedirectStream() {
        if (!buf) return;
        std::lock_guard<std::mutex> lk(sync.mutex);
        auto it = std::find(sync.queue.begin(), sync.queue.end(), buf);
        if (it != sync.queue.end()) sync.queue.erase(it); // remove from queue
        else if (sync.queue.empty()) sync.stream.rdbuf(sync.original); // set to original
        else { // let next waiting stream take over
            sync.stream.rdbuf(sync.queue[0]);
            sync.queue.erase(sync.queue.begin());
        }
    }
};

StreamSync cout_sync{std::cout, std::cout.rdbuf()};
StreamSync cerr_sync{std::cerr, std::cerr.rdbuf()};

/******************************************************************************/

struct PyTestCase : Object {
    using Object::Object;

    bool operator()(Value &out, Context ctx, ArgPack const &pack) noexcept {
        Object args = cpy::to_python(pack);
        if (!args) return false;
        Object o = {PyObject_CallObject(Object::ptr, +args), false};
        if (!o) return false;
        return cpy::from_python(out, std::move(o));
    }
};

/******************************************************************************/

bool build_argpack(ArgPack &pack, Object pypack) {
    return cpy::build_vector(pack, pypack, [](cpy::Object &&o, bool &ok) {
            cpy::Value v;
            ok &= cpy::from_python(v, std::move(o));
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

bool run_test(Value &v, TestCase const &test, bool no_gil, std::vector<Counter> &counts,
              std::vector<Callback> callbacks, ArgPack pack) {
    no_gil = no_gil && !test.function.target<PyTestCase>();
    ReleaseGIL lk(no_gil);
    if (no_gil) for (auto &c : callbacks)
        if (c) c.target<PyCallback>()->unlock = &lk;

    for (auto &c : counts) c.store(0u);

    Context ctx({test.name}, std::move(callbacks), &counts);
    return test.function(v, std::move(ctx), std::move(pack));
}

/******************************************************************************/

TestCase *find_test(Py_ssize_t i) {
    if (i >= cpy::suite().cases.size()) {
        PyErr_SetString(PyExc_IndexError, "Unit test index out of range");
        return nullptr;
    }
    return &cpy::suite().cases[i];
}

Object run_test(Py_ssize_t i, Object calls, Object pypack, bool cout, bool cerr, bool no_gil) {
    auto const test = cpy::find_test(i);
    if (!test) return {};

    std::vector<cpy::Callback> callbacks;
    if (!cpy::build_callbacks(callbacks, std::move(calls))) return {};

    cpy::ArgPack pack;
    if (PyLong_Check(+pypack)) {
        auto n = PyLong_AsSize_t(+pypack);
        if (PyErr_Occurred()) return {};
        if (n >= test->parameters.size()) {
            PyErr_SetString(PyExc_IndexError, "Parameter pack index out of range");
            return {};
        }
        pack = test->parameters[n];
    } else if (!build_argpack(pack, std::move(pypack))) return {};

    std::stringstream out, err;

    cpy::Value v;
    std::vector<cpy::Counter> counters(callbacks.size());

    {
        cpy::RedirectStream o(cpy::cout_sync, cout ? out.rdbuf() : nullptr);
        cpy::RedirectStream e(cpy::cerr_sync, cerr ? err.rdbuf() : nullptr);
        if (!cpy::run_test(v, *test, no_gil, counters, std::move(callbacks), std::move(pack))) {
            if (!PyErr_Occurred())
                PyErr_SetString(PyExc_TypeError, "Invalid unit test argument types");
            return {};
        }
    }

    auto value = cpy::to_python(v);
    if (!value) return {};
    auto counts = cpy::to_python(counters, [](auto const &c) {return c.load();});
    if (!counts) return {};
    auto pyout = cpy::to_python(out.str());
    if (!pyout) return {};
    auto pyerr = cpy::to_python(err.str());
    if (!pyerr) return {};
    return {PyTuple_Pack(4u, +value, +counts, +pyout, +pyerr), false};
}

/******************************************************************************/

template <class F>
PyObject * return_object(F &&f) noexcept {
    try {
        Object o = static_cast<F &&>(f)();
        Py_XINCREF(+o);
        return +o;
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

static PyObject *cpy_run_test(PyObject *self, PyObject *args) {
    Py_ssize_t i;
    PyObject *calls, *pack, *cout, *cerr, *gil;
    if (!PyArg_ParseTuple(args, "nOOOOO", &i, &calls, &pack, &gil, &cout, &cerr))
        return nullptr;
    return cpy::return_object([&] {
        return cpy::run_test(i, {calls, true}, {pack, true},
            PyObject_IsTrue(+cout), PyObject_IsTrue(+cerr), PyObject_Not(+gil));
    });
}

/******************************************************************************/

static PyObject *cpy_n_tests(PyObject *, PyObject *args) {
    Py_ssize_t n = cpy::suite().cases.size();
    return Py_BuildValue("n", n);
}

/******************************************************************************/

static PyObject *cpy_add_test(PyObject *, PyObject *args) {
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
        cpy::suite().cases.emplace_back(cpy::TestCase{s, {}, cpy::PyTestCase(fun, true), std::move(packs)});
        return cpy::Object(Py_None, true);
    });
}

/******************************************************************************/

static PyObject *cpy_compile_info(PyObject *, PyObject *) {
    auto v = cpy::to_python(__VERSION__ "");
    auto d = cpy::to_python(__DATE__ "");
    auto t = cpy::to_python(__TIME__ "");
    return (v && d && t) ? PyTuple_Pack(3u, +v, +d, +t) : nullptr;
}

/******************************************************************************/

static PyObject *cpy_test_names(PyObject *, PyObject *) {
    return cpy::return_object([] {
        return cpy::to_python(cpy::suite().cases,
            [](auto const &c) -> decltype(c.name) {return c.name;}
        );
    });
}

/******************************************************************************/

static PyObject *cpy_find_test(PyObject *self, PyObject *args) {
    char const *s;
    if (!PyArg_ParseTuple(args, "s", &s)) return nullptr;
    return cpy::return_object([s] {
        std::string_view name{s};
        auto const &cases = cpy::suite().cases;
        for (std::size_t i = 0; i != cases.size(); ++i)
            if (cases[i].name == name) return cpy::to_python(i);
        PyErr_SetString(PyExc_KeyError, "Test name not found");
        return cpy::Object();
    });
}

/******************************************************************************/

static PyObject *cpy_test_info(PyObject *self, PyObject *args) {
    Py_ssize_t i;
    if (!PyArg_ParseTuple(args, "n", &i)) return nullptr;
    auto c = cpy::find_test(i);
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

static PyMethodDef cpy_methods[] = {
    {"run_test",     (PyCFunction) cpy_run_test,     METH_VARARGS,
        "Run a unit test. Positional arguments:\n"
        "i (int):             test index\n"
        "callbacks (tuple):   list of callbacks for each event\n"
        "args (tuple or int): arguments to apply, or index of the already-registered argument pack\n"
        "gil (bool):          whether to keep the Python global interpreter lock on\n"
        "cout (bool):         whether to redirect std::cout\n"
        "cerr (bool):         whether to redirect std::cerr\n"},
    {"find_test",    (PyCFunction) cpy_find_test,    METH_VARARGS,
        "Find the index of a unit test from its registered name (str)"},
    {"n_tests",      (PyCFunction) cpy_n_tests,      METH_NOARGS,  "Number of registered tests (no arguments)"},
    {"compile_info", (PyCFunction) cpy_compile_info, METH_NOARGS,  "Compilation information (no arguments)"},
    {"test_names",   (PyCFunction) cpy_test_names,   METH_NOARGS,  "Names of registered tests (no arguments)"},
    {"test_info",    (PyCFunction) cpy_test_info,    METH_VARARGS, "Info of a registered test from its index (int)"},
    {"add_test",     (PyCFunction) cpy_add_test,     METH_VARARGS, "Add a unit test from a python object"},
    {nullptr, nullptr, 0, nullptr}};

/******************************************************************************/

static struct PyModuleDef cpy_definition = {
    PyModuleDef_HEAD_INIT,
    "libcpy",
    "A Python module to run C++ unit tests",
    -1,
    cpy_methods
};

/******************************************************************************/

PyMODINIT_FUNC PyInit_libcpy(void) {
    Py_Initialize();
    return PyModule_Create(&cpy_definition);
}

/******************************************************************************/


}
