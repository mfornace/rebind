/**
 * @brief Python-related C++ API for cpy
 * @file PythonAPI.h
 */

#pragma once
#include "Document.h"

#include <mutex>
#include <complex>
#include <iostream>
#include <unordered_map>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wregister"
#include <Python.h>
#pragma GCC diagnostic pop

namespace cpy {

extern PyTypeObject FunctionType, ValueType, TypeIndexType;
extern std::unordered_map<std::type_index, std::string> type_names;

enum class Scalar {Bool, Char, SignedChar, UnsignedChar, Unsigned, Signed, Float, Pointer};
extern Zip<Scalar, std::type_index, unsigned> scalars;

/******************************************************************************/

template <class T>
struct Holder {
    PyObject_HEAD // 16 bytes for the ref count and the type object
    T value; // I think stack is OK because this object is only casted to anyway.
};

struct Object {
    PyObject *ptr = nullptr;
    Object() = default;
    Object(PyObject *o, bool incref) : ptr(o) {if (incref) Py_INCREF(ptr);}
    Object(Object const &o) : ptr(o.ptr) {Py_XINCREF(ptr);}
    Object(Object &&o) noexcept : ptr(std::exchange(o.ptr, nullptr)) {}
    Object & operator=(Object o) noexcept {ptr = std::exchange(o.ptr, nullptr); return *this;}

    explicit operator bool() const {return ptr;}
    PyObject *operator+() const {return ptr;}

    bool operator<(Object const &o) const {return ptr < o.ptr;}
    bool operator>(Object const &o) const {return ptr > o.ptr;}
    bool operator==(Object const &o) const {return ptr == o.ptr;}
    bool operator!=(Object const &o) const {return ptr != o.ptr;}
    bool operator<=(Object const &o) const {return ptr <= o.ptr;}
    bool operator>=(Object const &o) const {return ptr >= o.ptr;}
    friend void swap(Object &o, Object &p) {std::swap(o.ptr, p.ptr);}

    ~Object() {Py_XDECREF(ptr);}
};

/******************************************************************************/

struct Buffer {
    static Zip<std::string_view, std::type_index> formats;
    Py_buffer view;
    bool ok;

    Buffer(PyObject *o, int flags) {ok = PyObject_GetBuffer(o, &view, flags) == 0;}

    static std::type_index format(std::string_view s);
    static Binary binary(Py_buffer *view, std::size_t len);
    static Arg binary_view(Py_buffer *view, std::size_t len);

