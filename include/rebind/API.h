#ifndef REBIND_API_H
#define REBIND_API_H

/*
    Defines raw C API and corresponding C++ API
*/

#include <stdint.h> // uintptr_t
#include <stddef.h> // size_t

#ifdef __cplusplus

extern "C" {

#endif

/// Output status code for raw operations
typedef int32_t rebind_stat;

/// Input tag for raw operations
typedef uint32_t rebind_tag;

/// Input tag for raw operations
typedef uint32_t rebind_len;

/// Qualifier
enum rebind_qualifiers {rebind_const=0, rebind_mutable=1, rebind_stack=2, rebind_heap=3};
typedef uint_fast8_t rebind_qualifier;

/// List of arguments
typedef struct rebind_args rebind_args;

typedef void (*rebind_fptr)(void);
// typedef union rebind_storage {
//     void *pointer;
//     char storage[16];
// } rebind_storage;

/******************************************************************************************/

/// rebind_index is a function pointer of the following type
// typedef rebind_stat (*rebind_index)(rebind_tag t, void *a, void *b, rebind_args);

typedef rebind_stat (*rebind_index)(
    rebind_tag tag,       // slot for dispatching the operation
    rebind_len size,     // slot for additional integer input
    void* output,         // slot for output location of any type
    rebind_fptr function, // slot for function pointer of any type
    void* self,           // slot for source reference
    rebind_args args);    // slot for function arguments

/******************************************************************************************/

/*
An optional reference type containing:
    1) the index of the held type
    2) the address of the held type
    3) and the qualifier of the held type (Const, Mutable, Stack, Heap)
Currently the qualifier is held as a tag on the rebind_index pointer.
The logic of the qualifiers is roughly that Const = T const & and Mutable = T &.
Stack and Heap both refer to temporary references. These are references to objects that will
be deleted soon and may be deleted manually at any time by the user of the reference.
Stack means that ~T() should be used, and Heap means that delete (or equivalent) should be used.
The logic behind tagging the index rather than the data is that maybe the data can hold
the actual value itself if the value is trivial and fits in void *.
*/
typedef struct rebind_ref {
    rebind_index tag_index;
    void *pointer;
} rebind_ref;

/******************************************************************************************/

/// span of a contiguous array of arguments
typedef struct rebind_args {
    rebind_ref *pointer;
    size_t len;
} rebind_args;

/******************************************************************************************/

/// rebind_str is essentially an in-house copy of std::string_view
typedef struct rebind_str {
    char const *pointer;
    uintptr_t len;
} rebind_str;

/******************************************************************************************/

// type-erasure used to deallocate another pointer
typedef struct rebind_alloc {
    void *pointer; // for std::allocator, this is just reinterpreted as capacity
    void (*destructor)(size_t, void *); // destructor function pointer, called with size and data
} rebind_alloc;

/// small buffer optimization of same size as rebind_alloc
typedef union rebind_alloc_sbo {
    rebind_alloc alloc;
    char storage[sizeof(rebind_alloc)];
} rebind_alloc_sbo;

/******************************************************************************************/

/// A simple std::string-like class containing a type-erased destructor and SSO
typedef struct rebind_string {
    rebind_alloc_sbo storage;
    size_t size; // if size <= sizeof(storage), SSO is used
} rebind_string;

/******************************************************************************************/

// reinterpret_cast is legal C++ for interconversion of uintptr_t and void *
// need to check about function pointer though
// 32-bit (4 bytes): the pointer must be a multiple of 4 (2 bits are guaranteed 0)
// 64-bit (8 bytes): the pointer must be a multiple of 8 (3 bits are guaranteed 0)
// We have 4 qualifiers, so we use the last 2 bits for the qualifier tagging, i.e. 11 = 3

// Tag the pointer's last 2 bits with the qualifier
inline rebind_index rebind_tag_index(rebind_index i, rebind_qualifier q) {
    return (rebind_index)( (uintptr_t)(i) & (uintptr_t)(q) );
}

// Get out the last 2 bits as the qualifier
inline rebind_qualifier rebind_get_tag(rebind_index i) {
    return (rebind_qualifier)( (uintptr_t)(i) & (uintptr_t)(3) );
}

// Get out the pointer minus the qualifier
inline rebind_index rebind_get_index(rebind_index i) {
    return (rebind_index)( (uintptr_t)(i) & !(uintptr_t)(3) );
}

/******************************************************************************************/

#ifdef __cplusplus
}

#include <type_traits>

namespace rebind {

// C++ version of rebind_qualifiers enum
enum Qualifier : rebind_qualifier {Const=rebind_const, Mutable=rebind_mutable, Stack=rebind_stack, Heap=rebind_heap};

static char const * QualifierNames[4] = {"const", "mutable", "stack", "heap"};

// Some opaque function pointer
using Fptr = rebind_fptr;

// C++ alias of rebind_tag
using Tag = rebind_tag;

using Len = rebind_len;

namespace tag {
    static constexpr Tag const
        check            {0},
        dealloc          {1},
        destruct         {2},
        copy             {3},
        name             {4},
        info             {5},
        relocate         {6},
        call             {7},
        dump             {8},
        load             {9};
        // assign           {9};
}

using Stat = rebind_stat;

namespace stat {
    enum class copy :     Stat {ok, unavailable, exception};
    enum class relocate : Stat {ok, unavailable};
    enum class drop :     Stat {ok, unavailable};
    enum class info :     Stat {ok, unavailable};
    enum class name :     Stat {ok, unavailable};
    // enum class load :  Stat {ok, unavailable, exception, none};
    enum class dump :     Stat {ok, unavailable, exception, none, null};
    // enum class assign : Stat {ok, unavailable, exception, none, null};
    enum class call :   Stat {ok, unavailable, exception, none, invalid_return, wrong_number, wrong_type};

    template <class T>
    Stat put(T t) {static_assert(std::is_enum_v<T>); return static_cast<Stat>(t);}

}


inline char const * tag_name(Tag t) {
    switch (t) {
        case tag::check:      return "check";
        case tag::dealloc:    return "dealloc";
        case tag::destruct:   return "destruct";
        // case tag::copy:       return "copy";
        case tag::name:       return "name";
        case tag::info:       return "info";
        // case tag::method:     return "method";
        case tag::call:       return "call";
        case tag::dump:       return "dump";
        // case tag::assign:     return "assign";
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

class Output;
class Scope;
class Caller;
class ArgView;
class Ref;

template <class T>
static constexpr bool is_usable = true
    &&  std::is_nothrow_destructible_v<T>
    && !std::is_void_v<T>
    && !std::is_const_v<T>
    && !std::is_reference_v<T>
    && !std::is_volatile_v<T>;
    // && !std::is_same_v<T, Value>;
    // && !is_type_t<T>::value;
    // && !std::is_null_pointer_v<T>
    // && !std::is_function_v<T>

/******************************************************************************/

template <class T>
constexpr void assert_usable() {
    static_assert(std::is_nothrow_destructible_v<T>);
    static_assert(!std::is_void_v<T>);
    static_assert(!std::is_const_v<T>);
    static_assert(!std::is_reference_v<T>);
    static_assert(!std::is_volatile_v<T>);
    // static_assert(!std::is_same_v<T, Value>);
    // static_assert(!is_type_t<T>::value);
    // static_assert(!std::is_null_pointer_v<T>);
}

}

#endif

#endif