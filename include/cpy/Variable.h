/**
 * @brief C++ type-erased Variable object
 * @file Variable.h
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

namespace cpy {

/******************************************************************************/

template <class T, class=void>
struct Response; // converts T into any type

template <class T, class=void>
struct Request; // makes type T from any type

template <class T>
struct Action;

class Variable;

/******************************************************************************/

enum class ActionType : unsigned char {destroy, copy, move, response, assign};
using ActionFunction = void *(*)(ActionType, void *, Variable *);
// destroy: delete the ptr
// copy: return a new ptr holding a copy
// copy: return a new ptr holding a move
// response: assign the fields in the Variable pointer

/******************************************************************************/

template <class T>
struct SameType {using type=T;};

/******************************************************************************/

class Variable {

    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> *target(Type<T> t, Qualifier q) const {
        if (std::is_rvalue_reference_v<T>)
            return (idx == +t && q == Qualifier::R) ? static_cast<std::remove_reference_t<T> *>(ptr) : nullptr;
        else if (std::is_const_v<std::remove_reference_t<T>>)
            return (idx == +t) ? static_cast<std::remove_reference_t<T> *>(ptr) : nullptr;
        else // Qualifier assumed not to be V
            return (idx == +t && q == Qualifier::L) ? static_cast<std::remove_reference_t<T> *>(ptr) : nullptr;
    }

public:
    void *ptr = nullptr;
    std::type_index idx = typeid(void);
    ActionFunction fun = nullptr;
    Qualifier qual;

    Variable &set(void *p, std::type_index t, ActionFunction f, Qualifier q) {
        ptr = p; idx = t; fun = f; qual = q;
        return *this;
    }

    void reset() {
        if (valued()) fun(ActionType::destroy, ptr, nullptr);
        set(nullptr, typeid(void), nullptr, Qualifier());
    }

    Variable(void *p, std::type_index t, ActionFunction f, Qualifier q)
        : ptr(p), idx(t), fun(f), qual(q) {}


    bool valued() const {return ptr && qual == Qualifier::V;}

    Variable() = default;

    // template <class V, std::enable_if_t<std::is_same_v<no_qualifier<V>, Variable>, int> = 0>
    // explicit Variable(V &&v) : Info{static_cast<V &&>(v)}, qual(qualifier_of<V &&>) {}

    template <class T, std::enable_if_t<!(std::is_same_v<std::decay_t<T>, T>), int> = 0>
    Variable(Type<T>, typename SameType<T>::type t) : Variable(
        const_cast<void *>(static_cast<void const *>(std::addressof(t))), typeid(T), Action<std::decay_t<T>>::apply, qualifier_of<T>) {}

    template <class T, class ...Ts, std::enable_if_t<(std::is_same_v<std::decay_t<T>, T>), int> = 0>
    Variable(Type<T>, Ts &&...ts) : Variable(new T(static_cast<Ts &&>(ts)...), typeid(T), Action<T>::apply, Qualifier::V) {
            static_assert(std::is_same_v<no_qualifier<T>, T>);
            static_assert(!std::is_same_v<no_qualifier<T>, Variable>);
            static_assert(!std::is_same_v<no_qualifier<T>, void *>);
        }

    template <class T, std::enable_if_t<!std::is_base_of_v<Variable, no_qualifier<T>>, int> = 0>
    Variable(T &&t) : Variable(Type<std::decay_t<T>>(), static_cast<T &&>(t)) {
        static_assert(!std::is_same_v<no_qualifier<T>, Variable>);
        static_assert(!std::is_same_v<no_qualifier<T>, void *>);
    }

    /// Take variables and reset the old ones
    Variable(Variable &&v) noexcept : Variable(std::exchange(v.ptr, nullptr), std::exchange(v.idx, typeid(void)),
                                               v.fun, std::exchange(v.qual, Qualifier::V)) {}

    /// Only call variable copy constructor if bool() and not reference
    Variable(Variable const &v) : Variable(v.valued() ? v.fun(ActionType::copy, v.ptr, nullptr) : v.ptr, v.idx, v.fun, v.qual) {}

    Variable & operator=(Variable &&v) noexcept {
        if (valued()) fun(ActionType::destroy, ptr, nullptr);
        return set(std::exchange(v.ptr, nullptr), std::exchange(v.idx, typeid(void)), v.fun, std::exchange(v.qual, Qualifier::V));
    }

    Variable & operator=(Variable const &v) {
        if (valued()) fun(ActionType::destroy, ptr, nullptr);
        return set(v.valued() ? v.fun(ActionType::copy, v.ptr, nullptr) : v.ptr, v.idx, v.fun, v.qual);
    }