    ~Buffer() {PyBuffer_Release(&view);}
};

/******************************************************************************/

inline PyObject * type_object(PyTypeObject &o) noexcept {return reinterpret_cast<PyObject *>(&o);}

inline PyTypeObject & type_ref(Type<Function>) {return FunctionType;}
inline PyTypeObject & type_ref(Type<Value>) {return ValueType;}
inline PyTypeObject & type_ref(Type<std::type_index>) {return TypeIndexType;}

/******************************************************************************/

template <class T>
T * cast_if(PyObject *o) {
    if constexpr(std::is_same_v<T, Value>)
        if (!PyObject_TypeCheck(o, &ValueType)) return nullptr;
    if constexpr(std::is_same_v<T, Function>)
        if (!PyObject_TypeCheck(o, &FunctionType)) return nullptr;
    if constexpr(std::is_same_v<T, std::type_index>)
        if (!PyObject_TypeCheck(o, &TypeIndexType)) return nullptr;
    return std::addressof(reinterpret_cast<Holder<T> *>(o)->value);
}

template <class T>
T & cast_object(PyObject *o) {
    if constexpr(std::is_same_v<T, Value>) {
        if (!PyObject_TypeCheck(o, &ValueType)) throw std::invalid_argument("Expected instance of cpy.Value");
    }
    if constexpr(std::is_same_v<T, Function>) {
        if (!PyObject_TypeCheck(o, &FunctionType)) throw std::invalid_argument("Expected instance of cpy.Function");
    }
    if constexpr(std::is_same_v<T, std::type_index>) {
        if (!PyObject_TypeCheck(o, &TypeIndexType)) throw std::invalid_argument("Expected instance of cpy.TypeIndex");
    }
    return reinterpret_cast<Holder<T> *>(o)->value;
}

/******************************************************************************/

template <class T>
struct ToPython;

inline Object to_python(Object t) noexcept {return t;}

inline Object to_python(bool b) noexcept {return {b ? Py_True : Py_False, true};}

inline Object to_python(char const *s) noexcept {
    return {PyUnicode_FromString(s ? s : ""), false};
}

template <class T, std::enable_if_t<(std::is_integral_v<T> && sizeof(T) <= sizeof(long long)), int> = 0>
Object to_python(T t) noexcept {return {PyLong_FromLongLong(static_cast<long long>(t)), false};}

template <class T, std::enable_if_t<(std::is_enum_v<T>), int> = 0>
Object to_python(T t) noexcept {return to_python(static_cast<std::underlying_type_t<T>>(t));}

inline Object to_python(double t) noexcept {return {PyFloat_FromDouble(t), false};}

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

template <class T, std::enable_if_t<(sizeof(T) <= sizeof(double)), int> = 0>
Object to_python(std::complex<T> const &s) noexcept {
    return {PyComplex_FromDoubles(s.real(), s.imag()), false};
}

template <class CharT, class T, class A, std::enable_if_t<Reinterpretable<CharT, char>, int> = 0>
Object to_python(std::basic_string<CharT, T, A> const &s) noexcept {
    return {PyByteArray_FromStringAndSize(reinterpret_cast<char const *>(s.data()), s.size()), false};
}

template <class CharT, class T, std::enable_if_t<Reinterpretable<CharT, char>, int> = 0>
Object to_python(std::basic_string_view<CharT, T> const &s) noexcept {
    return {PyByteArray_FromStringAndSize(reinterpret_cast<char const *>(s.data()), s.size()), false};
}

inline Object to_python(Function f) noexcept {
    Object o{PyObject_CallObject(type_object(FunctionType), nullptr), false};
    cast_object<Function>(+o) = std::move(f);
    return o;
}

inline Object to_python(std::type_index f) noexcept {
    Object o{PyObject_CallObject(type_object(TypeIndexType), nullptr), false};
    cast_object<std::type_index>(+o) = std::move(f);
    return o;
}

template <class T, std::enable_if_t<std::is_base_of_v<Arg, no_qualifier<T>>, int> = 0>
Object to_python(T &&a) {
    if (!a.has_value()) return {Py_None, true};
    std::type_index const t = a.type();
    if (t == typeid(Object))           return std::any_cast<Object>(a);
    if (t == typeid(bool))             return to_python(std::any_cast<bool>(a));
    if (t == typeid(Integer))          return to_python(std::any_cast<Integer>(a));
    if (t == typeid(Real))             return to_python(std::any_cast<Real>(a));
    if (t == typeid(std::string_view)) return to_python(std::any_cast<std::string_view>(a));
    if (t == typeid(std::string))      return to_python(std::any_cast<std::string const &>(a));
    if (t == typeid(Function))         return to_python(std::any_cast<Function>(a));
    if (t == typeid(ValuePack))        return to_python(std::any_cast<ValuePack const &>(a));
    if (t == typeid(ArgPack))          return to_python(std::any_cast<ArgPack const &>(a));
    if (t == typeid(std::type_index))  return to_python(std::any_cast<std::type_index>(a));
    if (t == typeid(Binary))           return to_python(std::any_cast<Binary const &>(a));
    Object o{PyObject_CallObject(type_object(ValueType), nullptr), false};
    static_cast<std::any &>(cast_object<Value>(+o)) = static_cast<T &&>(a).base();
    return o;
}

ArgPack positional_args(Object const &pypack);

/******************************************************************************/

template <class V>
Object to_tuple(V &&v) noexcept {
    Object out = {PyTuple_New(std::size(v)), false};
    if (!out) return {};
    auto it = std::begin(v);
    for (Py_ssize_t i = 0u; i != std::size(v); ++i, ++it) {
        using T = std::conditional_t<std::is_rvalue_reference_v<V>,
            decltype(*it), decltype(std::move(*it))>;
        Object item = to_python(static_cast<T>(*it));
        if (!item) return {};
        Py_INCREF(+item);
        if (PyTuple_SetItem(+out, i, +item)) {Py_DECREF(+item); return {};}
    }
    return out;
}

template <class T, std::size_t ...Is>
Object to_tuple(T &&t, std::index_sequence<Is...>) {
    Object out = {PyTuple_New(sizeof...(Is)), false};
    if (!out) return {};
    bool ok = true;
    auto go = [&](auto i, auto &&x) {
        if (!ok) return;
        Object item = to_python(x);
        if (!item) {ok = false; return;}
        Py_INCREF(+item);
        if (PyTuple_SetItem(+out, i, +item)) {Py_DECREF(+item); ok = false;}
    };
    (go(Is, std::get<Is>(static_cast<T &&>(t))), ...);
    return ok ? out : Object();
}

template <class ...Ts>
Object to_python(std::tuple<Ts...> const &t) {
    return to_tuple(t, std::make_index_sequence<sizeof...(Ts)>());
}

template <class T, class U>
Object to_python(std::pair<T, U> const &t) {return to_tuple(t, std::make_index_sequence<2>());}

inline Object to_python(Methods const &m) {return to_python(std::tie(m.name, m.methods));}

template <class V>
Object to_dict(V const &v) noexcept {
    Object out = {PyDict_New(), false};
    if (!out) return {};
    for (auto const &x : v) {
        Object key = to_python(std::get<0>(x));
        if (!key) return {};
        Object value = to_python(std::get<1>(x));
        if (!value) return {};
        if (PyDict_SetItem(+out, +key, +value) != 0) return {};
    }
    return out;
}

// template <class T, std::enable_if_t<std::is_same_v<T, Value> || std::is_same_v<T, Value>, int> = 0>
// Object to_python(T const &s) noexcept;

inline Object to_python(ArgPack const &s) {
    auto const n = s.contents.size();
    Object out = {PyTuple_New(n), false};
    if (!out) return {};
    Py_ssize_t i = 0u;
    for (auto const &x : s.contents) {
        Object item = to_python(x);
        if (!item) return {};
        Py_INCREF(+item);
        if (PyTuple_SetItem(+out, i, +item)) {Py_DECREF(+item); return {};}
        ++i;
    }
    return out;
}

template <class T>
Object to_python(Vector<T> const &v) {return to_tuple(v);}

/******************************************************************************/

struct PythonError : ClientError {
    PythonError(char const *s) : ClientError(s) {}
};

PythonError python_error() noexcept;

/******************************************************************************/

template <class F>
void map_iterable(Object iterable, F &&f) {
    Object iter = {PyObject_GetIter(+iterable), false};
    if (!iter) throw python_error();

    while (true) {
        auto it = Object(PyIter_Next(+iter), false);
        if (!+it) return;
        f(std::move(it));
    }
}

/******************************************************************************/

/// RAII release of Python GIL
struct ReleaseGIL {
    PyThreadState * state;
    std::mutex mutex;
    ReleaseGIL(bool no_gil) : state(no_gil ? PyEval_SaveThread() : nullptr) {}
    // lock to prevent multiple threads trying to get the thread going
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

template <>
struct ToArg<Object> {
    Arg operator()(Object const &o) const;
};

template <>
struct ToValue<Object> {
    Value operator()(Object const &o) const;
};

struct PythonFunction {
    Object function;

