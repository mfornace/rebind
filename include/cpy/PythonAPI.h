/**
 * @brief Python-related C++ API for cpy
 * @file PythonAPI.h
 */

#pragma once
#include "Document.h"

#include <mutex>
#include <complex>
#include <iostream>
#include <variant>
#include <unordered_map>
#include <map>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wregister"
#include <Python.h>
#pragma GCC diagnostic pop

namespace cpy {

extern PyObject *TypeErrorObject;

extern std::unordered_map<TypeIndex, std::string> type_names;

enum class Scalar {Bool, Char, SignedChar, UnsignedChar, Unsigned, Signed, Float, Pointer};
extern Zip<Scalar, TypeIndex, unsigned> scalars;

inline std::size_t reference_count(PyObject *o) {return o ? Py_REFCNT(o) : 0u;}

inline void incref(PyObject *o) noexcept {Py_INCREF(o);}
inline void decref(PyObject *o) noexcept {Py_DECREF(o);}
inline void xincref(PyObject *o) noexcept {Py_XINCREF(o);}
inline void xdecref(PyObject *o) noexcept {Py_XDECREF(o);}

/******************************************************************************/

struct TypeObject {
    PyTypeObject *ptr;
    operator PyObject *() const {return reinterpret_cast<PyObject *>(ptr);}
    operator PyTypeObject *() const {return ptr;}
};

template <class T>
struct Holder {
    static PyTypeObject type;
    PyObject_HEAD // 16 bytes for the ref count and the type object
    T value; // I think stack is OK because this object is only casted to anyway.
};

template <class T>
PyTypeObject Holder<T>::type;

template <class T>
TypeObject type_object(Type<T> t={}) {return {&Holder<T>::type};}

/******************************************************************************/

struct PythonError : ClientError {
    PythonError(char const *s) : ClientError(s) {}
};

PythonError python_error(std::nullptr_t=nullptr) noexcept;

template <class ...Ts>
std::nullptr_t type_error(char const *s, Ts ...ts) {PyErr_Format(TypeErrorObject, s, ts...); return nullptr;}

/******************************************************************************/

struct Object {
    PyObject *ptr = nullptr;
    Object() = default;
    Object(std::nullptr_t) {}
    static Object from(PyObject *o) {return o ? Object(o, false) : throw python_error();}
    Object(PyObject *o, bool increment) : ptr(o) {if (increment) xincref(ptr);}

    Object(Object const &o) noexcept : ptr(o.ptr) {xincref(ptr);}
    Object & operator=(Object const &o) noexcept {ptr = o.ptr; xincref(ptr); return *this;}

    Object(Object &&o) noexcept : ptr(std::exchange(o.ptr, nullptr)) {}
    Object & operator=(Object &&o) noexcept {ptr = std::exchange(o.ptr, nullptr); return *this;}

    explicit operator bool() const {return ptr;}
    operator PyObject *() const {return ptr;}
    PyObject *operator+() const {return ptr;}

    bool operator<(Object const &o) const {return ptr < o.ptr;}
    bool operator>(Object const &o) const {return ptr > o.ptr;}
    bool operator==(Object const &o) const {return ptr == o.ptr;}
    bool operator!=(Object const &o) const {return ptr != o.ptr;}
    bool operator<=(Object const &o) const {return ptr <= o.ptr;}
    bool operator>=(Object const &o) const {return ptr >= o.ptr;}
    friend void swap(Object &o, Object &p) {std::swap(o.ptr, p.ptr);}

    ~Object() {xdecref(ptr);}
};

extern std::map<Object, Object> output_conversions;
extern std::map<Object, Object> input_conversions;
extern std::unordered_map<std::type_index, Object> python_types;

struct Var : Variable {
    using Variable::Variable;
    Object ward = {};
};

template <>
struct Holder<Variable> : Holder<Var> {};

/******************************************************************************/

class Buffer {
    static Zip<std::string_view, std::type_info const *> formats;
    bool valid;

public:
    Py_buffer view;

    Buffer(Buffer const &) = delete;
    Buffer(Buffer &&b) noexcept : view(b.view), valid(std::exchange(b.valid, false)) {}

    Buffer & operator=(Buffer const &) = delete;
    Buffer & operator=(Buffer &&b) noexcept {view = b.view; valid = std::exchange(b.valid, false); return *this;}

    explicit operator bool() const {return valid;}

    Buffer(PyObject *o, int flags) {
        DUMP("before buffer", reference_count(o));
        valid = PyObject_GetBuffer(o, &view, flags) == 0;
        if (valid) DUMP("after buffer", reference_count(o), view.obj == o);
    }

    static std::type_info const & format(std::string_view s);
    static std::string_view format(std::type_info const &t);
    static std::size_t itemsize(std::type_info const &t);
    // static Binary binary(Py_buffer *view, std::size_t len);
    // static Variable binary_view(Py_buffer *view, std::size_t len);

