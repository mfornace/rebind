#pragma once
#include <cpy/PythonBindings.h>
#include <cpy/Test.h>
#include <mutex>

namespace cpy {

struct Identity {
    template <class T>
    T const & operator()(T const &t) const {return t;}
};

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

/******************************************************************************/

inline Object to_python(std::monostate const &) noexcept {
    return {Py_None, true};
}

inline Object to_python(bool b) noexcept {
    return {b ? Py_True : Py_False, true};
}

inline Object to_python(char const *s) noexcept {
    return {PyUnicode_FromString(s ? s : ""), false};
}

template <class T, std::enable_if_t<(std::is_integral_v<T>), int> = 0>
inline Object to_python(T t) noexcept {
    return {PyLong_FromLongLong(static_cast<long long>(t)), false};
}

inline Object to_python(double t) noexcept {
    return {PyFloat_FromDouble(t), false};
}

inline Object to_python(std::string const &s) noexcept {
    return {PyUnicode_FromStringAndSize(s.data(), static_cast<Py_ssize_t>(s.size())), false};
}

inline Object to_python(std::string_view const &s) noexcept {
    return {PyUnicode_FromStringAndSize(s.data(), static_cast<Py_ssize_t>(s.size())), false};
}

inline Object to_python(std::wstring const &s) noexcept {
    return {PyUnicode_FromWideChar(s.data(), static_cast<Py_ssize_t>(s.size())), false};
}

inline Object to_python(std::wstring_view const &s) noexcept {
    return {PyUnicode_FromWideChar(s.data(), static_cast<Py_ssize_t>(s.size())), false};
}

inline Object to_python(std::complex<double> const &s) noexcept {
    return {PyComplex_FromDoubles(s.real, s.imag), false};
}

inline Object to_python(Value const &s) noexcept {
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

template <class T, class F=Identity>
Object to_python(Vector<T> const &v, F const &f={}) noexcept {
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

struct PythonError : CallbackError {
    PythonError(char const *s) : CallbackError(s) {}
};

PythonError python_error() noexcept;

/******************************************************************************/

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

struct PyCallback {
    Object object;
    ReleaseGIL *unlock = nullptr;

    bool operator()(Event event, Scopes const &scopes, Logs &&logs) {
        if (!+object) return false;
        AcquireGIL lk(unlock); // reacquire the GIL (if it was released)

        Object pyevent = to_python(static_cast<Integer>(event));
        if (!pyevent) return false;

        Object pyscopes = to_python(scopes);
        if (!pyscopes) return false;

        Object pylogs = to_python(logs);
        if (!pylogs) return false;

        Object out = {PyObject_CallFunctionObjArgs(+object, +pyevent, +pyscopes, +pylogs, nullptr), false};
        if (PyErr_Occurred()) throw python_error();
        return bool(out);
    }
};

/// std::ostream synchronizer for redirection from multiple threads
struct StreamSync {
    std::ostream &stream;
    std::streambuf *original; // never changed (unless by user)
    std::mutex mutex;
    Vector<std::streambuf *> queue;
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

/******************************************************************************/

bool from_python(Value &v, Object o);

struct PyTestCase : Object {
    using Object::Object;

    Value operator()(Context ctx, ArgPack const &pack) {
        AcquireGIL lk(static_cast<ReleaseGIL *>(ctx.metadata));
        Object args = cpy::to_python(pack);
        if (!args) throw python_error();
        Object o = {PyObject_CallObject(Object::ptr, +args), false};
        if (!o) throw python_error();
        Value out;
        if (!cpy::from_python(out, std::move(o))) throw python_error();
        return out;
    }
};

/******************************************************************************/

}
