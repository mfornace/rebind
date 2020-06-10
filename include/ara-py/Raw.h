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
#include <utility>

// #define ARA_PY_BEGIN ara::py { inline namespace v37
// #define ARA_PY_END }

/******************************************************************************/

namespace ara::py {


static constexpr auto Version = std::make_tuple(PY_MAJOR_VERSION, PY_MINOR_VERSION, PY_MICRO_VERSION);

/******************************************************************************/

template <class ...Ts>
std::nullptr_t type_error(char const *s, Ts ...ts) {
    PyErr_Format(PyExc_TypeError, s, ts...);
    return nullptr;
}

/******************************************************************************/

struct PythonError {
    PythonError(std::nullptr_t=nullptr) {}

    template <class ...Ts>
    static PythonError type(char const *s, Ts ...ts) {return type_error(s, ts...);}
};

/******************************************************************************/

enum class LockType {Read, Write};

/******************************************************************************/

template <class Module>
PyObject* init_module() noexcept;

/******************************************************************************/

struct Object {
    using type = PyObject;
};

template <class T>
struct Wrap : Object {};

template <class T>
struct Traits;

/******************************************************************************/

template <class T=Object>
struct Ptr {
    static_assert(std::is_base_of_v<Object, T>);
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

/******************************************************************************/

template <class T=Object>
struct Always : Ptr<T> {
    using type = typename T::type;
    using Base = Ptr<T>;
    using Base::base;

    // Always(T &t) : Base(std::addressof(t)) {}

    // template <bool B=true, std::enable_if_t<B && !std::is_same_v<T, type>, int> = 0>
    template <class U>
    explicit Always(U *t) noexcept : Base{reinterpret_cast<type*>(t)} {}
    Always(type &t) noexcept : Always(std::addressof(t)) {}


    operator Always<>() const noexcept {return *reinterpret_cast<PyObject*>(base);}

    // type& operator*() const {return *base;}
    // type* operator->() const {return base;}
    operator type&() const noexcept {return *base;}

    template <class U>
    static Always from(Always<U>);
    // template <class U>
    // Maybe<U> get() const;
};

/******************************************************************************/

template <class T=Object>
struct Maybe : Ptr<T> {
    using type = typename T::type;
    using Ptr<T>::base;

    constexpr Maybe(type *t) noexcept : Ptr<T>{t} {}
    constexpr Maybe(Always<T> o) noexcept : Ptr<T>{o.base} {}

    template <class U>
    explicit Maybe(Always<U> o) : Ptr<T>{T::check(o) ? reinterpret_cast<type*>(o.base) : nullptr} {}

    template <class U>
    explicit Maybe(Maybe<U> o) : Ptr<T>{o && T::check(*o) ? reinterpret_cast<type*>(o.base) : nullptr} {}

    Always<T> operator*() const {assert(base); return *base;}
};

// template <class T> template <class U>
// Maybe<U> Always<T>::get() const {
//     if constexpr(std::is_same_v<T, U>) {
//         return base;
//     } else {
//         return T::check(*this) ? base : nullptr;
//     }
// }

/******************************************************************************/

// RAII shared pointer interface to a possibly null Object
template <class T=Object>
struct Value : Maybe<T> {
    using Base = Maybe<T>;
    using Base::base;
    using type = typename T::type;

    Value() : Base(nullptr) {}
    Value(std::nullptr_t) : Base(nullptr) {}

    Value(type* o, bool increment) : Base{o} {if (increment) Py_XINCREF(base);}
    Value(Always<T> o, bool increment) : Base{+o} {if (increment) Py_INCREF(base);}

    template <class U, std::enable_if_t<!std::is_same_v<U, T> && std::is_same_v<T, Object>, int> = 0>
    Value(Value<U> v) : Base(reinterpret_cast<PyObject*>(std::exchange(v.base, nullptr))) {}

    static Value from(PyObject* o) {return o ? Value(o, false) : throw PythonError();}
    static Value alloc();

    Value(Value const &o) noexcept : Base{o.base} {Py_XINCREF(base);}
    Value & operator=(Value const &o) noexcept {base = o.base; Py_XINCREF(base); return *this;}

    Value(Value &&o) noexcept : Base{std::exchange(o.base, nullptr)} {}
    Value & operator=(Value &&o) noexcept {base = std::exchange(o.base, nullptr); return *this;}

    friend void swap(Value &o, Value &p) noexcept {std::swap(o.base, p.base);}

    type* leak() noexcept {return std::exchange(base, nullptr);}
    Always<T> operator*() const {assert(base); return *base;}

    ~Value() {Py_XDECREF(base);}
};

/******************************************************************************/

char const * unknown_exception_description() noexcept;

template <class T>
PyObject* leak(Value<T> s) noexcept {return s.leak();}

inline PyObject* leak(PyObject* s) noexcept {Py_INCREF(s); return s;}

template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
inline T leak(T t) noexcept {return t;}

template <auto F, class T, class ...Casts>
struct CallNoThrow;

template <auto F, class Out, class ...Args, class ...Casts>
struct CallNoThrow<F, Out(*)(Args...), Casts...> {
    static Out call(Args... args) noexcept {
        if constexpr(noexcept(F(static_cast<Casts>(args)...))) {
            return leak(F(static_cast<Casts>(args)...));
        } else {
            try {
                return leak(F(static_cast<Casts>(args)...));
            } catch (PythonError const &) {
                return nullptr;
            } catch (std::bad_alloc const &e) {
                PyErr_SetString(PyExc_MemoryError, "C++: out of memory (std::bad_alloc)");
            } catch (std::exception const &e) {
                if (!PyErr_Occurred())
                    PyErr_Format(PyExc_RuntimeError, "C++: %s", e.what());
            } catch (...) {
                if (!PyErr_Occurred())
                    PyErr_SetString(PyExc_RuntimeError, unknown_exception_description());
            }
            return nullptr;
        }
    }
};

template <auto F, class ...Casts>
struct NoThrow {
    template <class T>
    constexpr operator T() const noexcept {return CallNoThrow<F, T, Casts...>::call;}
};

template <auto F, class ...Casts>
static constexpr NoThrow<F, Casts...> api{};

/******************************************************************************/

// Non null wrapper for Object pointer
// template <class T=PyObject>
// struct Instance {
//     // static_assert(!std::is_pointer_v<T>);

//     T *ptr;
//     explicit constexpr Instance(T *b) noexcept __attribute__((nonnull (2))) : ptr(b) {}
//     constexpr Instance(T &t) : ptr(std::addressof(t)) {}

//     constexpr T* operator+() const noexcept __attribute__((returns_nonnull)) {return ptr;}
//     PyObject* Object() const noexcept __attribute__((returns_nonnull)) {return reinterpret_cast<PyObject*>(ptr);}

//     template <class To>
//     Instance<To> as() const {return Instance<To>(reinterpret_cast<To *>(ptr));}

//     constexpr bool operator==(Instance const &other) noexcept {return ptr == other.ptr;}
// };

// // inline constexpr Instance<> instance(PyObject* t) {return Instance<>(t);}

// template <class T>
// constexpr Instance<T> instance(T& t) noexcept {return t;}
//     // if (!t) throw std::runtime_error("bad!");
//     return Instance<T>(t);
// }

// template <class T>
// Instance<T> instance(PyObject* t) {return Instance<T>(reinterpret_cast<T*>(t));}

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
        size_t operator()(ara::py::Value<T> const &o) const {return std::hash<T*>()(o.base);}
    };
}