    ~Variable() {if (valued()) fun(ActionType::destroy, ptr, nullptr);}

    void assign(Variable v) {
        if (!ptr || qual == Qualifier::V) {
            if (v.qual == Qualifier::V) {
                *this = std::move(v);
            } else {
                // for now, just make a fresh value copy of the variable
                auto act = v.qual == Qualifier::R ? ActionType::move : ActionType::copy;
                set(v.ptr ? v.fun(act, v.ptr, nullptr) : v.ptr,  v.idx, v.fun, qual);
            }
        } else if (qual == Qualifier::C) {
            throw std::invalid_argument("Cannot assign to const Variable");
        } else {
            if (!fun(ActionType::assign, ptr, &v))
                throw std::invalid_argument("Could not coerce Variable to matching type");
        }
    }

    explicit constexpr operator bool() const {return ptr;}
    constexpr bool has_value() const {return ptr;}
    auto name() const {return idx.name();}
    std::type_index type() const {return idx;}
    Qualifier qualifier() const {return qual;}

    /**************************************************************************/

    // request any type T by non-custom conversions
    Variable request_variable(Dispatch &msg, std::type_index const, Qualifier q=Qualifier::V) const;

    // request reference T by custom conversions
    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> *request(Dispatch &msg, bool custom=false, Type<T> t={}) const;

    template <class T, std::enable_if_t<!std::is_reference_v<T>, int> = 0>
    std::optional<T> request(Dispatch &msg, bool custom=false, Type<T> t={}) const;

    /**************************************************************************/

    void downcast(Dispatch &msg, bool custom=true, Type<void> t={}) const {}
    void downcast(bool custom=true, Type<void> t={}) const {}

    template <class T>
    T downcast(Dispatch &msg, bool custom=true, Type<T> t={}) const {
        if (auto p = request(msg, custom, t)) return static_cast<T>(*p);
        throw msg.exception();
    }

    // request non-reference T by custom conversions
    template <class T>
    std::optional<T> request(bool custom=false, Type<T> t={}) const {Dispatch msg; return request(msg, custom, t);}

    // request non-reference T by custom conversions
    template <class T>
    T downcast(bool custom=true, Type<T> t={}) const {
        Dispatch msg;
        if (auto p = request(msg, custom, t))
            return msg.storage.empty() ? static_cast<T>(*p) : throw std::runtime_error("contains temporaries");
        return downcast(msg, custom, t);
    }

    // return pointer to target if it is trivially convertible to requested type
    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> *target(Type<T> t={}) const & {
        return target(t, (qual == Qualifier::V) ? Qualifier::L : qual);
    }

    Variable reference() & {return {ptr, idx, fun, qual == Qualifier::V ? Qualifier::L : qual};}
    Variable reference() const & {return {ptr, idx, fun, qual == Qualifier::V ? Qualifier::C : qual};}
    Variable reference() && {return {ptr, idx, fun, qual == Qualifier::V ? Qualifier::R : qual};}
};

/******************************************************************************/

template <class T>
void qualified_response(Variable &out, Qualifier dest, T &&t, std::type_index idx) {
    using S = Response<no_qualifier<T>>;
    if (dest == Qualifier::V)
        if constexpr(std::is_invocable_v<S, Variable &, T const &, std::type_index &&>)
            S()(out, static_cast<T &&>(t), std::move(idx));

    if (dest == Qualifier::R)
        if constexpr(std::is_invocable_v<S, Variable &, T &&, std::type_index &&, rvalue &&>)
            S()(out, static_cast<T &&>(t), std::move(idx), rvalue());

    if (dest == Qualifier::L)
        if constexpr(std::is_invocable_v<S, Variable &, T &&, std::type_index &&, lvalue &&>)
            S()(out, static_cast<T &&>(t), std::move(idx), lvalue());

    if (dest == Qualifier::C)
        if constexpr(std::is_invocable_v<S, Variable &, T &&, std::type_index &&, cvalue &&>)
            S()(out, static_cast<T &&>(t), std::move(idx), cvalue());
}

