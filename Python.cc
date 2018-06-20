#include "Test.h"
#include <Python.h>
#include <chrono>
#include <iostream>

namespace cpy {

/******************************************************************************/

Value & Value::operator=(Value const &v) noexcept {var = v.var; return *this;}
Value & Value::operator=(Value &&v) noexcept {var = std::move(v.var); return *this;}
Value::Value(Value &&v) noexcept : var(std::move(v.var)) {}
Value::Value(Value const &v) noexcept : var(v.var) {}
Value::Value(std::monostate v) : var(v) {}
Value::Value(bool v) : var(v) {}
Value::Value(std::size_t v) : var(v) {}
Value::Value(std::ptrdiff_t v) : var(v) {}
Value::Value(double v) : var(v) {}
Value::Value(std::complex<double> v) : var(v) {}
Value::Value(std::string v) : var(std::move(v)) {}
Value::Value(std::string_view v) : var(std::move(v)) {}
Value::~Value() {}

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
    explicit Object(PyObject *o, bool incref) : ptr(o) {if (incref) Py_INCREF(ptr);}
    Object(Object const &o) : ptr(o.ptr) {Py_XINCREF(ptr);}
    Object(Object &&o) noexcept : ptr(std::exchange(o.ptr, nullptr)) {}
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

bool from_python(Value &v, PyObject *o) {
    if (o == Py_None) {
        v = std::monostate();
    } else if (PyBool_Check(o)) {
        v = (o == Py_True) ? true : false;
    } else if (PyLong_Check(o)) {
        long long i = PyLong_AsLongLong(o);
        if (i < 0) v = static_cast<std::ptrdiff_t>(i);
        else v = static_cast<std::size_t>(i);
    } else if (PyFloat_Check(o)) {
        double d = PyFloat_AsDouble(o);
        v = d;
    } else if (PyComplex_Check(o)) {
        v = std::complex<double>{PyComplex_RealAsDouble(o), PyComplex_ImagAsDouble(o)};
    } else if (PyBytes_Check(o)) {
        char *c;
        Py_ssize_t size;
        PyBytes_AsStringAndSize(o, &c, &size);
        v = std::string_view(c, size);
    } else if (PyUnicode_Check(o)) { // no use of wstring for now.
        Py_ssize_t size;
        char const *c = PyUnicode_AsUTF8AndSize(o, &size);
        if (c) v = std::string_view(c, size);
        else return false;
    } else {
        PyErr_SetString(PyExc_TypeError, "Invalid type for conversion to C++");
    }
    return !PyErr_Occurred();
};

/******************************************************************************/

PyObject *to_python(std::monostate const &) noexcept {
    Py_RETURN_NONE;
}

PyObject *to_python(bool b) noexcept {
    if (b) Py_RETURN_TRUE;
    else Py_RETURN_FALSE;
}

PyObject *to_python(char const *s) noexcept {
    return PyUnicode_FromString(s ? s : "");
}


PyObject *to_python(std::size_t t) noexcept {
    return PyLong_FromSsize_t(static_cast<Py_ssize_t>(t));
}

PyObject *to_python(std::ptrdiff_t t) noexcept {
    return PyLong_FromLongLong(static_cast<long long>(t));
}

PyObject *to_python(double t) noexcept {
    return PyFloat_FromDouble(t);
}

PyObject *to_python(std::string const &s) noexcept {
    return PyUnicode_FromStringAndSize(s.data(), static_cast<Py_ssize_t>(s.size()));
}

PyObject *to_python(std::string_view const &s) noexcept {
    return PyUnicode_FromStringAndSize(s.data(), static_cast<Py_ssize_t>(s.size()));
}

PyObject *to_python(std::wstring const &s) noexcept {
    return PyUnicode_FromWideChar(s.data(), static_cast<Py_ssize_t>(s.size()));
}

PyObject *to_python(std::wstring_view const &s) noexcept {
    return PyUnicode_FromWideChar(s.data(), static_cast<Py_ssize_t>(s.size()));
}

PyObject *to_python(Value const &s) noexcept {
    return std::visit([](auto const &x) {return to_python(x);}, s.var);
}

/******************************************************************************/

PyObject *to_python(KeyPair const &p) noexcept {
    PyObject *key = to_python(p.key);
    if (!key) return nullptr;
    PyObject *value = to_python(p.value);
    if (!value) return nullptr;
    return PyTuple_Pack(2u, key, value);
}

struct Identity {
    template <class T>
    T const & operator()(T const &t) const {return t;}
};

template <class T, class F=Identity>
PyObject *to_python(std::vector<T> const &v, F const &f={}) noexcept {
    PyObject *out = PyTuple_New(v.size());
    if (!out) return nullptr;
    for (Py_ssize_t i = 0u; i != v.size(); ++i) {
        PyObject *item = to_python(f(v[i]));
        if (!item || PyTuple_SetItem(out, i, item)) return nullptr;
    }
    return out;
}

/******************************************************************************/

struct PyCallback {
    Object object;
    ReleaseGIL *unlock = nullptr;

