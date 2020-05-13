#pragma once
#include "API.h"
#include <type_traits>

/******************************************************************************************/

#define ARA_DEFINE(NAME, TYPE) ara_stat ARA_FUNCTION(NAME)(ara_input i, void* s, void* p, void* a) noexcept {return ara::Switch<TYPE>::call(i,s,p,a);}

#define ARA_DECLARE(NAME, TYPE) \
    extern "C" ara_stat ARA_FUNCTION(NAME)(ara_input, void*, void*, void*) noexcept; \
    namespace ara {template<> struct Lookup<TYPE> {static constexpr ara_index call = ARA_FUNCTION(NAME);};}

/******************************************************************************************/

namespace ara {

// C++ version of ara_tags enum
enum class Tag : ara_tag {Stack=ara_stack, Heap=ara_heap, Const=ara_const, Mutable=ara_mutable};

static char const * TagNames[4] = {"stack", "heap", "const", "mutable"};

// C++ alias of ara_code
using Code    = ara_code;
using Stat    = ara_stat;
using Idx     = ara_index;
using Integer = ara_integer;
using Float   = ara_float;
using Bool    = ara_bool;

/******************************************************************************************/

// Alias<T> is used in special cases to allow reinterpret_cast between an exposed C type
// and an exposed C++ type without disobeying strict aliasing.

template <class T>
struct AliasType {using type = T;};

template <class T>
using Alias = typename AliasType<T>::type;

/******************************************************************************************/

// is_dependent<T> marks whether a type holds a reference to something else (to do ... clarify)

template <class T>
struct Dependent : std::is_reference<T> {};

template <class T>
static constexpr bool is_dependent = Dependent<T>::value;

/******************************************************************************************/

static_assert(sizeof(ara_input) == 2 * sizeof(ara_code));

namespace code {
    static constexpr Code const
        check            {0},
        destruct         {1},
        copy             {2},
        name             {3},
        info             {4},
        relocate         {5},
        call             {6},
        dump             {7},
        load             {8},
        assign           {9};
}

inline char const * code_name(Code t) {
    switch (t) {
        case code::check:      return "check";
        case code::destruct:   return "destruct";
        case code::copy:       return "copy";
        case code::name:       return "name";
        case code::info:       return "info";
        case code::relocate:   return "relocate";
        case code::call:       return "call";
        case code::dump:       return "dump";
        case code::load:       return "load";
        case code::assign:     return "assign";
        default:               return "unknown";
    }
}

/******************************************************************************/

struct ArgView;
union Ref;
union Target;

template <class T>
static constexpr bool is_implementable = true
    && !std::is_void_v<T>
    && !std::is_const_v<T>
    && !std::is_reference_v<T>
    && !std::is_volatile_v<T>
    && !std::is_same_v<T, Ref>
    && !std::is_same_v<T, Target>
    && !std::is_null_pointer_v<T>;
    // && !is_type_t<T>::value;
    // && !std::is_function_v<T>

/******************************************************************************/

template <class T>
constexpr void assert_implementable() {
    // static_assert(std::is_nothrow_destructible_v<T>);
    static_assert(!std::is_void_v<T>);
    static_assert(!std::is_const_v<T>);
    static_assert(!std::is_reference_v<T>);
    static_assert(!std::is_volatile_v<T>);
    static_assert(!std::is_same_v<T, Ref>);
    static_assert(!std::is_same_v<T, Target>);
    static_assert(!std::is_null_pointer_v<T>);
    // static_assert(!is_type_t<T>::value);
}

/******************************************************************************/

}

