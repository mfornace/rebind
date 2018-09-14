/**
 * @brief C++ type-erased Value object
 * @file Value.h
 */

#pragma once
#include "Error.h"
#include "Signature.h"
#include "Common.h"

#include <iostream>
#include <vector>
#include <type_traits>
#include <string_view>
#include <any>
#include <typeindex>

#define CPY_CAT_IMPL(s1, s2) s1##s2
#define CPY_CAT(s1, s2) CPY_CAT_IMPL(s1, s2)

#define CPY_STRING_IMPL(x) #x
#define CPY_STRING(x) CPY_STRING_IMPL(x)

namespace cpy {

struct Value;

/******************************************************************************/

enum class Qualifier : unsigned char {C, L, R};

struct rvalue {};
struct lvalue {};
struct cvalue {};

class TypeRequest {
    std::type_index const *m_begin=nullptr;
    std::type_index const *m_end=nullptr;
public:
    constexpr TypeRequest() = default;
    constexpr TypeRequest(std::type_index const *b, std::type_index const *e) : m_begin(b), m_end(e) {}
    TypeRequest(std::initializer_list<std::type_index> const &t) : m_begin(t.begin()), m_end(t.end()) {}
    auto begin() const {return m_begin;}
    auto end() const {return m_end;}
    auto data() const {return m_begin;}
    auto size() const {return m_end - m_begin;}

    auto contains(std::type_index t) const {return std::find(m_begin, m_end, t) != m_end;}
};

using CopyFunction = Value(*)(void const *, TypeRequest const &);

template <class T>
struct CopyImpl {
    static Value apply(void const *, TypeRequest const &);
};

// template <class T>
// struct ToReference {
// for any & you should have the address, the index,
// };

class Reference {
    void *ptr = nullptr;
    std::type_index idx = typeid(void);
    CopyFunction copy = nullptr;
    Qualifier qual;
public:
    Reference() = default;

    template <class T, std::enable_if_t<!std::is_base_of_v<Reference, T>, int> = 0>
    Reference(T &&t) : ptr(std::addressof(t)), idx(typeid(T)), copy(CopyImpl<T>::apply), qual(Qualifier::R) {}

    template <class T, std::enable_if_t<!std::is_base_of_v<Reference, T>, int> = 0>
    Reference(T &t) : ptr(std::addressof(t)), idx(typeid(T)), copy(CopyImpl<T>::apply), qual(Qualifier::L) {}

    template <class T, std::enable_if_t<!std::is_base_of_v<Reference, T>, int> = 0>
    Reference(T const &t) : ptr(const_cast<T *>(std::addressof(t))), idx(typeid(T)), copy(CopyImpl<T>::apply), qual(Qualifier::C) {}

    std::type_index type() const {return idx;}
    Qualifier qualifier() const {return qual;}

    Value value(TypeRequest const &t={}) const;

    template <class T, std::enable_if_t<std::is_lvalue_reference_v<T> && !std::is_const_v<std::remove_reference_t<T>>, int> = 0>
    std::remove_reference_t<T> *target(Type<T> t={}) const {
        return (idx == +t && qual == Qualifier::L) ? static_cast<std::remove_reference_t<T> *>(ptr) : nullptr;
    }

    template <class T, std::enable_if_t<std::is_rvalue_reference_v<T>, int> = 0>
    std::remove_reference_t<T> *target(Type<T> t={}) const {
        return (idx == +t && qual == Qualifier::R) ? static_cast<std::remove_reference_t<T> *>(ptr) : nullptr;
    }

    template <class T, std::enable_if_t<std::is_lvalue_reference_v<T> && std::is_const_v<std::remove_reference_t<T>>, int> = 0>
    std::remove_reference_t<T> *target(Type<T> t={}) const {
        return (idx == +t) ? static_cast<std::remove_reference_t<T> *>(ptr) : nullptr;
    }

    template <class T, std::enable_if_t<!std::is_reference_v<T>, int> = 0>
    T unsafe_target(Type<T> t={}) const {
        if (qual == Qualifier::R) return std::move(*static_cast<T *>(ptr));
        else return *static_cast<T const *>(ptr);
    }
};

static_assert(std::is_constructible_v<Reference, Reference>);

/******************************************************************************/

struct Value : std::any {
    constexpr Value() = default;

    template <class T>
    Value(std::in_place_t, T &&t) : any(static_cast<T &&>(t)) {}

    template <class T>
    static Value from_any(T &&t) {return {std::in_place_t(), static_cast<T &&>(t)};}

    template <class T, class ...Ts>
    Value(Type<T> t, Ts &&...ts) : any(std::in_place_type_t<T>(), static_cast<Ts &&>(ts)...) {}

    Value(Reference &&r) : Value(r.value()) {}
    Value(Reference const &r) : Value(r.value()) {}

    template <class T, std::enable_if_t<(!std::is_base_of_v<std::any, no_qualifier<T>>), int> = 0>
    Value(T &&t) : Value(Reference(static_cast<T &&>(t)).value()) {}

