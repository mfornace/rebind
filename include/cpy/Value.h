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
#include <typeindex>
#include <optional>

#define CPY_CAT_IMPL(s1, s2) s1##s2
#define CPY_CAT(s1, s2) CPY_CAT_IMPL(s1, s2)

#define CPY_STRING_IMPL(x) #x
#define CPY_STRING(x) CPY_STRING_IMPL(x)

namespace cpy {

struct Info;
class Value;
class Reference;

/******************************************************************************/

enum class Qualifier : unsigned char {C, L, R};

struct cvalue {constexpr operator Qualifier() const {return Qualifier::C;}};
struct lvalue {constexpr operator Qualifier() const {return Qualifier::L;}};
struct rvalue {constexpr operator Qualifier() const {return Qualifier::R;}};

template <class T, class Ref> struct Qualified;
template <class T> struct Qualified<T, cvalue> {using type = T const &;};
template <class T> struct Qualified<T, lvalue> {using type = T &;};
template <class T> struct Qualified<T, rvalue> {using type = T &&;};

template <class Ref, class T> using qualified = typename Qualified<Ref, T>::type;

template <class T>
static constexpr Qualifier qualifier = std::is_rvalue_reference_v<T> ? Qualifier::R :
    (std::is_const_v<std::remove_reference_t<T>> ? Qualifier::C : Qualifier::L);

/******************************************************************************/

enum class ActionType : unsigned char {destroy, copy, move, make_value, to_reference};

using ActionFunction = void *(*)(ActionType, void *, Info *);

struct Info {
    void *ptr = nullptr;
    std::type_index idx = typeid(void);
    ActionFunction fun = nullptr;

    explicit constexpr operator bool() const {return ptr;}
    constexpr bool has_value() const {return ptr;}
    auto name() const {return idx.name();}

    std::type_index type() const {return idx;}
};

template <class T>
struct Action;

class Reference : public Info {
    Qualifier qual;
public:
    Reference() = default;

    Reference(void *p, std::type_index t, ActionFunction f, Qualifier q) : Info{p, t, f}, qual(q) {}

    template <class V, std::enable_if_t<std::is_same_v<no_qualifier<V>, Value>, int> = 0>
    explicit Reference(V &&v) : Info{static_cast<V &&>(v)}, qual(::cpy::qualifier<V &&>) {}

    template <class T, std::enable_if_t<!std::is_base_of_v<Info, no_qualifier<T>>, int> = 0>
    explicit Reference(T &&t) : Info{std::addressof(t), typeid(T), Action<T>::apply}, qual(Qualifier::R) {}

    template <class T, std::enable_if_t<!std::is_base_of_v<Info, T>, int> = 0>
    explicit Reference(T &t) : Info{std::addressof(t), typeid(T), Action<T>::apply}, qual(Qualifier::L) {}

    template <class T, std::enable_if_t<!std::is_base_of_v<Info, T>, int> = 0>
    explicit Reference(T const &t) : Info{const_cast<T *>(std::addressof(t)), typeid(T), Action<T>::apply}, qual(Qualifier::C) {}

    Qualifier qualifier() const {return qual;}

    Value request(std::type_index const) const;

    template <class T>
    std::optional<T> request(Type<T> t={}) const;

    template <class T>
    std::remove_reference_t<T> *to_reference(Type<T> t={}) const;

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
    T unsafe_cast(Type<T> t={}) const {
        if (qual == Qualifier::R) return std::move(*static_cast<T *>(ptr));
        else return *static_cast<T const *>(ptr);
    }
};

static_assert(std::is_trivially_destructible_v<Reference>);
static_assert(std::is_constructible_v<Reference, Reference>);

/******************************************************************************/

class Value : public Info {
public:
    Value() = default;

    template <class T, class ...Ts>
    Value(Type<T> t, Ts &&...ts) :
        Info{new T(static_cast<Ts &&>(ts)...), typeid(T), Action<T>::apply} {
            static_assert(std::is_same_v<no_qualifier<T>, T>);
            static_assert(!std::is_same_v<no_qualifier<T>, Reference>);
            static_assert(!std::is_same_v<no_qualifier<T>, Value>);
            static_assert(!std::is_same_v<no_qualifier<T>, void *>);
        }

