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
typedef uint32_t rebind_input;

/// Input tag for raw operations
typedef uint32_t rebind_len;

/// Qualifier
enum rebind_tags {rebind_stack=0, rebind_heap=1, rebind_const=2, rebind_mutable=3};
typedef uint_fast8_t rebind_tag;

/// List of arguments
typedef struct rebind_args rebind_args;

// typedef void (*rebind_fptr)(void);
// typedef union rebind_storage {
//     void *pointer;
//     char storage[16];
// } rebind_storage;

/******************************************************************************************/

/// rebind_index is a function pointer of the following type
// typedef rebind_stat (*rebind_index)(rebind_input t, void *a, void *b, rebind_args);

typedef rebind_stat (*rebind_index)(
    rebind_input tag,       // slot for dispatching the operation
    rebind_len size,     // slot for additional integer input
    void* output,         // slot for output location of any type
    void* self,           // slot for source reference
    rebind_args *args);   // slot for function arguments

    // rebind_fptr function, // slot for function pointer of any type
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
    void *caller_ptr;
    uint32_t args;
} rebind_args;

/// span of a contiguous array of arguments
typedef struct rebind_name_args {
    char const *name_ptr;
    void *caller_ptr;
    uint32_t name_len, args;
} rebind_name_args;

/******************************************************************************************/

/// rebind_str is essentially an in-house copy of std::string_view
typedef struct rebind_str {
    char const *pointer;
    uintptr_t len;
} rebind_str;

/******************************************************************************************/

typedef struct rebind_exception {
    rebind_index index;
    void *pointer;
} rebind_exception;

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

// Input the pointer's last 2 bits with the qualifier
inline rebind_index rebind_tag_index(rebind_index i, rebind_tag q) {
    return (rebind_index)( (uintptr_t)(i) | (uintptr_t)(q) );
}

// Get out the last 2 bits as the tag
inline rebind_tag rebind_get_tag(rebind_index i) {
    return (rebind_tag)( (uintptr_t)(i) & (uintptr_t)(3) );
}

// Get out the pointer minus the tag
inline rebind_index rebind_get_index(rebind_index i) {
    return (rebind_index)( (uintptr_t)(i) & ~((uintptr_t)(3)) );
}

/******************************************************************************************/

#ifdef __cplusplus
}

#include <type_traits>

namespace rebind {

// C++ version of rebind_tags enum
enum Tag : rebind_tag {Stack=rebind_stack, Heap=rebind_heap, Const=rebind_const, Mutable=rebind_mutable};

static char const * TagNames[4] = {"stack", "heap", "const", "mutable"};

// C++ alias of rebind_input
using Input = rebind_input;

using Len = rebind_len;

using Stat = rebind_stat;

using Idx = rebind_index;

namespace input {
    static constexpr Input const
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

inline char const * input_name(Input t) {
    switch (t) {
        case input::check:      return "check";
        case input::destruct:   return "destruct";
        case input::copy:       return "copy";
        case input::name:       return "name";
        case input::info:       return "info";
        case input::relocate:   return "relocate";
        case input::call:       return "call";
        case input::dump:       return "dump";
        case input::load:       return "load";
        case input::assign:     return "assign";
        default:                return "unknown";
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

class Scope;
class Caller;
class ArgView;
class Ref;

template <class T>
static constexpr bool is_usable = true
    && !std::is_void_v<T>
    && !std::is_const_v<T>
    && !std::is_reference_v<T>
    && !std::is_volatile_v<T>
    && !std::is_same_v<T, Ref>;
    // && !is_type_t<T>::value;
    // && !std::is_null_pointer_v<T>
    // && !std::is_function_v<T>

/******************************************************************************/

template <class T>
constexpr void assert_usable() {
    // static_assert(std::is_nothrow_destructible_v<T>);
    static_assert(!std::is_void_v<T>);
    static_assert(!std::is_const_v<T>);
    static_assert(!std::is_reference_v<T>);
    static_assert(!std::is_volatile_v<T>);
    static_assert(!std::is_same_v<T, Ref>);
    // static_assert(!std::is_same_v<T, Value>);
    // static_assert(!is_type_t<T>::value);
    // static_assert(!std::is_null_pointer_v<T>);
}

}

#endif

#endif