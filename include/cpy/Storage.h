#pragma once
#include "Common.h"
#include "Error.h"

namespace cpy {

/******************************************************************************/

struct VariableData;

using Storage = std::aligned_storage_t<3 * sizeof(void*), std::alignment_of_v<void *>>;

template <class T, class=void>
struct UseStack : std::integral_constant<bool, false && (sizeof(T) <= sizeof(Storage))
    && (std::alignment_of_v<Storage> % std::alignment_of_v<T> == 0)
    && std::is_nothrow_move_constructible_v<T>> {
    static_assert(std::is_same_v<T, std::decay_t<T>>);
};

/******************************************************************************/

enum class ActionType : std::uint_fast8_t {destroy, copy, move, response, assign};
using ActionFunction = void(*)(ActionType, void *, VariableData *);

struct RequestData {
    std::type_index type;
    Dispatch *msg;
    Qualifier source, dest;
};

// static_assert(UseStack<RequestData>::value);
static_assert(std::is_trivially_destructible_v<RequestData>);

// destroy: delete the value stored at (void *)
// copy: copy value from (void *) into empty (Variable *)
// move: move value from (void *) into empty (Variable *)
// assign: assign existing value at (void *) = existing value in (Variable *)
// response: convert existing value in (void *) to new value in (Variable *)
//           using RequestData pre-stored in Variable->storage

/******************************************************************************/

static_assert(sizeof(std::type_info const *) == sizeof(std::type_index));
static_assert(alignof(std::type_info const *) == alignof(std::type_index));

struct VariableData {
    Storage buff; //< Buffer holding either pointer to the object, or the object itself
    std::type_info const * idx; //< typeid of the held object, or NULL
    ActionFunction act; //< Action<T>::apply of the held object, or NULL
    Qualifier qual; //< Qualifier of the held object (::V if empty)
    bool stack; //< Whether the held type (non-reference) can fit in the buffer

    constexpr VariableData() noexcept : buff(), idx(nullptr), act(nullptr), qual(Qualifier::V), stack(false) {}

    VariableData(std::type_info const *i, ActionFunction a, Qualifier q, bool s) noexcept : buff(), idx(i), act(a), qual(q), stack(s) {}

    void reset_data() noexcept {
        if (!act) return;
        buff = Storage();
        idx = nullptr;
        act = nullptr;
        stack = false;
        qual = Qualifier::V;
    }

    /// Return a pointer to the held object if it exists
    void * pointer() const noexcept {
        if (!act) return nullptr;
        else if (qual == Qualifier::V && stack) return &const_cast<Storage &>(buff);
        else return reinterpret_cast<void * const &>(buff);
    }

    /// Return a pointer to the held object if it exists and is being managed
    void * handle() const noexcept {
        if (!act || qual != Qualifier::V) return nullptr;
        else if (stack) return &const_cast<Storage &>(buff);
        else return reinterpret_cast<void * const &>(buff);
    }

    template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
    std::remove_reference_t<T> *target_pointer(Type<T> t, Qualifier q) const noexcept {
        // Qualifier is assumed not to be V
        return (idx && *idx == typeid(no_qualifier<T>)) && (
            (std::is_const_v<std::remove_reference_t<T>>)
            || (std::is_rvalue_reference_v<T> && q == Qualifier::R)
            || (std::is_lvalue_reference_v<T> && (q == Qualifier::L))
        ) ? static_cast<std::remove_reference_t<T> *>(pointer()) : nullptr;
    }
};

/******************************************************************************/

}