    /// Run C++ functor; logs non-ClientError and rethrows all exceptions
    Value operator()(Caller ct, ArgPack args) const {
        AcquireGIL lk(&ct.cast<ReleaseGIL>());
        Object o = to_python(std::move(args));
        if (!o) throw python_error();
        o = {PyObject_CallObject(+function, +o), false};
        if (!o) throw python_error();
        return o;
    }
};

/******************************************************************************/

std::string_view get_type_name(std::type_index idx) noexcept;

std::string wrong_types_message(WrongTypes const &e);

template <class F>
PyObject *raw_object(F &&f) noexcept {
    try {
        Object o = static_cast<F &&>(f)();
        Py_XINCREF(+o);
        return +o;
    } catch (PythonError const &) {
        return nullptr;
    } catch (std::bad_alloc const &e) {
        PyErr_SetString(PyExc_MemoryError, "C++: out of memory (std::bad_alloc)");
    } catch (WrongNumber const &e) {
        unsigned int n0 = e.expected, n = e.received;
        PyErr_Format(PyExc_TypeError, "C++: wrong number of arguments (expected %u, got %u)", n0, n);
    } catch (WrongTypes const &e) {
        try {PyErr_SetString(PyExc_TypeError, wrong_types_message(e).c_str());}
        catch(...) {PyErr_SetString(PyExc_TypeError, e.what());}
    } catch (std::exception const &e) {
        if (!PyErr_Occurred())
            PyErr_Format(PyExc_RuntimeError, "C++: %s", e.what());
    } catch (...) {
        if (!PyErr_Occurred())
            PyErr_SetString(PyExc_RuntimeError, "C++: unknown exception");
    }
    return nullptr;
}

/******************************************************************************/

}