    std::any && base() && noexcept {return std::move(*this);}
    std::any const & base() const & noexcept {return *this;}
};

static_assert(std::is_copy_constructible_v<Value>);
static_assert(std::is_move_constructible_v<Value>);
static_assert(32 == sizeof(Value));              // 8 + 24 buffer I think

inline Value Reference::value(TypeRequest const &t) const {
    std::cout << (copy != nullptr) << t.size() << std::endl;
    return copy(ptr, t);
}

template <class T, class=void>
struct ToValue {
    static_assert(std::is_same_v<no_qualifier<T>, T>);
    Value operator()(T const &t, TypeRequest const &) const {
        std::cout << typeid(T).name() << std::endl;
        auto out = Value::from_any(t);
        std::cout << typeid(T).name() << "2" << std::endl;
        return out;
    }
};

template <class T>
Value CopyImpl<T>::apply(void const *p, TypeRequest const &req) {
    std::cout << "running" << std::endl;
    static_assert(std::is_same_v<Value, no_qualifier<decltype(ToValue<T>()(*static_cast<T const *>(p), req))>>);
    return ToValue<T>()(*static_cast<T const *>(p), req);
}

/******************************************************************************/

/// The 4 default behaviors for casting a reference to an expected type
template <class T, class=void>
struct FromReference {
    static_assert(std::is_same_v<no_qualifier<T>, T>);

