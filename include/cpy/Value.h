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

/******************************************************************************/

template <class T, class=void>
struct Simplify; // converts T into any type

template <class T, class=void>
struct Request; // makes type T from any type

template <class T>
struct Action;

struct Info;
class Value;
class Reference;

/******************************************************************************/

enum class ActionType : unsigned char {destroy, copy, move, make_value, make_reference};

using ActionFunction = void *(*)(ActionType, void *, Info *);

/******************************************************************************/

struct Info {
    void *ptr = nullptr;
    std::type_index idx = typeid(void);
    ActionFunction fun = nullptr;

    explicit constexpr operator bool() const {return ptr;}
    constexpr bool has_value() const {return ptr;}
    auto name() const {return idx.name();}

    std::type_index type() const {return idx;}
};

/******************************************************************************/

class Reference : public Info {
    Qualifier qual;
public:
    Reference() = default;

    Reference(void *p, std::type_index t, ActionFunction f, Qualifier q) : Info{p, t, f}, qual(q) {}

    template <class V, std::enable_if_t<std::is_same_v<no_qualifier<V>, Value>, int> = 0>
    explicit Reference(V &&v) : Info{static_cast<V &&>(v)}, qual(::cpy::qualifier<V &&>) {}

    template <class T, std::enable_if_t<!std::is_base_of_v<Info, no_qualifier<T>>, int> = 0>
    explicit Reference(T &&t) : Info{const_cast<no_qualifier<T> *>(static_cast<no_qualifier<T> const *>(std::addressof(t))),
                                     typeid(no_qualifier<T>), Action<no_qualifier<T>>::apply}, qual(::cpy::qualifier<T &&>) {}

    Qualifier qualifier() const {return qual;}

    // request non-reference T by custom conversions
    Value request(std::type_index const) const;

    // request non-reference T by custom conversions
    template <class T>
    std::optional<T> request(Type<T> t={}) const;

    // request reference T by custom conversions
    template <class T>
    std::remove_reference_t<T> *request_reference(Type<T> t={}) const;

    // return pointer to target if it is trivially convertible to requested type
    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> *target(Type<T> t={}) const {
        if (std::is_rvalue_reference_v<T>)
            return (idx == +t && qual == Qualifier::R) ? static_cast<std::remove_reference_t<T> *>(ptr) : nullptr;
        else if (std::is_const_v<std::remove_reference_t<T>>)
            return (idx == +t) ? static_cast<std::remove_reference_t<T> *>(ptr) : nullptr;
        else
            return (idx == +t && qual == Qualifier::L) ? static_cast<std::remove_reference_t<T> *>(ptr) : nullptr;
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
    Value(T &&t) : Value(Type<std::decay_t<T>>(), static_cast<T &&>(t)) {
        static_assert(!std::is_same_v<no_qualifier<T>, Reference>);
        static_assert(!std::is_same_v<no_qualifier<T>, Value>);
        static_assert(!std::is_same_v<no_qualifier<T>, void *>);
    }

    explicit Value(Info const &other) : Info(other) {ptr = fun(ActionType::copy, ptr, nullptr);}

    Value(Value &&v) noexcept : Info{std::exchange(v.ptr, nullptr), std::exchange(v.idx, typeid(void)), v.fun} {}

    Value(Value const &v) : Info{v.ptr ? v.fun(ActionType::copy, v.ptr, nullptr) : nullptr, v.idx, v.fun} {}

    Value & operator=(Value &&v) noexcept {
        if (ptr) fun(ActionType::destroy, ptr, nullptr);
        static_cast<Info &>(*this) = {std::exchange(v.ptr, nullptr), std::exchange(v.idx, typeid(void)), v.fun};
        return *this;
    }

    Value & operator=(Value const &v) {
        if (ptr) fun(ActionType::destroy, ptr, nullptr);
        static_cast<Info &>(*this) = {v.ptr ? v.fun(ActionType::copy, v.ptr, nullptr) : nullptr, v.idx, v.fun};
        return *this;
    }

    void reset() {
        if (!ptr) return;
        fun(ActionType::destroy, ptr, nullptr);
        static_cast<Info &>(*this) = {nullptr, idx, nullptr};
    }

    ~Value() {if (ptr) fun(ActionType::destroy, ptr, nullptr);}

    template <class T>
    T *target() {return idx == typeid(T) ? static_cast<T *>(ptr) : nullptr;}

    template <class T>
    T const *target() const {return idx == typeid(T) ? static_cast<T const *>(ptr) : nullptr;}
};

static_assert(std::is_copy_constructible_v<Value>);
static_assert(std::is_move_constructible_v<Value>);

/******************************************************************************/

template <class V>
void *simplify_to_reference(Qualifier dest, V &&v, std::type_index &&t) {
    using S = Simplify<no_qualifier<V>>;
    if (dest == Qualifier::R) {
        if constexpr(!std::is_invocable_v<S, rvalue &&, V &&, std::type_index &&>) return nullptr;
        else return static_cast<void *>(S()(rvalue(), static_cast<V &&>(v), std::move(t)));
    } else if (dest == Qualifier::L) {
        if constexpr(!std::is_invocable_v<S, lvalue &&, V &&, std::type_index &&>) return nullptr;
        else return static_cast<void *>(S()(lvalue(), static_cast<V &&>(v), std::move(t)));
    } else {
        if (Debug) std::cout << "    - call reference " << std::is_invocable_v<S, cvalue &&, V &&, std::type_index &&>
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
        } else if (a == ActionType::make_reference) {
            if (Debug) std::cout << "    - convert to reference " << out->idx.name() << std::endl;
            Qualifier src{static_cast<unsigned char>(reinterpret_cast<std::uintptr_t>(out->ptr))};
            Qualifier dest{static_cast<unsigned char>(reinterpret_cast<std::uintptr_t>(out->fun))};
            if (src == Qualifier::R)
                return simplify_to_reference(dest, static_cast<T &&>(*static_cast<T *>(ptr)), std::move(out->idx));
            else if (src == Qualifier::L)
                return simplify_to_reference(dest, *static_cast<T *>(ptr), std::move(out->idx));
            else
                return simplify_to_reference(dest, *static_cast<T const *>(ptr), std::move(out->idx));
        }
        return nullptr;
    }
};

