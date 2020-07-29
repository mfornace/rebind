#pragma once
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wregister"
#ifndef PY_SSIZE_T_CLEAN
#   define PY_SSIZE_T_CLEAN
#endif
#include <Python.h>
#pragma GCC diagnostic pop

#include <ara/API.h>
#include <ara/Type.h>
#include <ara/Index.h>
#include <ara/Common.h>
#include <utility>

// #define ARA_PY_BEGIN ara::py { inline namespace v37
// #define ARA_PY_END }

/******************************************************************************/

namespace ara::py {

static constexpr auto Version = std::make_tuple(PY_MAJOR_VERSION, PY_MINOR_VERSION, PY_MICRO_VERSION);

/******************************************************************************/

template <class ...Ts>
std::nullptr_t set_error(PyObject* exc, char const *s, Ts ...ts) {
    PyErr_Format(exc, s, ts...);
    return nullptr;
}

template <class ...Ts>
std::nullptr_t type_error(char const *s, Ts ...ts) {return set_error(PyExc_TypeError, s, ts...);}

/******************************************************************************/

struct PythonError {
    PythonError(std::nullptr_t=nullptr) {}

    template <class ...Ts>
    static PythonError type(char const *s, Ts ...ts) {return type_error(s, ts...);}
    
    static PythonError attribute(PyObject* k) {return set_error(PyExc_AttributeError, "No attribute %R", k);}
    
    static PythonError index(PyObject* k) {return set_error(PyExc_IndexError, "Index %R out of range", k);}
};

/******************************************************************************/

enum class LockType {Read, Write};

/******************************************************************************/

template <class Module>
PyObject* init_module() noexcept;

/******************************************************************************/

struct Object {
    using type = PyObject;
    static bool check(Ignore) {return true;}
};

template <class T>
struct Wrap : Object {};

template <class T>
struct Traits;

/******************************************************************************/

template <class T=Object>
struct Ptr {
    static_assert(std::is_base_of_v<Object, T>);
    static_assert(std::is_empty_v<T>);
    using type = typename T::type;
    type *base;

    bool operator<(Ptr const &o) const {return base < o.base;}
    bool operator>(Ptr const &o) const {return base > o.base;}
    bool operator==(Ptr const &o) const {return base == o.base;}
    bool operator!=(Ptr const &o) const {return base != o.base;}
    bool operator<=(Ptr const &o) const {return base <= o.base;}
    bool operator>=(Ptr const &o) const {return base >= o.base;}

    explicit operator bool() const {return base;}

    // Access to raw PyObject*
    PyObject* operator~() const {return reinterpret_cast<PyObject*>(base);}
    // Access to the inner type
    type* operator+() const {return base;}
    // Access to the wrapping type
    type* operator->() const {assert(base); return base;}
    type& operator*() const {assert(base); return *base;}
};

template <class T>
std::size_t reference_count(Ptr<T> o) {return o ? Py_REFCNT(~o) : 0u;}

/******************************************************************************/

template <class T=Object>
struct Always : Ptr<T> {
    using type = typename T::type;
    using Base = Ptr<T>;
    using Base::base;

    Always(type &t) noexcept : Base{std::addressof(t)} {}

    static Always from_raw(void* t) noexcept {return *static_cast<type*>(t);}

    operator Always<>() const noexcept {return *reinterpret_cast<PyObject*>(base);}

    operator type&() const noexcept {return *base;}

    type* operator->() const {return base;}
    type& operator*() const {return *base;}

    template <class U>
    static Always from(Always<U> o) {
        return T::check(o) ? *reinterpret_cast<type*>(o.base) : throw PythonError::type("bad");
    }
};

template <class T=Object>
struct Bound : Always<T> {
    Bound(Always<T> a) noexcept : Always<T>(a) {Py_INCREF(this->base);}
    Bound(Bound&&) = delete;
    Bound(Bound const&) = delete;
    Bound& operator=(Bound&&) = delete;
    Bound& operator=(Bound const&) = delete;
    ~Bound() noexcept {Py_DECREF(this->base);}
};

/******************************************************************************/

template <class T=Object>
struct Maybe : Ptr<T> {
    using type = typename T::type;
    using Ptr<T>::base;

    constexpr Maybe() noexcept : Ptr<T>{nullptr} {}
    constexpr Maybe(type *t) noexcept : Ptr<T>{t} {}
    constexpr Maybe(Always<T> o) noexcept : Ptr<T>{o.base} {}

    template <class U>
    explicit Maybe(Always<U> o) : Ptr<T>{T::check(o) ? reinterpret_cast<type*>(o.base) : nullptr} {}

    template <class U>
    explicit Maybe(Maybe<U> o) : Ptr<T>{o && T::check(*o) ? reinterpret_cast<type*>(o.base) : nullptr} {}

    Always<T> operator*() const {assert(base); return *base;}
};

/******************************************************************************/

struct Construct {};
static constexpr Construct construct{};

// RAII shared pointer interface to a possibly null Object
template <class T=Object>
struct Value : Maybe<T> {
    using Base = Maybe<T>;
    using Base::base;
    using type = typename T::type;

