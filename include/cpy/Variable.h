/**
 * @brief C++ type-erased Variable object
 * @file Variable.h
 */

#pragma once
#include "Storage.h"
#include "Signature.h"

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

/******************************************************************************/

class Variable : protected VariableData {
    Variable(void *p, std::type_info const *idx, ActionFunction act, Qualifier qual, bool s) noexcept
        : VariableData(p ? idx : nullptr, p ? act : nullptr, p ? qual : Qualifier::V, p && s)
        {if (p) reinterpret_cast<void *&>(buff) = p;}

    Variable(Variable const &v, bool move) : VariableData(v) {
        if (v.has_value())
            act(move ? ActionType::move : ActionType::copy, v.pointer(), this);
        qual = Qualifier::V;
    }

public:

    /**************************************************************************/

    constexpr Variable() noexcept = default;

    /// Reference type
    template <class T, std::enable_if_t<!(std::is_same_v<std::decay_t<T>, T>), int> = 0>
    Variable(Type<T>, typename SameType<T>::type t) noexcept
        : VariableData(&typeid(T), Action<std::decay_t<T>>::apply, qualifier_of<T>, UseStack<no_qualifier<T>>::value) {
            reinterpret_cast<std::remove_reference_t<T> *&>(buff) = std::addressof(t);
        }

    /// Non-Reference type
    template <class T, class ...Ts, std::enable_if_t<(std::is_same_v<std::decay_t<T>, T>), int> = 0>
    Variable(Type<T>, Ts &&...ts) : VariableData(&typeid(T), Action<T>::apply, Qualifier::V, UseStack<T>::value) {
        static_assert(!std::is_same_v<no_qualifier<T>, Variable>);
        if constexpr(UseStack<T>::value) ::new (&buff) T{static_cast<Ts &&>(ts)...};
        else reinterpret_cast<void *&>(buff) = ::new T{static_cast<Ts &&>(ts)...};
    }

    template <class T, std::enable_if_t<!std::is_base_of_v<VariableData, no_qualifier<T>>, int> = 0>
    Variable(T &&t) : Variable(Type<std::decay_t<T>>(), static_cast<T &&>(t)) {
        static_assert(!std::is_same_v<no_qualifier<T>, Variable>);
    }

    /// Take variables and reset the old ones
    // If RHS is Reference, RHS is left unchanged
    // If RHS is Value and held in stack, RHS is moved from
    // If RHS is Value and not held in stack, RHS is reset
    Variable(Variable &&v) noexcept : VariableData(static_cast<VariableData const &>(v)) {
        if (auto p = v.handle()) {
            if (stack) act(ActionType::move, p, this);
            else v.reset_data();
        }
    }

    /// Only call variable copy constructor if its lifetime is being managed
    Variable(Variable const &v) : VariableData(static_cast<VariableData const &>(v)) {
        DUMP(v.handle(), &buff, stack);
        if (auto p = v.handle()) act(ActionType::copy, p, this);
    }

    /// Only call variable move constructor if its lifetime is being managed inside the buffer
    Variable & operator=(Variable &&v) noexcept {
        DUMP("move assign", qualifier(), name(), v.qualifier(), v.name());
        if (auto p = handle()) act(ActionType::destroy, p, nullptr);
        static_cast<VariableData &>(*this) = v;
        if (auto p = v.handle()) {
            if (stack) act(ActionType::move, p, this);
            else v.reset_data();
        }
        return *this;
    }

    Variable & operator=(Variable const &v) {
        DUMP("copy assign", qualifier(), name(), v.qualifier(), v.name());
        if (auto p = handle()) act(ActionType::destroy, p, nullptr);
        static_cast<VariableData &>(*this) = v;
        if (auto p = v.handle()) v.act(ActionType::copy, p, this);
        return *this;
    }

    ~Variable() {
        DUMP("destroy", qualifier(), name());
        if (auto p = handle()) act(ActionType::destroy, p, nullptr);
        DUMP("destroyed", qualifier(), name());
    }

    /**************************************************************************/

    void reset() {
        DUMP("reset", qualifier(), name());

        if (auto p = handle()) act(ActionType::destroy, p, nullptr);
        reset_data();
    }

    void assign(Variable v);

    void const * data() const {return pointer();}

    char const * name() const {return idx ? idx->name() : typeid(void).name();}
    std::type_index type() const {return idx ? *idx : typeid(void);}
    std::type_info const & info() const {return idx ? *idx : typeid(void);}
    Qualifier qualifier() const {return qual;}
    ActionFunction action() const {return act;}
    bool is_stack_type() const {return stack;}

    constexpr bool has_value() const {return act;}
    explicit constexpr operator bool() const {return act;}

    /**************************************************************************/

    Variable copy() && {return {*this, qual == Qualifier::V || qual == Qualifier::R};}
    Variable copy() const & {return {*this, qual == Qualifier::R};}
    Variable reference() & {return {pointer(), idx, act, qual == Qualifier::V ? Qualifier::L : qual, stack};}
    Variable reference() const & {return {pointer(), idx, act, qual == Qualifier::V ? Qualifier::C : qual, stack};}
    Variable reference() && {return {pointer(), idx, act, qual == Qualifier::V ? Qualifier::R : qual, stack};}

