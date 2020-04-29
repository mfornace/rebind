#ifndef ARA_API_H
#define ARA_API_H

/*
    Defines raw C API and corresponding C++ API
*/

#include <stdint.h> // uintptr_t
#include <stddef.h> // size_t

#ifdef __cplusplus

extern "C" {

#endif

/// Output status code for raw operations
typedef int32_t ara_stat;

/// Input tag for raw operations
typedef uint32_t ara_code;

typedef struct ara_input {
    ara_code code, tag;
} ara_input;

/// Qualifier
enum ara_tags {ara_stack=0, ara_heap=1, ara_const=2, ara_mutable=3};
typedef uint_fast8_t ara_tag;

/// List of arguments
typedef struct ara_args ara_args;

// typedef void (*ara_fptr)(void);
// typedef union ara_storage {
//     void *pointer;
//     char storage[16];
// } ara_storage;

typedef intmax_t ara_integer;
typedef double ara_float;

/******************************************************************************************/

/// ara_index is a function pointer of the following type
// typedef ara_stat (*ara_index)(ara_code t, void *a, void *b, ara_args);

typedef ara_stat (*ara_index)(
    ara_input code_tag,  // slot for dispatching the operation, slot for additional integer input
    void* output,           // slot for output location of any type
    void* self,             // slot for source reference
    ara_args *args);     // slot for function arguments

    // ara_fptr function, // slot for function pointer of any type
/******************************************************************************************/

/*
An optional reference type containing:
    1) the index of the held type
    2) the address of the held type
    3) and the qualifier of the held type (Const, Mutable, Stack, Heap)
Currently the qualifier is held as a tag on the ara_index pointer.
The logic of the qualifiers is roughly that Const = T const & and Mutable = T &.
Stack and Heap both refer to temporary references. These are references to objects that will
be deleted soon and may be deleted manually at any time by the user of the reference.
Stack means that ~T() should be used, and Heap means that delete (or equivalent) should be used.
The logic behind tagging the index rather than the data is that maybe the data can hold
the actual value itself if the value is trivial and fits in void *.
*/
typedef struct ara_ref {
    ara_index tag_index;
    void *pointer;
} ara_ref;

/******************************************************************************************/

/// span of a contiguous array of tags and arguments
typedef struct ara_args {
    void *caller_ptr;
    uint32_t tags, args;
} ara_args;

/******************************************************************************************/

/// ara_str is essentially an in-house copy of std::string_view
typedef struct ara_str {
    char const *pointer;
    uintptr_t len;
} ara_str;

/******************************************************************************************/

// typedef struct ara_exception {
//     ara_index index;
//     void *pointer;
// } ara_exception;

/******************************************************************************************/

// type-erasure used to deallocate another pointer
typedef struct ara_alloc {
    void *pointer; // for std::allocator, this is just reinterpreted as capacity
    void (*destructor)(size_t, void *); // destructor function pointer, called with size and data
} ara_alloc;

/// small buffer optimization of same size as ara_alloc
typedef union ara_alloc_sbo {
    ara_alloc alloc;
    char storage[sizeof(ara_alloc)];
} ara_alloc_sbo;

/******************************************************************************************/

/// A simple std::string-like class containing a type-erased destructor and SSO
typedef struct ara_string {
    ara_alloc_sbo storage;
    size_t size; // if size <= sizeof(storage), SSO is used
} ara_string;

/******************************************************************************************/

typedef struct ara_array {
    ara_index index; // type and qualifier of the held type
    void *pointer;      // address to the start of the array
    size_t length;      // length of the array
    void (*destructor)(size_t, void *);
} ara_array;

/******************************************************************************************/

typedef struct ara_target {
    // Requested type index. May be null if no type is requested
    ara_index index;
    // Output storage address. Must satisfy at least void* alignment and size requirements.
    void* output;
    // Bit mask for dependent reference argument indices
    std::uint64_t lifetime;
    // Output storage capacity in bytes (sizeof)
    ara_code length;
    // Requested qualifier (roughly T, T &, or T const &)
    ara_tag tag;
    // void* allocator_data;
    // void* (*allocator)(Code size, Code alignment) noexcept
} ara_target;

/******************************************************************************************/

// reinterpret_cast is legal C++ for interconversion of uintptr_t and void *
// need to check about function pointer though
// 32-bit (4 bytes): the pointer must be a multiple of 4 (2 bits are guaranteed 0)
// 64-bit (8 bytes): the pointer must be a multiple of 8 (3 bits are guaranteed 0)
// We have 4 qualifiers, so we use the last 2 bits for the qualifier tagging, i.e. 11 = 3

// Input the pointer's last 2 bits with the qualifier
inline ara_index ara_tag_index(ara_index i, ara_tag q) {
    return (ara_index)( (uintptr_t)(i) | (uintptr_t)(q) );
}

// Get out the last 2 bits as the tag
inline ara_tag ara_get_tag(ara_index i) {
    return (ara_tag)( (uintptr_t)(i) & (uintptr_t)(3) );
}

// Get out the pointer minus the tag
inline ara_index ara_get_index(ara_index i) {
    return (ara_index)( (uintptr_t)(i) & ~((uintptr_t)(3)) );
}

/******************************************************************************************/

#define ARA_FUNCTION(NAME) ara_define_##NAME

#ifdef __cplusplus
}

#define ARA_DEFINE(NAME, TYPE) ara_stat ARA_FUNCTION(NAME)(ara_input i, void* s, void* p, ara_args* a) noexcept {return ara::impl<TYPE>::build(i,s,p,a);}

#define ARA_DECLARE(NAME, TYPE) \
    extern "C" ara_stat ARA_FUNCTION(NAME)(ara_input, void*, void*, ara_args*) noexcept; \
    namespace ara {inline ara_index fetch(ara::Type<TYPE>) {return ARA_FUNCTION(NAME);}}

#include <type_traits>

namespace ara {

// C++ version of ara_tags enum
enum class Tag : ara_tag {Stack=ara_stack, Heap=ara_heap, Const=ara_const, Mutable=ara_mutable};

static char const * TagNames[4] = {"stack", "heap", "const", "mutable"};

// C++ alias of ara_code
using Code = ara_code;

using Stat = ara_stat;

using Idx = ara_index;

using Integer = ara_integer;
using Float = ara_float;

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
        default:                return "unknown";
    }
}

/******************************************************************************/

// class Caller;
class ArgView;
struct Ref;
struct Target;

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