    T operator()(Reference const &r, Dispatch &msg) const {

        throw msg.error("mismatched class type", r.type(), typeid(T));
    }
};

template <class T, class C>
struct FromReference<T &, C> {
    T & operator()(Reference const &r, Dispatch &msg) const {
        throw msg.error("could not bind to lvalue reference", r.type(), typeid(T));
    }
};

template <class T, class C>
struct FromReference<T const &, C> {
    T const & operator()(Reference const &r, Dispatch &msg) const {
        throw msg.error("could not bind to const lvalue", r.type(), typeid(T));
    }
};

template <class T, class C>
struct FromReference<T &&, C> {
    static_assert(T::aa);
    T && operator()(Reference const &r, Dispatch &msg) const {
        throw msg.error("could not bind to rvalue", r.type(), typeid(T));
    }
};

// /// Default ToValue passes lvalues as Reference and rvalues as Value
// template <class T, class>
// struct ToValue {
//     static_assert(!std::is_base_of_v<T, Value>);

//     Value arg(T &t) const {
//         if (Debug) std::cout << "ref " << typeid(T &).name() << std::endl;
//         return {Type<Reference<T &>>(), t};
//     }
//     Value arg(T const &t) const {
//         if (Debug) std::cout << "cref " << typeid(T const &).name() << std::endl;
//         return {Type<Reference<T const &>>(), t};
//     }
//     Value arg(T &&t) const {
//         if (Debug) std::cout << "rref " << typeid(T).name() << std::endl;
//         return ToValue<T>()(std::move(t));
//     }

//     /// The default implementation is to serialize to Value without conversion
//     Value value(T &&t) const {return Value::from_any(static_cast<T &&>(t));}

//     Value value(T const &t) const {return Value::from_any(t);}
// };

/// A common behavior passing the argument directly by value into an Value without conversion
// struct ToArgFromAny {
//     template <class T>
//     Value arg(T t) const {
//         if (Debug) std::cout << "convert directly to arg " << typeid(T).name() << std::endl;
//         return Value::from_any(std::move(t));}
// };

// template <>
// struct ToValue<Reference<Value &>> : ToArgFromAny {};

// template <>
// struct ToValue<Reference<Value const &>> : ToArgFromAny {};

// inline Value to_value(std::nullptr_t) {return {};}

// /// ADL version
// template <class T>
// struct ToValue<T, std::void_t<decltype(to_value(std::declval<T>()))>> {
//     Value operator()(T &&t) const {return to_value(static_cast<T &&>(t));}
//     Value operator()(T const &t) const {return to_value(t);}
// };

/******************************************************************************/

// /// If the type is matched exactly, return it
// template <class T, class=void>
// struct FromValue {
//     T operator()(Value v, Dispatch &msg) {
//         if (Debug) std::cout << v.type().name() << " " << typeid(T).name() << std::endl;
//         if (auto p = std::any_cast<T>(&v)) return std::move(*p);
//         throw msg.error("mismatched class type", v.type(), typeid(T));
//     }
// };

// /// I don't know why these are that necessary? Not sure when you'd expect FromValue to ever return a reference
// template <class T, class V>
// struct FromValue<T &, V> {
//     T & operator()(Value const &v, Dispatch &msg) {
//         throw msg.error("cannot form lvalue reference", v.type(), typeid(T));
//     }
// };

// template <class T, class V>
// struct FromValue<T const &, V> {
//     T const & operator()(Value const &v, Dispatch &msg) {
//         throw msg.error("cannot form const lvalue reference", v.type(), typeid(T));
//     }
// };

// template <class T, class V>
// struct FromValue<T &&, V> {
//     T && operator()(Value const &v, Dispatch &msg) {
//         throw msg.error("cannot form rvalue reference", v.type(), typeid(T));
//     }
// };

/******************************************************************************/

/// The default implementation is to accept convertible arguments or Value of the exact typeid match
/// castvalue always needs an output Value &, and an input Value
// template <class T, class=void>
// struct FromReference {
//     /// If exact match, return it, else return FromValue implementation
//     T operator()(Value &out, Reference const &in, Dispatch &msg) const {
//         if (auto t = std::any_cast<T>(&in))
//             return *t;
//         return FromValue<T>()(Value::from_any(in.base()), msg);
//     }
//     /// Remove Reference wrappers
//     T operator()(Value &out, Reference &&in, Dispatch &msg) const {
//         if (Debug) std::cout << "casting FromReference " << in.type().name() << " to " <<  typeid(T).name() << std::endl;
//         if (auto t = std::any_cast<T>(&in))
//             return std::move(*t);
//         if (auto p = std::any_cast<Reference<T const &>>(&in))
//             return p->get();
//         if (auto p = std::any_cast<Reference<T &>>(&in))
//             return p->get();
//         if (auto p = std::any_cast<Reference<Value const &>>(&in))
//             return (*this)(out, p->get(), msg);
//         if (auto p = std::any_cast<Reference<Value &>>(&in)) {
//             if (Debug) std::cout << "casting reference " << p->get().type().name() << std::endl;
//             return (*this)(out, p->get(), msg);
//         }
//         return FromValue<T>()(Value::from_any(std::move(in).base()), msg);
//     }
// };

// template <class T, class V>
// struct FromReference<T &, V> {
//     T & operator()(Value &out, Value &in, Dispatch &msg) const {
//         if (!in.has_value())
//             throw msg.error("object was already moved", in.type(), typeid(T));
//         /// Must be passes by & wrapper
//         if (auto p = std::any_cast<Reference<Value &>>(&in)) {
//             if (auto t = std::any_cast<T>(&p->get())) return *t;
//             return FromReference<T &>()(out, std::move(p->get()), msg);
//         }
//         return FromValue<T &>()(Value::from_any(in.base()), msg);
//     }

//     T & operator()(Value &out, Value &&in, Dispatch &msg) const {
//         if (!in.has_value())
//             throw msg.error("object was already moved", in.type(), typeid(T));
//         /// Must be passes by & wrapper
//         if (auto p = std::any_cast<Reference<Value &>>(&in)) {
//             if (auto t = std::any_cast<T>(&p->get())) return *t;
//             return FromReference<T &>()(out, p->get(), msg);
//         }
//         return FromValue<T &>()(Value::from_any(std::move(in).base()), msg);
//     }
// };

// template <class T, class V>
// struct FromReference<T const &, V> {
//     T const & operator()(Value &out, Value const &in, Dispatch &msg) const {
//         if (auto p = std::any_cast<no_qualifier<T>>(&in))
//             return *p;
//         /// Check for & and const & wrappers
//         if (auto p = std::any_cast<Reference<Value &>>(&in)) {
//             if (auto t = std::any_cast<T>(&p->get())) return *t;
//             return (*this)(out, p->get(), msg);
//         }
//         if (auto p = std::any_cast<Reference<Value const &>>(&in)) {
//             if (auto t = std::any_cast<T>(&p->get())) return *t;
//             return (*this)(out, p->get(), msg);
//         }
//         /// To bind a temporary to a const &, we store it in the out value
//         return out.emplace<T>(FromReference<T>()(out, in, msg));
//     }
// };

// template <class T, class V>
// struct FromReference<T &&, V> {
//     T && operator()(Value &out, Value &&in, Dispatch &msg) const {
//         /// No reference wrappers are used here, simpler to just move the Value in
//         /// To bind a temporary to a &&, we store it in the out value
//         return std::move(out.emplace<T>(FromReference<no_qualifier<T>>()(out, std::move(in), msg)));
//     }
// };

template <class T>
T downcast(Reference const &r, Dispatch &msg) {
    if (Debug) std::cout << "casting " << r.type().name() << " to " << typeid(T).name() << std::endl;
    if constexpr(std::is_convertible_v<Value &&, T>) return r.value();
    else if constexpr(!std::is_reference_v<T>) {
        std::cout << "not reference " << typeid(T).name() << std::endl;
        if (r.type() == typeid(no_qualifier<T>)) return r.unsafe_target<T>();
        return FromReference<T>()(r, msg);
    } else {
        std::cout << "reference" << typeid(T).name() << std::endl;
        if (auto p = r.target<T>()) return static_cast<T>(*p);
        return FromReference<T>()(r, msg);
    }
}

// template <class T>
// T downcast(Reference const &r) {
//     Dispatch msg;
//     return downcast<T>(r, msg);
// }

/******************************************************************************/

}