    bool operator()(Event event, Scopes const &scopes, Logs &&logs) noexcept {
        if (!object.ptr) return false;
        AcquireGIL lk(unlock); // reacquire the GIL (if it was released)

        PyObject *pyevent = to_python(static_cast<std::size_t>(event));
        if (!pyevent) return false;

        PyObject *pyscopes = to_python(scopes);
        if (!pyscopes) return false;

        PyObject *pylogs = to_python(logs);
        if (!pylogs) return false;

        PyObject *args = PyTuple_Pack(3u, pyevent, pyscopes, pylogs);
        if (!args) return false;

        PyObject *out = PyObject_CallObject(object.ptr, args);
        return bool(out);
    }
};

/******************************************************************************/

template <class V, class F>
bool build_vector(V &v, PyObject *iterable, F &&f) {
    PyObject *iter = PyObject_GetIter(iterable);
    if (!iter) return false;

    bool ok = true;
    while (ok) {
        Object it{PyIter_Next(iter), false};
        if (!it.ptr) break;
        v.emplace_back(f(std::move(it), ok));
    }
    return ok;
}

/******************************************************************************/

/// RAII acquisition of cout or cerr given a mutex
struct RedirectStream {
    std::ostream &os;
    std::mutex &mut;
    std::streambuf *buf;

    RedirectStream(std::streambuf *b, std::ostream &o, std::mutex &m) : os(o), mut(m), buf(b) {
        if (buf) {
            std::lock_guard<std::mutex> lk(mut);
            buf = os.rdbuf(b);
        }
    }

    ~RedirectStream() {
        if (buf) {
            std::lock_guard<std::mutex> lk(mut);
            os.rdbuf(buf);
        }
    }
};

std::mutex cout_mutex, cerr_mutex;

/******************************************************************************/

struct PyTestCase : Object {
    using Object::Object;