    ~Buffer() {
        if (valid) {
            PyObject *o = view.obj;
            DUMP("before release", reference_count(view.obj));
            PyBuffer_Release(&view);
            DUMP("after release", reference_count(o));
        }
    }
};

struct ArrayBuffer {
    std::vector<Py_ssize_t> shape_stride;
    std::size_t n_elem;
    Object base;
    void *data;
    std::type_info const * type;
    bool mutate;

    ArrayBuffer() noexcept = default;
    ArrayBuffer(ArrayView const &a, Object const &b) : n_elem(a.layout.n_elem()),
        base(b), data(const_cast<void *>(a.data.pointer())), type(&a.data.type()), mutate(a.data.mutate()) {
        for (std::size_t i = 0; i != a.layout.depth(); ++i)
            shape_stride.emplace_back(a.layout.shape(i));
        auto const item = Buffer::itemsize(*type);
        for (std::size_t i = 0; i != a.layout.depth(); ++i)
            shape_stride.emplace_back(a.layout.stride(i) * item);
    }
};

/******************************************************************************/

template <class T>
T * cast_if(PyObject *o) {
    if (!PyObject_TypeCheck(o, type_object<T>())) return nullptr;
    return std::addressof(reinterpret_cast<Holder<T> *>(o)->value);
}

template <class T>
T & cast_object(PyObject *o) {
    if (!PyObject_TypeCheck(o, type_object<T>()))
        throw std::invalid_argument("Expected instance of cpy.TypeIndex");
    return reinterpret_cast<Holder<T> *>(o)->value;
}

/******************************************************************************/

Variable variable_from_object(Object o);
Sequence args_from_python(Object const &pypack);
bool object_response(Variable &v, TypeIndex t, Object o);
std::string_view from_unicode(PyObject *o);

template <Qualifier Q>
struct Response<Object, Q> {
    bool operator()(Variable &v, TypeIndex t, Object o) const {
        DUMP("trying to get reference from qualified Object", Q, t);
        if (auto p = cast_if<Variable>(o)) {
            Dispatch msg;
            DUMP("requested qualified variable", t, p->type());
            v = p->reference().request_variable(msg, t);
            DUMP(p->type(), t, v.type());
        }
        return v.has_value();
    }
};

template <>
struct Response<Object, Value> {
    bool operator()(Variable &v, TypeIndex t, Object o) const {
        DUMP("trying to get reference from unqualified Object", t);
        if (!o) return false;
        DUMP("ref1", reference_count(o));
        Object type = Object(reinterpret_cast<PyObject *>((+o)->ob_type), true);
        if (auto p = input_conversions.find(type); p != input_conversions.end()) {
            Object guard(+o, false); // PyObject_CallFunctionObjArgs increments reference
            o = Object::from(PyObject_CallFunctionObjArgs(+p->second, +o, nullptr));
            type = Object(reinterpret_cast<PyObject *>((+o)->ob_type), true);
        }
        DUMP("ref2", reference_count(o));
        bool ok = object_response(v, t, std::move(o));
        DUMP("got response from object", ok);
        if (!ok) { // put diagnostic for the source type
            auto o = Object::from(PyObject_Repr(+type));
            DUMP("setting object error description", from_unicode(o));
            v = {Type<std::string>(), from_unicode(o)};
        }
        return ok;
    }
};

/******************************************************************************/

inline bool set_tuple_item(Object const &t, Py_ssize_t i, Object const &x) {
    if (!x) return false;
    incref(+x);
    PyTuple_SET_ITEM(+t, i, +x);
    return true;
}

template <class V, class F>
Object map_as_tuple(V &&v, F &&f) noexcept {
    auto out = Object::from(PyTuple_New(std::size(v)));
    auto it = std::begin(v);
    using T = std::conditional_t<std::is_rvalue_reference_v<V>,
        decltype(*it), decltype(std::move(*it))>;
    for (Py_ssize_t i = 0u; i != std::size(v); ++i, ++it)
        if (!set_tuple_item(out, i, f(static_cast<T>(*it)))) return {};
    return out;
}

template <class ...Ts>
Object args_as_tuple(Ts &&...ts) {
    auto out = Object::from(PyTuple_New(sizeof...(Ts)));
    Py_ssize_t i = 0;
    auto go = [&](Object const &x) {return set_tuple_item(out, i++, x);};
    return (go(ts) && ...) ? out : Object();
}

Object variable_cast(Variable &&v, Object const &t={});

inline Object args_to_python(Sequence &&s, Object const &sig={}) {
    if (sig && !PyTuple_Check(+sig))
        throw python_error(type_error("expected tuple but got %R", (+sig)->ob_type));
    std::size_t len = sig ? PyTuple_GET_SIZE(+sig) : 0;
    auto const n = s.size();
    auto out = Object::from(PyTuple_New(n));
    Py_ssize_t i = 0u;
    for (auto &v : s) {
        if (i < len) {
            PyObject *t = PyTuple_GET_ITEM(+sig, i);
            throw python_error(type_error("conversion to python signature not implemented yet"));
        } else {
            // special case: if given an rvalue reference, make it into a value
            Variable &&var = v.qualifier() == Rvalue ? v.copy() : std::move(v);
            if (!set_tuple_item(out, i, variable_cast(std::move(var)))) return {};
        }
        ++i;
    }
    return out;
}

/******************************************************************************/

template <class F>
void map_iterable(Object iterable, F &&f) {
    auto iter = Object::from(PyObject_GetIter(+iterable));
    while (true) {
        if (auto it = Object(PyIter_Next(+iter), false)) f(std::move(it));
        else return;
    }
}

/******************************************************************************/

/// RAII release of Python GIL
struct PythonFrame final : Frame {
    std::mutex mutex;
    PyThreadState *state = nullptr;
    bool no_gil;