    /**************************************************************************/

    // request any type T by non-custom conversions
    Variable request_variable(Dispatch &msg, std::type_index const, Qualifier q=Qualifier::V) const;

    // request reference T by custom conversions
    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> *request(Dispatch &msg, Type<T> t={}) const {
        DUMP(typeid(Type<T>).name(), qualifier(), has_value(), idx->name());
        if (!has_value()) return nullptr;
        if (*idx == typeid(no_qualifier<T>)) return target<T>();
        auto v = request_variable(msg, typeid(T), qualifier_of<T>);
        if (auto p = v.template target<T>()) return p;
        else return Request<T>()(*this, msg);
    }

    template <class T, std::enable_if_t<!std::is_reference_v<T>, int> = 0>
    std::optional<T> request(Dispatch &msg, Type<T> t={}) const {
        static_assert(std::is_same_v<T, no_qualifier<T>>);
        if (!has_value()) return {};
        if (auto p = target<T const &>()) {
            DUMP(qual, p, &buff, reinterpret_cast<void * const &>(buff), stack, typeid(p).name(), typeid(Type<T>).name());
            return *p;
        }
        auto v = request_variable(msg, typeid(T));
        if (auto p = v.target<T const &>()) return *p;
        else return Request<T>()(*this, msg);
    }

    /**************************************************************************/

    void downcast(Dispatch &msg, Type<void> t={}) const {}
    void downcast(Type<void> t={}) const {}

    template <class T>
    T downcast(Dispatch &msg, Type<T> t={}) const {
        if (auto p = request(msg, t)) return static_cast<T>(*p);
        throw msg.exception();
    }

    // request non-reference T by custom conversions
    template <class T>
    std::optional<T> request(Type<T> t={}) const {Dispatch msg; return request(msg, t);}

    // request non-reference T by custom conversions
    template <class T>
    T downcast(Type<T> t={}) const {
        Dispatch msg;
        if (auto p = request(msg, t))
            return msg.storage.empty() ? static_cast<T>(*p) : throw std::runtime_error("contains temporaries");
        return downcast(msg, t);
    }

    // return pointer to target if it is trivially convertible to requested type
    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> *target(Type<T> t={}) const & {
        DUMP(name(), typeid(Type<T>).name(), qual, stack);
        return target_pointer(t, (qual == Qualifier::V) ? Qualifier::C : qual);
    }

    // return pointer to target if it is trivially convertible to requested type
    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> *target(Type<T> t={}) & {
        DUMP(name(), typeid(Type<T>).name(), qual, stack);
        return target_pointer(t, (qual == Qualifier::V) ? Qualifier::L : qual);
    }
};

/******************************************************************************/

/// Set out to a new variable with given qualifier dest and type idx from type T
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

    static void response(Variable &v, void *p, RequestData r) {
        if (r.source == Qualifier::C)
            qualified_response(v, r.dest, *static_cast<T const *>(p), std::move(r.type));
        else if (r.source == Qualifier::L)
            qualified_response(v, r.dest, *static_cast<T *>(p), std::move(r.type));
        else if (r.source == Qualifier::R)
            qualified_response(v, r.dest, static_cast<T &&>(*static_cast<T *>(p)), std::move(r.type));
    }

    static void apply(ActionType a, void *p, VariableData *v) {
        DUMP(int(a), typeid(T).name());
        if (a == ActionType::destroy) { // Delete the object (known to be non-reference)
            if constexpr(UseStack<T>::value) static_cast<T *>(p)->~T();
            else delete static_cast<T *>(p);

        } else if (a == ActionType::copy) { // Copy-Construct the object
            DUMP(v->stack, UseStack<T>::value);
            if constexpr(UseStack<T>::value) ::new(static_cast<void *>(&v->buff)) T{*static_cast<T const *>(p)};
            else reinterpret_cast<void *&>(v->buff) = ::new T{*static_cast<T const *>(p)};

        } else if (a == ActionType::move) { // Move-Construct the object (known to be on stack)
            DUMP(v->stack, UseStack<T>::value);
            ::new(static_cast<void *>(&v->buff)) T{std::move(*static_cast<T *>(p))};

        } else if (a == ActionType::response) { // Respond to a given type_index
            response(reinterpret_cast<Variable &>(*v), p, reinterpret_cast<RequestData &&>(std::move(v->buff)));

        } else if (a == ActionType::assign) { // Assign from another variable
            DUMP("assign", v->idx->name(), typeid(T).name(), v->qual);
            if constexpr(std::is_move_assignable_v<T>) {
                if (auto r = reinterpret_cast<Variable &&>(*v).request<T>())
                    *static_cast<T *>(p) = std::move(*r);
            }
        }
    }
};

/******************************************************************************/

}