    bool operator()(Value &out, Context ctx, ArgPack const &pack) noexcept {
        PyObject *args = cpy::to_python(pack);
        if (!args) return false;
        PyObject *o = PyObject_CallObject(Object::ptr, args);
        if (!o) return false;
        return cpy::from_python(out, o);
    }
};

/******************************************************************************/

bool build_argpack(ArgPack &pack, PyObject *pypack) {
    return cpy::build_vector(pack, pypack, [](cpy::Object &&o, bool &ok) {
            cpy::Value v;
            ok &= cpy::from_python(v, o.ptr);
            return v;
    });
}

/******************************************************************************/

bool build_callbacks(std::vector<Callback> &v, PyObject *pycalls) {
    return build_vector(v, pycalls, [](Object &&o, bool) -> Callback {
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

/******************************************************************************/

}

extern "C" {

/******************************************************************************/

static PyObject *cpy_run_test(PyObject *self, PyObject *args) {
    Py_ssize_t i;
    PyObject *pycalls, *pypack, *cout, *cerr, *gil;
    if (!PyArg_ParseTuple(args, "nOOOOO", &i, &pycalls, &pypack, &gil, &cout, &cerr))
        return nullptr;


    auto const test = cpy::find_test(i);
    if (!test) return nullptr;

    std::vector<cpy::Callback> callbacks;
    if (!cpy::build_callbacks(callbacks, pycalls)) return nullptr;

    cpy::ArgPack pack;
    if (PyLong_Check(pypack)) {
        auto n = PyLong_AsSize_t(pypack);
        if (PyErr_Occurred()) return nullptr;
        if (n >= test->parameters.size()) {
            PyErr_SetString(PyExc_IndexError, "Parameter pack index out of range");
            return nullptr;
        }
        pack = test->parameters[n];
    } else if (!build_argpack(pack, pypack)) return nullptr;

    bool const no_gil = PyObject_Not(gil);
    std::stringstream out, err;

    cpy::Value v;
    std::vector<cpy::Counter> counters(callbacks.size());

    {
        cpy::RedirectStream o(PyObject_IsTrue(cout) ? out.rdbuf() : nullptr, std::cout, cpy::cout_mutex);
        cpy::RedirectStream e(PyObject_IsTrue(cerr) ? err.rdbuf() : nullptr, std::cerr, cpy::cerr_mutex);
        if (!cpy::run_test(v, *test, no_gil, counters, std::move(callbacks), std::move(pack))) {
            PyErr_SetString(PyExc_TypeError, "Invalid unit test argument types");
            return nullptr;
        }
    }

    PyObject *value = cpy::to_python(v);
    if (!value) return nullptr;
    PyObject *counts = cpy::to_python(counters, [](auto const &c) {return c.load();});
    if (!counts) return nullptr;
    PyObject *pyout = cpy::to_python(out.str());
    if (!pyout) return nullptr;
    PyObject *pyerr = cpy::to_python(err.str());
    if (!pyerr) return nullptr;
    return PyTuple_Pack(4u, value, counts, pyout, pyerr);
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

    std::vector<cpy::ArgPack> packs;
    if (pypacks) {
        cpy::build_vector(packs, pypacks, [](cpy::Object &&o, bool &ok) {
            cpy::ArgPack pack;
            ok &= cpy::build_argpack(pack, o.ptr);
            return pack;
        });
    }

    cpy::suite().cases.emplace_back(cpy::TestCase{s, {}, cpy::PyTestCase(fun, true), std::move(packs)});
    Py_RETURN_NONE;
}

/******************************************************************************/

static PyObject *cpy_compile_info(PyObject *, PyObject *) {
    return PyTuple_Pack(3u,
        cpy::to_python(__VERSION__ ""),
        cpy::to_python(__DATE__ ""),
        cpy::to_python(__TIME__ "")
    );
}

/******************************************************************************/

static PyObject *cpy_test_names(PyObject *, PyObject *) {
    return cpy::to_python(cpy::suite().cases,
        [](auto const &c) -> decltype(c.name) {return c.name;}
    );
}

/******************************************************************************/

static PyObject *cpy_find_test(PyObject *self, PyObject *args) {
    char const *s;
    if (!PyArg_ParseTuple(args, "s", &s)) return nullptr;
    std::string_view name{s};
    auto const &cases = cpy::suite().cases;
    for (std::size_t i = 0; i != cases.size(); ++i)
        if (cases[i].name == name) return cpy::to_python(i);
    PyErr_SetString(PyExc_KeyError, "Test name not found");
    return nullptr;
}

/******************************************************************************/

static PyObject *cpy_test_info(PyObject *self, PyObject *args) {
    Py_ssize_t i;
    if (!PyArg_ParseTuple(args, "n", &i)) return nullptr;
    auto c = cpy::find_test(i);
    if (!c) return nullptr;
    PyObject *t0 = cpy::to_python(c->name);
    if (!t0) return nullptr;
    PyObject *t1 = cpy::to_python(c->comment.location.file);
    if (!t1) return nullptr;
    PyObject *t2 = cpy::to_python(static_cast<std::size_t>(c->comment.location.line));
    if (!t2) return nullptr;
    PyObject *t3 = cpy::to_python(c->comment.comment);
    if (!t3) return nullptr;
    return PyTuple_Pack(4u, t0, t1, t2, t3);
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
