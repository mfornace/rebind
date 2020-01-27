#pragma once
#include "Storage.h"

namespace rebind {

/******************************************************************************/

class BaseVariable : protected VariableData {
    BaseVariable(void *p, TypeIndex idx, ActionFunction act, bool s) noexcept
        : VariableData(p ? idx : TypeIndex(), p ? act : nullptr, p && s)
        {if (p) reinterpret_cast<void *&>(buff) = p;}

    BaseVariable(BaseVariable const &v, bool move) : VariableData(v) {
        if (v.has_value())
            act(move ? ActionType::move : ActionType::copy, v.pointer(), this);
        idx.set_qualifier(Value);
    }

public:

    Qualifier qualifier() const {return idx.qualifier();}
    void const * data() const {return pointer();}

    TypeIndex type() const {return idx;}
    ActionFunction action() const {return act;}
    bool is_stack_type() const {return stack;}

    /**************************************************************************/

    using VariableData::VariableData;
    constexpr BaseVariable() noexcept = default;

    template <class T, std::enable_if_t<!std::is_base_of_v<VariableData, unqualified<T>>, int> = 0>
    BaseVariable(T &&t) : BaseVariable(Type<std::decay_t<T>>(), static_cast<T &&>(t)) {
        static_assert(!std::is_same_v<unqualified<T>, BaseVariable>);
    }

    /// Take variables and reset the old ones
    // If RHS is Reference, RHS is left unchanged
    // If RHS is Value and held in stack, RHS is moved from
    // If RHS is Value and not held in stack, RHS is reset
    BaseVariable(BaseVariable &&v) noexcept : VariableData(static_cast<VariableData const &>(v)) {
        if (auto p = v.handle()) {
            if (stack) act(ActionType::move, p, this);
            else v.reset_data();
        }
    }

    /// Only call variable copy constructor if its lifetime is being managed
    BaseVariable(BaseVariable const &v) : VariableData(static_cast<VariableData const &>(v)) {
        if (auto p = v.handle()) act(ActionType::copy, p, this);
    }

    template <class T, std::enable_if_t<!std::is_base_of_v<VariableData, unqualified<T>>, int> = 0>
    BaseVariable & operator=(T &&t) {emplace(Type<std::decay_t<T>>(), static_cast<T &&>(t)); return *this;}

    /// Only call variable move constructor if its lifetime is being managed inside the buffer
    BaseVariable & operator=(BaseVariable &&v) noexcept {
        // DUMP("move assign ", type(), v.type());
        if (auto p = handle()) act(ActionType::destroy, p, nullptr);
        static_cast<VariableData &>(*this) = v;
        if (auto p = v.handle()) {
            if (stack) act(ActionType::move, p, this);
            else v.reset_data();
        }
        return *this;
    }

    BaseVariable & operator=(BaseVariable const &v) {
        // DUMP("copy assign ", type(), v.type());
        if (auto p = handle()) act(ActionType::destroy, p, nullptr);
        static_cast<VariableData &>(*this) = v;
        if (auto p = v.handle()) v.act(ActionType::copy, p, this);
        return *this;
    }

    ~BaseVariable() {
        if (auto p = handle()) act(ActionType::destroy, p, nullptr);
    }

    /**************************************************************************/

    void reset() {
        // DUMP("reset", type());

        if (auto p = handle()) act(ActionType::destroy, p, nullptr);
        reset_data();
    }

    constexpr bool has_value() const {return act;}
    explicit constexpr operator bool() const {return act;}

    /**************************************************************************/

    BaseVariable copy() && {return {*this, qualifier() == Value || qualifier() == Rvalue};}
    BaseVariable copy() const & {return {*this, qualifier() == Rvalue};}

    BaseVariable reference() & {return {pointer(), idx.add(Lvalue), act, stack};}
    BaseVariable reference() const & {return {pointer(), idx.add(Const), act, stack};}
    BaseVariable reference() && {return {pointer(), idx.add(Rvalue), act, stack};}

    bool move_if_lvalue() {return idx.qualifier() == Lvalue ? idx.set_qualifier(Rvalue), true : false;}

    /**************************************************************************/

    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> *target(Type<T> t={}) && {
        // DUMP(name(), typeid(Type<T>).name(), qual, stack);
        return target_pointer(t, add(qualifier(), Rvalue));
    }

    // return pointer to target if it is trivially convertible to requested type
    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> *target(Type<T> t={}) const & {
        // DUMP(name(), typeid(Type<T>).name(), qual, stack);
        return target_pointer(t, add(qualifier(), Const));
    }

    // return pointer to target if it is trivially convertible to requested type
    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> *target(Type<T> t={}) & {
        // DUMP(name(), typeid(Type<T>).name(), qual, stack);
        return target_pointer(t, add(qualifier(), Lvalue));
    }
};

/******************************************************************************/

}