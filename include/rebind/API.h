#ifndef REBIND_API_H
#define REBIND_API_H
#include "stdint.h"

#ifdef __cplusplus

extern "C" {

#endif

/// Output status code for raw operations
typedef int rebind_stat;

/// Input tag for raw operations
typedef unsigned int rebind_tag;

/// Qualifier: 0=const, 1=lvalue, 2=stack, 3=heap
typedef unsigned char rebind_qualifier;

/// List of arguments
typedef struct rebind_args rebind_args;

typedef union rebind_storage {
    void *pointer;
    char storage[16];
} rebind_storage;

/******************************************************************************************/

/// rebind_index is a function pointer of the following type
typedef rebind_stat (*rebind_index)(rebind_tag t, void *a, void *b, rebind_args);

/******************************************************************************************/

typedef struct rebind_value {
    rebind_index tag_index;
    rebind_storage storage;
} rebind_value;

/******************************************************************************************/

/// span of a contiguous array of rebind_value
typedef struct rebind_args {
    rebind_value *ptr;
    uint64_t len;
} rebind_args;

/******************************************************************************************/

/// rebind_str is essentially an in-house copy of std::string_view
typedef struct rebind_str {
    char const *data;
    uintptr_t size;
} rebind_str;

/******************************************************************************************/

// reinterpret_cast is legal C++ for interconversion of uintptr_t and void *
// need to check about function pointer though
// 32-bit (4 bytes): the pointer must be a multiple of 4 (2 bits are guaranteed 0)
// 64-bit (8 bytes): the pointer must be a multiple of 8 (3 bits are guaranteed 0)
// We have 4 qualifiers, so we use the last 2 bits for the qualifier tagging, i.e. 11 = 3

// Tag the pointer's last 2 bits with the qualifier
inline rebind_index rebind_tagged_index(rebind_index i, rebind_qualifier q) {
    return (rebind_index)( (uintptr_t)(i) & (uintptr_t)(q) );
}

// Get out the last 2 bits as the qualifier
inline rebind_qualifier rebind_get_qualifier(rebind_index i) {
    return (rebind_qualifier)((uintptr_t)(i) & (uintptr_t)(3));
}

// Get out the pointer minus the qualifier
inline rebind_index rebind_get_index(rebind_index i) {
    return (rebind_index)((uintptr_t)(i) & !(uintptr_t)(3));
}

/******************************************************************************************/

#ifdef __cplusplus
}

#include <type_traits>

namespace rebind {

using Storage = rebind_storage;
/*
Destructor behavior:
if Const or Mutable, storage holds a (T *) and nothing is done
if External, storage holds a (T *) and the destructor is run
if Managed, storage holds either a (T *) or a (T) and the dealloc is run as needed
*/

enum Qualifier : rebind_qualifier {Const, Mutable, Managed, External};

static char const * QualifierNames[4] = {"const", "mutable", "managed", "external"};

using Tag = rebind_tag;

namespace tag {
    static constexpr Tag const
        check            {0},
        dealloc          {1},
        destruct         {2},
        copy             {3},
        name             {4},
        info             {5},
        method           {6},
        call             {7},
        dump             {8},
        assign           {9};
}

using Stat = rebind_stat;

namespace stat {
    enum class copy :   Stat {ok, unavailable, exception};
    enum class drop :   Stat {ok, unavailable};
    enum class info :   Stat {ok, unavailable};
    enum class load :   Stat {ok, unavailable, exception, none};
    enum class dump :   Stat {ok, unavailable, exception, none, null};
    enum class assign : Stat {ok, unavailable, exception, none, null};
    enum class call :   Stat {ok, unavailable, exception, none, invalid_return, wrong_number, wrong_type};
}


inline char const * tag_name(Tag t) {
    switch (t) {
        case tag::check:      return "check";
        case tag::dealloc:    return "dealloc";
        case tag::destruct:   return "destruct";
        case tag::copy:       return "copy";
        case tag::name:       return "name";
        case tag::info:       return "info";
        case tag::method:     return "method";
        case tag::call:       return "call";
        case tag::dump:       return "dump";
        case tag::assign:     return "assign";
        default:              return "unknown";
    }
}

/******************************************************************************/

template <class T, class SFINAE=void>
struct is_trivially_relocatable : std::is_trivially_copyable<T> {};

template <class T>
static constexpr bool is_trivially_relocatable_v = is_trivially_relocatable<T>::value;

template <class T>
static constexpr bool is_stack_type = false;

/******************************************************************************/

class Value;
class Output;
class Scope;
class Caller;
class ArgView;

template <class T>
static constexpr bool is_manageable = true
    &&  std::is_nothrow_destructible_v<T>
    && !std::is_void_v<T>
    && !std::is_const_v<T>
    && !std::is_reference_v<T>
    && !std::is_volatile_v<T>
    && !std::is_same_v<T, Value>;
    // && !is_type_t<T>::value;
    // && !std::is_null_pointer_v<T>
    // && !std::is_function_v<T>

/******************************************************************************/

template <class T>
constexpr void assert_manageable() {
    static_assert(std::is_nothrow_destructible_v<T>);
    static_assert(!std::is_void_v<T>);
    static_assert(!std::is_const_v<T>);
    static_assert(!std::is_reference_v<T>);
    static_assert(!std::is_volatile_v<T>);
    static_assert(!std::is_same_v<T, Value>);
    // static_assert(!is_type_t<T>::value);
    // static_assert(!std::is_null_pointer_v<T>);
}

}

#endif

#endif