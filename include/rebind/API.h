#ifndef REBIND_API_H
#define REBIND_API_H
#include "stdint.h"

#ifdef __cplusplus

extern "C" {

#endif

typedef int rebind_stat;

typedef unsigned int rebind_tag;

typedef unsigned char rebind_qualifier;

typedef void * rebind_ptr;

typedef struct rebind_args rebind_args;

/******************************************************************************************/

// rebind_index is a function pointer of the following type
typedef rebind_stat (*rebind_index)(rebind_tag t, void *a, void *b, rebind_args);

/******************************************************************************************/

typedef struct rebind_ref {
    rebind_index ind;
    rebind_ptr tagged_ptr;
} rebind_ref;

/******************************************************************************************/

typedef struct rebind_str {
    char const *data;
    uintptr_t size;
} rebind_str;

/******************************************************************************************/

typedef struct rebind_value {
    rebind_index ind;
    void *ptr;
} rebind_value;

/******************************************************************************************/

typedef struct rebind_args {
    rebind_ref *ptr;
    uint64_t len;
    // unsigned char qual;
} rebind_args;

/******************************************************************************************/


// reinterpret_cast is legal C++ for interconversion of uintptr_t and void *
// pointer tagging is legal C++, but implementation defined whether the bits are indeed 0 where assumed
// 32-bit (4 bytes): the pointer must be a multiple of 4 (2 bits are guaranteed 0)
// 64-bit (8 bytes): the pointer must be a multiple of 8 (3 bits are guaranteed 0)
// Below we use the last 2 bits for the qualifier tagging, i.e. 11 = 3

// Tag the pointer's last 2 bits with the qualifier
inline rebind_ptr rebind_make_ptr(void *v, rebind_qualifier q) {
    return (rebind_ptr)( (uintptr_t)(v) & (uintptr_t)(q) );
}

// Get out the last 2 bits as the qualifier
inline rebind_qualifier rebind_get_qualifier(rebind_ptr p) {
    return (rebind_qualifier)((uintptr_t)(p) & (uintptr_t)(3));
}

// Get out the pointer minus the qualifier
inline void * rebind_get_address(rebind_ptr p) {
    return (void *)((uintptr_t)(p) & !(uintptr_t)(3));
}

#ifdef __cplusplus
}

#include <type_traits>

namespace rebind {


using Tag = rebind_tag;

namespace tag {
    static constexpr Tag const
        check            {0},
        drop             {1},
        copy             {2},
        name             {3},
        info             {4},
        method_to_value  {5},
        method_to_ref    {6},
        call_to_value    {7},
        call_to_ref      {8},
        request_to_value {9},
        request_to_ref   {10},
        assign_to        {11};
}


using Stat = rebind_stat;

namespace stat {
    enum class copy :      Stat {ok, unavailable, exception};
    enum class drop :      Stat {ok, unavailable};
    enum class info :      Stat {ok, unavailable};
    enum class from_ref :  Stat {ok, unavailable, exception, none};
    enum class request :   Stat {ok, unavailable, exception, none, null};
    enum class assign_if : Stat {ok, unavailable, exception, none, null};
    enum class call :      Stat {ok, unavailable, exception, none, invalid_return, wrong_number, wrong_type};

}


inline char const * tag_name(Tag t) {
    switch (t) {
        case tag::check: return "check";
        case tag::drop: return "drop";
        case tag::copy: return "copy";
        case tag::name: return "name";
        case tag::info: return "info";
        case tag::method_to_value: return "method_to_value";
        case tag::method_to_ref: return "method_to_ref";
        case tag::call_to_value: return "call_to_value";
        case tag::call_to_ref: return "call_to_ref";
        case tag::request_to_value: return "request_to_value";
        case tag::request_to_ref: return "request_to_ref";
        case tag::assign_to: return "assign_to";
        default: return "unknown";
    }
}

/******************************************************************************/

class Ref;
class Value;
class Output;
class Scope;
class Caller;
class ArgView;

template <class T>
static constexpr bool is_usable = true
    && !std::is_reference_v<T>
    && !std::is_const_v<T>
    && !std::is_volatile_v<T>
    && !std::is_void_v<T>
    && std::is_nothrow_destructible_v<T>
    && !std::is_same_v<T, Ref>
    && !std::is_same_v<T, Value>;
    // && !is_type_t<T>::value;
    // && !std::is_null_pointer_v<T>
    // && !std::is_function_v<T>

/******************************************************************************/

template <class T>
constexpr void assert_usable() {
    static_assert(!std::is_reference_v<T>);
    static_assert(!std::is_const_v<T>);
    static_assert(!std::is_volatile_v<T>);
    static_assert(!std::is_void_v<T>);
    static_assert(std::is_nothrow_destructible_v<T>);

    static_assert(!std::is_same_v<T, Ref>);
    static_assert(!std::is_same_v<T, Value>);
    // static_assert(!is_type_t<T>::value);

    // static_assert(!std::is_null_pointer_v<T>);
}

}

#endif

#endif