/******************************************************************************/

inline Value Reference::request(std::type_index const t) const {
    if (Debug) std::cout << "        - " << (fun != nullptr) << " asking for " << t.name() << " from " << type().name() << std::endl;
    Value v;
    if (t == idx) static_cast<Info &>(v) = {fun(ActionType::copy, ptr, nullptr), t, fun};
    else {
        v.idx = t;
        v.fun = reinterpret_cast<ActionFunction>(static_cast<std::uintptr_t>(Qualifier::C));
        void * new_ptr = fun(ActionType::make_value, ptr, &v);
    }
    if (v.idx != t) v.reset();
    return v;
}

template <class T>
std::optional<T> Reference::request(Type<T> t) const {
    std::optional<T> out;
    auto &&v = request(typeid(T));
    if (auto p = v.target<T>()) out.emplace(std::move(*p));
    return out;
}

template <class T>
std::remove_reference_t<T> *Reference::request_reference(Type<T> t) const {
    if (idx == typeid(no_qualifier<T>))
        return std::is_const_v<std::remove_reference_t<T>>
            || (std::is_rvalue_reference_v<T> && qual == Qualifier::R)
            || (std::is_lvalue_reference_v<T> && qual == Qualifier::L) ?
            static_cast<std::remove_reference_t<T> *>(ptr) : nullptr;
    // Make Info with source qualifier, desired type, desired qualifier
    Info r{reinterpret_cast<void *>(static_cast<std::uintptr_t>(qual)), t,
        reinterpret_cast<ActionFunction>(static_cast<std::uintptr_t>(Qualifier::C))};
    return static_cast<std::remove_reference_t<T> *>(fun(ActionType::make_reference, ptr, &r));
}

/******************************************************************************/

// downcast a reference into a type T using
template <class T>
T downcast(Reference const &r, Dispatch &msg) {
    if (Debug) std::cout << "    - casting " << r.type().name() << " to " << typeid(T).name() << std::endl;
    if constexpr(std::is_same_v<T, void>) {
        // do nothing
    } else if constexpr(std::is_convertible_v<Value &&, T>) {
        return Value(r);
    } else if constexpr(!std::is_reference_v<T>) {
        if (Debug) std::cout << "    - not reference " << typeid(T).name() << std::endl;
        if (auto v = r.request<no_qualifier<T>>()) {
            if (Debug) std::cout << "    - exact match " << std::endl;
            return std::move(*v);
        }
        return Request<T>()(r, msg);
    } else {
        if (Debug) std::cout << "    - reference " << typeid(T).name() << std::endl;
        if (auto p = r.request_reference<T>()) return static_cast<T>(*p);
        return Request<T>()(r, msg);
    }
}

template <class T>
T downcast(Reference const &r) {
    if constexpr(!std::is_same_v<T, void>) {
        Dispatch msg;
        T out = downcast<T>(r, msg);
        return (msg.storage.empty()) ? out : throw std::runtime_error("contains temporaries");
    }
}

/******************************************************************************/

}