    template <class T, std::enable_if_t<!std::is_base_of_v<Info, no_qualifier<T>>, int> = 0>
    Value(T &&t) : Value(+Type<T>(), static_cast<T &&>(t)) {
        static_assert(!std::is_same_v<no_qualifier<T>, Reference>);
        static_assert(!std::is_same_v<no_qualifier<T>, Value>);
        static_assert(!std::is_same_v<no_qualifier<T>, void *>);
    }

    explicit Value(Info const &other) : Info(other) {
        ptr = fun(ActionType::copy, ptr, nullptr);
    }

    Value(Value &&v) noexcept : Info{std::exchange(v.ptr, nullptr), std::exchange(v.idx, typeid(void)), v.fun} {}

    Value(Value const &v) : Info{v.ptr ? v.fun(ActionType::copy, v.ptr, nullptr) : nullptr, v.idx, v.fun} {}

    Value & operator=(Value &&v) noexcept {
        if (ptr) fun(ActionType::destroy, ptr, nullptr);
        ptr = std::exchange(v.ptr, nullptr);
        idx = std::exchange(v.idx, typeid(void));
        fun = v.fun;
        return *this;
    }

    Value & operator=(Value const &v) {
        if (ptr) fun(ActionType::destroy, ptr, nullptr);
        ptr = v.ptr ? v.fun(ActionType::copy, v.ptr, nullptr) : nullptr;
        idx = v.idx;
        fun = v.fun;
        return *this;
    }

    void reset() {
        if (ptr) {
            fun(ActionType::destroy, ptr, nullptr);
            fun = nullptr;
            ptr = nullptr;
            idx = typeid(void);
        }
    }

    ~Value() {if (ptr) fun(ActionType::destroy, ptr, nullptr);}

    template <class T>
    static Value from_any(T &&t) {return {std::in_place_t(), static_cast<T &&>(t)};}

    template <class T>
    T *target() {return idx == typeid(T) ? static_cast<T *>(ptr) : nullptr;}

    template <class T>
    T const *target() const {return idx == typeid(T) ? static_cast<T const *>(ptr) : nullptr;}

    template <class T>
    T unsafe_cast(Type<T> t={}) const & {return static_cast<T>(*static_cast<no_qualifier<T> const *>(ptr));}

    template <class T>
    T unsafe_cast(Type<T> t={}) & {return static_cast<T>(*static_cast<no_qualifier<T> *>(ptr));}

    template <class T>
    T unsafe_cast(Type<T> t={}) && {return static_cast<T>(std::move(*static_cast<no_qualifier<T> *>(ptr)));}

    template <class T>
    T cast(Type<T> t={}) const {
        return (idx == +t) ? unsafe_cast(t) : throw std::runtime_error("bad");
    }

