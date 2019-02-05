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
        if (auto p = handle()) act(ActionType::destroy, p, nullptr);
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
        if (*idx == typeid(no_qualifier<T>)) return target<T>();
        auto v = request_variable(msg, typeid(T), qualifier_of<T>);
        if (auto p = v.template target<T>()) {msg.source.reset(); return p;}
        if (auto p = Request<T>()(*this, msg)) {msg.source.reset(); return p;}
        return nullptr;
    }

    template <class T, std::enable_if_t<!std::is_reference_v<T>, int> = 0>
    std::optional<T> request(Dispatch &msg, Type<T> t={}) const {
        static_assert(std::is_same_v<T, no_qualifier<T>>);
        if constexpr(std::is_same_v<T, Variable>) return *this;
        if (auto p = target<T const &>()) {
            DUMP(qual, p, &buff, reinterpret_cast<void * const &>(buff), stack, typeid(p).name(), typeid(Type<T>).name());
            return *p;
        }
        auto v = request_variable(msg, typeid(T));
        if (auto p = std::move(v).target<T &&>()) {msg.source.reset(); return std::move(*p);};
        if (auto p = Request<T>()(*this, msg)) {msg.source.reset(); return std::move(p);}
        return {};
    }

    /**************************************************************************/

    void cast(Dispatch &msg, Type<void> t={}) const {}
    void cast(Type<void> t={}) const {}

    template <class T>
    T cast(Dispatch &msg, Type<T> t={}) const {
        if (auto p = request(msg, t)) return static_cast<T>(*p);
        throw std::move(msg).exception();
    }

    // request non-reference T by custom conversions
    template <class T>
    std::optional<T> request(Type<T> t={}) const {Dispatch msg; return request(msg, t);}

    // request non-reference T by custom conversions
    template <class T>
    T cast(Type<T> t={}) const {
        Dispatch msg;
        if (auto p = request(msg, t))
            return msg.storage.empty() ? static_cast<T>(*p) : throw std::runtime_error("contains temporaries");
        return cast(msg, t);
    }

    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> *target(Type<T> t={}) && {
        DUMP(name(), typeid(Type<T>).name(), qual, stack);
        return target_pointer(t, (qual == Qualifier::V) ? Qualifier::R : qual);
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

void set_source(Dispatch &, std::type_info const &, Variable &&v);

/// Set out to a new variable with given qualifier dest and type idx from type T
template <class T>
bool qualified_response(Variable &out, Qualifier dest, T &&t, std::type_index idx) {
    using R = Response<no_qualifier<T>>;
    if (dest == Qualifier::V)
        if constexpr(std::is_invocable_v<R, Variable &, T const &, std::type_index &&>)
            return R()(out, static_cast<T &&>(t), std::move(idx)); // should be able to set the source

    if (dest == Qualifier::R)
        if constexpr(std::is_invocable_v<R, Variable &, T &&, std::type_index &&, rvalue &&>)
            return R()(out, static_cast<T &&>(t), std::move(idx), rvalue());

    if (dest == Qualifier::L)
        if constexpr(std::is_invocable_v<R, Variable &, T &&, std::type_index &&, lvalue &&>)
            return R()(out, static_cast<T &&>(t), std::move(idx), lvalue());

    if (dest == Qualifier::C)
        if constexpr(std::is_invocable_v<R, Variable &, T &&, std::type_index &&, cvalue &&>)
            return R()(out, static_cast<T &&>(t), std::move(idx), cvalue());
    return false;
}

template <class T>
struct Action {
    static_assert(std::is_same_v<no_qualifier<T>, T>);

    static void response(Variable &v, void *p, RequestData &&r) {
        bool ok = false;
        if (r.source == Qualifier::C)
            ok = qualified_response(v, r.dest, *static_cast<T const *>(p), std::move(r.type));
        if (r.source == Qualifier::L)
            ok = qualified_response(v, r.dest, *static_cast<T *>(p), std::move(r.type));
        if (r.source == Qualifier::R)
            ok = qualified_response(v, r.dest, static_cast<T &&>(*static_cast<T *>(p)), std::move(r.type));
        DUMP("tried response", r.source, ok);
        if (!ok) set_source(*r.msg, typeid(T), std::move(v));
    }

    static void apply(ActionType a, void *p, VariableData *v) {
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
                if (auto r = reinterpret_cast<Variable &&>(*v).request<T>()) {

                    DUMP("got the assignable", v->idx->name(), typeid(T).name(), v->qual, typeid(T).name());

                    if constexpr(std::is_same_v<T, std::vector< std::pair<std::vector<std::string>, double> >>) {
                        DUMP(r->size());
                    }

                    *static_cast<T *>(p) = std::move(*r);
                    reinterpret_cast<Variable &>(*v).reset(); // signifies that assignment took place

                    if constexpr(std::is_same_v<T, std::vector< std::pair<std::vector<std::string>, double> >>) {
                        DUMP(static_cast<T *>(p)->size());
                        DUMP(r->size());
                    }
                }
            }
        }
    }
};

/******************************************************************************/

}