    Value() noexcept : Base(nullptr) {}
    Value(std::nullptr_t) noexcept : Base(nullptr) {}

    Value(type* o, bool increment) noexcept : Base{o} {if (increment) Py_XINCREF(base);}

    template <class U, std::enable_if_t<std::is_same_v<U, T> || std::is_same_v<T, Object>, int> = 0>
    Value(Always<U> o) noexcept : Base{reinterpret_cast<type*>(o.base)} {Py_INCREF(base);}

    template <class U, std::enable_if_t<std::is_same_v<U, T> || std::is_same_v<T, Object>, int> = 0>
    Value(Maybe<U> o) noexcept : Base{reinterpret_cast<type*>(o.base)} {Py_XINCREF(base);}

    template <class U, std::enable_if_t<!std::is_same_v<U, T> && std::is_same_v<T, Object>, int> = 0>
    Value(Value<U> v) noexcept : Base(reinterpret_cast<PyObject*>(std::exchange(v.base, nullptr))) {}

    static Value take(PyObject* o) {return o ? Value(reinterpret_cast<type*>(o), false) : throw PythonError();}

    template <class ...Args>
    static Value new_from(Args &&...args);

    Value(Value const &o) noexcept : Base{o.base} {Py_XINCREF(base);}
    Value & operator=(Value const &o) noexcept {base = o.base; Py_XINCREF(base); return *this;}

    Value(Value &&o) noexcept : Base{std::exchange(o.base, nullptr)} {}
    Value & operator=(Value &&o) noexcept {base = std::exchange(o.base, nullptr); return *this;}

    friend void swap(Value &o, Value &p) noexcept {std::swap(o.base, p.base);}

    PyObject* leak() noexcept {return reinterpret_cast<PyObject*>(std::exchange(base, nullptr));}
    Always<T> operator*() const {assert(base); return *base;}

    template <class U>
    static Value from(Always<U>);

    ~Value() {Py_XDECREF(base);}
};

static_assert(!std::is_constructible_v<Value<>, bool>);

/******************************************************************************/

template <class T>
PyObject* leak(Value<T> s) noexcept {return s.leak();}

template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
inline constexpr T leak(T t) noexcept {return t;}

template <auto F, auto Bad, class T, class ...Casts>
struct ReinterpretCall;

template <class T, class U>
T argument_cast(U u) noexcept {
    if constexpr(std::is_constructible_v<T, U>) {
        return static_cast<T>(u);
    } else {
        return T::from_raw(u);
    }
}

template <auto F, auto Bad, class Out, class ...Args, class ...Casts>
struct ReinterpretCall<F, Bad, Out(*)(Args...), Casts...> {
    static Out call(Args... args) noexcept {
        if constexpr(noexcept(F(argument_cast<Casts>(args)...))) {
            return leak(F(argument_cast<Casts>(args)...));
        } else {
            try {
                return leak(F(argument_cast<Casts>(args)...));
                // DUMP("output:", type_name<T>(), bool(out), "ref count =", reference_count(out));
                // DUMP("output reference count", Py_REFCNT(out), type_name<decltype(out)>());
            } catch (PythonError const &) {
                return Bad;
            } catch (std::bad_alloc const &e) {
                PyErr_SetString(PyExc_MemoryError, "C++: out of memory (std::bad_alloc)");
            } catch (std::exception const &e) {
                if (!PyErr_Occurred())
                    PyErr_Format(PyExc_RuntimeError, "C++: %s", e.what());
            } catch (...) {
                if (!PyErr_Occurred())
                    PyErr_SetString(PyExc_RuntimeError, unknown_exception_description());
            }
            return Bad;
        }
    }
};

template <auto F, auto Bad, class ...Casts>
struct Reinterpret {
    template <class T>
    constexpr operator T() const noexcept {
        if constexpr(std::is_same_v<T, PyCFunction> && sizeof...(Casts) == 3) {
            return reinterpret_cast<PyCFunction>(
                ReinterpretCall<F, Bad, PyCFunctionWithKeywords, Casts...>::call);
        } else {
            return ReinterpretCall<F, Bad, T, Casts...>::call;
        }
    }
};

template <auto F, auto Bad, class ...Casts>
static constexpr Reinterpret<F, Bad, Casts...> reinterpret{};

/******************************************************************************/

template <int M, int N>
union PythonObject {PyObject* base;};

using Export = PythonObject<PY_MAJOR_VERSION, PY_MINOR_VERSION>;

// Dump
//  {
//     PyErr_Format(PyExc_ImportError, "Python version %d.%d was not compiled by the ara library", Major, Minor);
//     return nullptr;
// }

// template <>
// PyObject* init_module<PY_MAJOR_VERSION, PY_MINOR_VERSION>(Value<PY_MAJOR_VERSION, PY_MINOR_VERSION> const &);


}

namespace std {
    template <class T>
    struct hash<ara::py::Value<T>> {
        size_t operator()(ara::py::Value<T> const &o) const {return std::hash<typename T::type*>()(o.base);}
    };
}