    Reference reference() & {return {ptr, idx, fun, Qualifier::L};}
    Reference reference() const & {return {ptr, idx, fun, Qualifier::C};}
    Reference reference() && {return {ptr, idx, fun, Qualifier::R};}
};

static_assert(std::is_copy_constructible_v<Value>);
static_assert(std::is_move_constructible_v<Value>);
static_assert(24 == sizeof(Value));              // 8 + 24 buffer I think

template <class T>
std::optional<T> Reference::request(Type<T> t) const {
    auto &&v = request(typeid(T));
    if (auto p = v.target<T>()) return std::move(*p);
    else return {};
}

/******************************************************************************/

template <class T, class=void>
struct SimplifyValue {
    void operator()(Value &out, T const &t, std::type_index) const {
        if (Debug) std::cout << typeid(T).name() << std::endl;
        out = t;
        if (Debug) std::cout << typeid(T).name() << " 2" << std::endl;
    }
};

template <class T>
struct SimplifyValue<T, std::void_t<decltype(simplify(std::declval<T const &>(), std::declval<std::type_index &&>()))>> {
    void operator()(Value &out, T const &t, std::type_index idx) const {
        if (Debug) std::cout << typeid(T).name() << std::endl;
        out = simplify(t, std::move(idx));
        if (Debug) std::cout << typeid(T).name() << " 2" << std::endl;
    }
};

template <class T, class=void>
struct SimplifyReference {
    using custom = std::false_type;
    void * operator()(Qualifier q, T const &t, std::type_index i) const {
        if (Debug) std::cout << "no conversion " << typeid(T).name() << " "
            << int(static_cast<unsigned char>(q)) << " " << i.name() << std::endl;
        return nullptr;
    }
};

template <class T>
struct SimplifyReference<T, std::void_t<decltype(simplify(cvalue(), std::declval<T const &>(), std::declval<std::type_index &&>()))>> {
    using custom = std::true_type;
    template <class Q>
    void * operator()(Q q, qualified<T, Q> t, std::type_index idx) const {
        if (Debug) std::cout << "convert reference " << typeid(T).name() << std::endl;
        return const_cast<void *>(static_cast<void const *>(simplify(q, static_cast<decltype(t) &&>(t), std::move(idx))));
    }
};


template <class T, class=void>
struct Simplify : SimplifyValue<T>, SimplifyReference<T> {
    static_assert(std::is_same_v<no_qualifier<T>, T>);
};

/******************************************************************************/

template <class V>
void *call_reference(Qualifier dest, V &&v, std::type_index &&t) {
    using S = Simplify<no_qualifier<V>>;
    if (dest == Qualifier::R) {
        if constexpr(!std::is_invocable_v<S, rvalue &&, V &&, std::type_index &&>) return nullptr;
        else return static_cast<void *>(S()(rvalue(), static_cast<V &&>(v), std::move(t)));
    } else if (dest == Qualifier::L) {
        if constexpr(!std::is_invocable_v<S, lvalue &&, V &&, std::type_index &&>) return nullptr;
        else return static_cast<void *>(S()(lvalue(), static_cast<V &&>(v), std::move(t)));
    } else {
        if (Debug) std::cout << "call reference " << std::is_invocable_v<S, cvalue &&, V &&, std::type_index &&>
            << " " << typeid(S).name() << " " << typeid(V).name() << std::endl;
        if constexpr(!std::is_invocable_v<S, cvalue &&, V &&, std::type_index &&>) return nullptr;
        else return const_cast<void *>(static_cast<void const *>(S()(cvalue(), static_cast<V &&>(v), std::move(t))));
    }
}

template <class T>
struct Action {
    static_assert(std::is_same_v<no_qualifier<T>, T>);
    static void * apply(ActionType a, void *ptr, Info *out) {
        if (a == ActionType::destroy) delete static_cast<T *>(ptr);
        else if (a == ActionType::copy) return new T(*static_cast<T const *>(ptr));
        else if (a == ActionType::move) return new T(std::move(*static_cast<T *>(ptr)));
        else if (a == ActionType::make_value) {
            std::type_index t = out->idx;
            out->idx = typeid(void);
            Qualifier q{static_cast<unsigned char>(reinterpret_cast<std::uintptr_t>(out->fun))};
            out->fun = nullptr;
            Simplify<T>()(*static_cast<Value *>(out), *static_cast<T const *>(ptr), std::move(t));
        } else if (a == ActionType::to_reference) {
            if (Debug) std::cout << "convert to reference " << out->idx.name() << std::endl;
            Qualifier src{static_cast<unsigned char>(reinterpret_cast<std::uintptr_t>(out->ptr))};
            Qualifier dest{static_cast<unsigned char>(reinterpret_cast<std::uintptr_t>(out->fun))};
            if (src == Qualifier::R)
                return call_reference(dest, static_cast<T &&>(*static_cast<T *>(ptr)), std::move(out->idx));
            else if (src == Qualifier::L)
                return call_reference(dest, *static_cast<T *>(ptr), std::move(out->idx));
            else
                return call_reference(dest, *static_cast<T const *>(ptr), std::move(out->idx));
        }
        return nullptr;
    }
};

inline Value Reference::request(std::type_index const t) const {
    if (Debug) std::cout << (fun != nullptr) << " asking for " << t.name() << " from " << type().name() << std::endl;
    Value v;
    if (t == idx) {
        v.idx = t;
        v.fun = fun;
        v.ptr = fun(ActionType::copy, ptr, nullptr);
    } else {
        v.idx = t;
        v.fun = reinterpret_cast<ActionFunction>(static_cast<std::uintptr_t>(Qualifier::C));
        void * new_ptr = fun(ActionType::make_value, ptr, &v);
    }
    if (v.idx != t) v.reset();
    return v;
}

template <class T>
std::remove_reference_t<T> *Reference::to_reference(Type<T> t) const {
    if (idx == typeid(no_qualifier<T>))
        return std::is_const_v<std::remove_reference_t<T>>
            || (std::is_rvalue_reference_v<T> && qual == Qualifier::R)
            || (std::is_lvalue_reference_v<T> && qual == Qualifier::L) ?
            static_cast<std::remove_reference_t<T> *>(ptr) : nullptr;
    Info r;
    r.idx = t; // desired type
    r.ptr = reinterpret_cast<void *>(static_cast<std::uintptr_t>(qual)); // source qualifier
    r.fun = reinterpret_cast<ActionFunction>(static_cast<std::uintptr_t>(Qualifier::C)); // desired qualifier
    return static_cast<std::remove_reference_t<T> *>(fun(ActionType::to_reference, ptr, &r));
}

/******************************************************************************/

/// The 4 default behaviors for casting a reference to an expected type
template <class T, class=void>
struct FromReference {
    static_assert(std::is_same_v<no_qualifier<T>, T>);