    PythonFrame(bool no_gil) : no_gil(no_gil) {}

    void enter() override {
        DUMP("running with nogil=", no_gil);
        if (no_gil && !state) state = PyEval_SaveThread(); // release GIL
    }

    std::shared_ptr<Frame> operator()(std::shared_ptr<Frame> &&t) override {
        DUMP("suspended Python ", bool(t));
        if (no_gil || state) return std::move(t); // return this
        else return std::make_shared<PythonFrame>(no_gil); // return a new frame
    }

    // acquire GIL; lock mutex to prevent multiple threads trying to get the thread going
    void acquire() noexcept {if (state) {mutex.lock(); PyEval_RestoreThread(state);}}
    // release GIL; unlock mutex
    void release() noexcept {if (state) {state = PyEval_SaveThread(); mutex.unlock();}}

    ~PythonFrame() {if (state) PyEval_RestoreThread(state);}
};

/******************************************************************************/

/// RAII reacquisition of Python GIL
struct ActivePython {
    PythonFrame &lock;

    ActivePython(PythonFrame &u) : lock(u) {lock.acquire();}
    ~ActivePython() {lock.release();}
};

/******************************************************************************/

struct PythonFunction {
    Object function, signature;

    PythonFunction(Object f, Object s={}) : function(std::move(f)), signature(std::move(s)) {
        if (+signature == Py_None) signature = Object();
        if (!function)
            throw python_error(type_error("cannot convert null object to Function"));
        if (!PyCallable_Check(+function))
            throw python_error(type_error("expected callable type but got %R", (+function)->ob_type));
        if (+signature && !PyTuple_Check(+signature))
            throw python_error(type_error("expected tuple or None but got %R", (+signature)->ob_type));
    }

    /// Run C++ functor; logs non-ClientError and rethrows all exceptions
    Variable operator()(Caller c, Sequence args) const {
        DUMP("calling python function");
        auto p = c.target<PythonFrame>();
        if (!p) throw DispatchError("Python context is expired or invalid");
        ActivePython lk(*p);
        Object o = args_to_python(std::move(args), signature);
        if (!o) throw python_error();
        return Variable(Object::from(PyObject_CallObject(function, o)));
    }
};

/******************************************************************************/

std::string get_type_name(TypeIndex idx) noexcept;

std::string wrong_type_message(WrongType const &e, std::string_view={});

namespace runtime {
    char const * unknown_exception_description() noexcept;
}

/******************************************************************************/

template <class F>
PyObject *raw_object(F &&f) noexcept {
    try {
        Object o = static_cast<F &&>(f)();
        xincref(+o);
        return +o;
    } catch (PythonError const &) {
        return nullptr;
    } catch (std::bad_alloc const &e) {
        PyErr_SetString(PyExc_MemoryError, "C++: out of memory (std::bad_alloc)");
    } catch (WrongNumber const &e) {
        unsigned int n0 = e.expected, n = e.received;
        PyErr_Format(TypeErrorObject, "C++: wrong number of arguments (expected %u, got %u)", n0, n);
    } catch (WrongType const &e) {
        try {PyErr_SetString(TypeErrorObject, wrong_type_message(e, "C++: ").c_str());}
        catch(...) {PyErr_SetString(TypeErrorObject, e.what());}
    } catch (std::exception const &e) {
        if (!PyErr_Occurred())
            PyErr_Format(PyExc_RuntimeError, "C++: %s", e.what());
    } catch (...) {
        if (!PyErr_Occurred())
            PyErr_SetString(PyExc_RuntimeError, runtime::unknown_exception_description());
    }
    return nullptr;
}

/******************************************************************************/

}
