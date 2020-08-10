#pragma once
#include "API.h"
#include <type_traits>

/******************************************************************************************/

#define ARA_DEFINE(NAME, TYPE) ara_stat ARA_FUNCTION(NAME)(ara_input i, void* s, void* p, void* a) noexcept {return ara::Switch<TYPE>::invoke(i,s,p,a);}

#define ARA_DECLARE(NAME, TYPE) \
    extern "C" ara_stat ARA_FUNCTION(NAME)(ara_input, void*, void*, void*) noexcept; \
    namespace ara {template<> struct Lookup<TYPE> {static constexpr ara_index invoke = ARA_FUNCTION(NAME);};}

/******************************************************************************************/

namespace ara {

// C++ version of ara_tags enum
enum class Mode : ara_mode {Stack=ara_stack, Heap=ara_heap, Read=ara_read, Write=ara_write};

static char const * TagNames[4] = {"stack", "heap", "const", "mutable"};

// C++ alias of ara_code
using Context = ara_context;
using Code    = ara_code;
using Stat    = ara_stat;
using Idx     = ara_index;
using Integer = ara_integer;
using Float   = ara_float;

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

enum codes {
    check      = 0,
    destruct   = 1,
    deallocate = 2,
    copy       = 3,
    swap       = 4,
    name       = 5,
    info       = 6,
    relocate   = 7,
    call       = 8,
    method     = 9,
    dump       = 10,
    load       = 11,
    attribute  = 12,
    element    = 13,
    hash       = 14,
    compare    = 15,
    equal      = 16
    // unary
    // binary
    // print
    // increment/decrement
};

inline constexpr bool valid(Code c) {return c < 17;}

}

inline char const * code_name(Code c) {
    if (!code::valid(c)) return "unknown";
    switch (static_cast<code::codes>(c)) {
        case code::check :      return "check";
        case code::destruct :   return "destruct";
        case code::deallocate : return "deallocate";
        case code::copy :       return "copy";
        case code::swap :       return "swap";
        case code::hash :       return "hash";
        case code::name :       return "name";
        case code::info :       return "info";
        case code::relocate :   return "relocate";
        case code::call :       return "call";
        case code::method :     return "method";
        case code::dump :       return "dump";
        case code::load :       return "load";
        case code::attribute :  return "attribute";
        case code::element :    return "element";
        case code::compare :    return "compare";
        case code::equal :      return "equal";
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