    T operator()(Reference const &r, Dispatch &msg) const {
        auto v = r.request(typeid(T));
        if (auto p = v.target<T>()) return static_cast<T &&>(*p);
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
        if (Debug) std::cout << "trying temporary const & storage " << typeid(T const &).name() << std::endl;
        try {
            return FromReference<T &>()(r, msg);
        } catch (DispatchError const &) {
            return msg.storage.emplace_back().emplace<T>(FromReference<T>()(r, msg));
        }
    }
};

template <class T, class C>
struct FromReference<T &&, C> {
    T && operator()(Reference const &r, Dispatch &msg) const {
        if (Debug) std::cout << "trying temporary && storage " << typeid(T &&).name() << std::endl;
        return static_cast<T &&>(msg.storage.emplace_back().emplace<T>(FromReference<T>()(r, msg)));
    }
};


void from_reference(int, int, int);

/// ADL version
template <class T>
struct FromReference<T, std::void_t<decltype(
    from_reference(Type<T>(), std::declval<Reference const &>(), std::declval<Dispatch &>()))>> {

    T operator()(Reference const &r, Dispatch &msg) const {
        return static_cast<T>(from_reference(Type<T>(), r, msg));
    }
};

template <class T>
T downcast(Reference const &r, Dispatch &msg) {
    if (Debug) std::cout << "casting " << r.type().name() << " to " << typeid(T).name() << std::endl;
    if constexpr(std::is_same_v<T, void>) {
        // do nothing
    } else if constexpr(std::is_convertible_v<Value &&, T>) {
        return Value(r);

    } else if constexpr(!std::is_reference_v<T>) {
        if (Debug) std::cout << "not reference " << typeid(T).name() << std::endl;
        if (auto v = r.request(typeid(no_qualifier<T>))) {
            if (Debug) std::cout << "exact match " << v.name() << std::endl;
            return std::move(v).unsafe_cast<T>();
        }
        return FromReference<T>()(r, msg);
    } else {
        if (Debug) std::cout << "reference " << typeid(T).name() << std::endl;
        if (auto p = r.to_reference<T>()) return static_cast<T>(*p);
        if (Debug) std::cout << "reference2 " << typeid(FromReference<T>).name() << std::endl;
        return FromReference<T>()(r, msg);
    }
}

template <class T, std::enable_if_t<!std::is_reference_v<T>, int> = 0>
T downcast(Reference const &r) {
    Dispatch msg;
    return downcast<T>(r, msg);
}

/******************************************************************************/

}