template <class T>
struct Action {
    static_assert(std::is_same_v<no_qualifier<T>, T>);
    static void * apply(ActionType a, void *ptr, Variable *out) {
        // Delete the object
        if (a == ActionType::destroy) delete static_cast<T *>(ptr);
        // Copy the object
        else if (a == ActionType::copy) return new T(*static_cast<T const *>(ptr));
        // Move the object
        else if (a == ActionType::move) return new T(std::move(*static_cast<T *>(ptr)));
        // Respond to a given type_index
        else if (a == ActionType::response) {
            // Get the requested type
            std::type_index t = out->idx;
            // Get the requested qualifier
            Qualifier const dest = out->qual;
            // Get the qualifier of the source variable
            Qualifier const src{static_cast<unsigned char>(reinterpret_cast<std::uintptr_t>(out->ptr))};
            // Get the Dispatch
            Dispatch &msg = *reinterpret_cast<Dispatch *>(out->fun);
            out->set(nullptr, typeid(void), nullptr, Qualifier());
            // Set the output Variable with the result
            // if (Debug) std::cout << "response " << static_cast<int>(src) << static_cast<int>(dest) << " " << typeid(Response<T>).name() << std::endl;

            if (src == Qualifier::C) {
                // if (Debug) std::cout << "const source" << std::endl;
                qualified_response(*out, dest, *static_cast<T const *>(ptr), std::move(t));
            }
            else if (src == Qualifier::L)
                qualified_response(*out, dest, *static_cast<T *>(ptr), std::move(t));
            else if (src == Qualifier::R)
                qualified_response(*out, dest, static_cast<T &&>(*static_cast<T *>(ptr)), std::move(t));

            if (!out->ptr) {
                msg.error("Did not respond with anything");
            } else if (out->idx != t)  {
                msg.error("Did not respond with correct type");
                out->reset();
            } else if (out->idx != t) {
                msg.error("Did not respond with correct qualifier");
                out->reset();
            }

        } else if (a == ActionType::assign) {
            if (Debug) std::cout << "    - assign " << out->idx.name() << std::endl;
            if constexpr(std::is_move_assignable_v<T>) {
                if (auto p = std::move(*out).request<T>()) {
                    *static_cast<T *>(ptr) = std::move(*p);
                    return ptr;
                }
            }
        }
        return nullptr;
    }
};

/******************************************************************************/

inline Variable Variable::request_variable(Dispatch &msg, std::type_index const t, Qualifier q) const {
    if (Debug) std::cout << "        - " << (fun != nullptr) << " asking for " << t.name() << std::endl;
    Variable v;
    if (t == type()) { // Exact type match
        if (q == Qualifier::V) { // Make a copy or move
            auto act = (qual == Qualifier::R) ? ActionType::move : ActionType::copy;
            v.set(fun(act, ptr, nullptr), t, fun, Qualifier::V);
        } else if (q == Qualifier::C || q == qual) {
            v.set(ptr, t, fun, q);
        } else {
            msg.error("Source and target qualifiers are not compatible");
        }
    } else {
        auto src = (qual == Qualifier::V) ? Qualifier::C : qual;
        v.set(reinterpret_cast<void *>(static_cast<std::uintptr_t>(src)),
              t, reinterpret_cast<ActionFunction>(&msg), q);
        (void) fun(ActionType::response, ptr, &v);
    }
    return v;
}

template <class T, std::enable_if_t<!std::is_reference_v<T>, int>>
std::optional<T> Variable::request(Dispatch &msg, bool custom, Type<T> t) const {
    // if (Debug) std::cout << "ask for " << typeid(T).name() << " from " << name() << std::endl;
    if (auto p = target<T const &>()) return *p;
    // if (Debug) std::cout << "ask2 for " << typeid(T).name() << std::endl;
    auto v = request_variable(msg, typeid(T));
    // if (Debug) std::cout << "ask3 for " << typeid(T).name() << " " << v.name() << std::endl;
    if (auto p = v.target<T const &>()) {
        // if (Debug) std::cout << "got " << typeid(T).name() << std::endl;
        return *p;
    } else if (true || custom) {
        // if (Debug) std::cout << "custom Request " << type().name() << " -> " << typeid(T).name() << std::endl;
        return Request<T>()(*this, msg);
    } else {
        return {};
    }
}

template <class T, std::enable_if_t<std::is_reference_v<T>, int>>
std::remove_reference_t<T> *Variable::request(Dispatch &msg, bool custom, Type<T> t) const {
    // if (Debug) std::cout << "ask2 for ref " << typeid(T).name() << std::endl;
    if (idx == typeid(no_qualifier<T>)) return target<T>();
    auto v = request_variable(msg, typeid(T), qualifier_of<T>);
    // if (Debug) std::cout << "ask3 for " << typeid(T).name() << " " << v.name() << std::endl;
    if (auto p = v.template target<T>()) {
        // if (Debug) std::cout << "got " << typeid(T).name() << std::endl;
        return p;
    } else if (custom) {
        // if (Debug) std::cout << "custom Request2 " << type().name() << " -> " << typeid(T).name() << std::endl;
        return Request<T>()(*this, msg);
        // return nullptr;
    } else {
        return nullptr;
    }
}

/******************************************************************************/

}
