#ifndef ARA_API_H
#define ARA_API_H

/*
Defines raw C API
*/

#include <stdint.h> // uintptr_t
#include <stddef.h> // size_t

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************************/

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

typedef intmax_t ara_integer;
typedef double ara_float;

/******************************************************************************************/

// ara_index := ara_stat(ara_code, void*, void*, void*)
typedef ara_stat (*ara_index)(
    ara_input code_tag,  // slot for dispatching the operation, slot for additional integer input
    void* output,           // slot for output location of any type
    void* self,             // slot for source reference
    void* args);     // slot for function arguments

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

/// due to unlikely issue of conflicting implementations of bool ...
typedef struct ara_bool {
    unsigned char value; // either 0 or 1. Guaranteed 1 byte of storage.
} ara_bool;

/******************************************************************************************/

/// ara_str is essentially an in-house copy of std::string_view.
typedef struct ara_str {
    char const *data;
    size_t size;
} ara_str;

/******************************************************************************************/

// type-erasure used to deallocate another pointer
typedef struct ara_string_alloc {
    char *pointer; // for std::allocator, this is just reinterpreted as capacity
    void (*destructor)(char*, size_t); // destructor function pointer, called with size and data
} ara_string_alloc;

/// small buffer optimization of same size as ara_string_alloc
typedef union ara_string_sbo {
    char storage[sizeof(ara_string_alloc)];
    ara_string_alloc alloc;
} ara_string_sbo;

/// A simple null-terminated std::string-like class containing a type-erased destructor and SSO
typedef struct ara_string {
    ara_string_sbo sbo;
    size_t size; // if size < sizeof(storage), SSO is used
} ara_string;

/******************************************************************************************/

/// ara_str is essentially an in-house copy of std::string_view
typedef struct ara_bin {
    unsigned char const *data;
    uintptr_t size;
} ara_bin;

// type-erasure used to deallocate another pointer
typedef struct ara_binary_alloc {
    unsigned char *pointer; // for std::allocator, this is just reinterpreted as capacity
    void (*destructor)(size_t, void *); // destructor function pointer, called with size and data
} ara_binary_alloc;

/// small buffer optimization of same size as ara_binary_alloc
typedef union ara_binary_sbo {
    ara_binary_alloc alloc;
    char storage[sizeof(ara_binary_alloc)];
} ara_binary_sbo;

/// A simple non-null-terminated byte container containing a type-erased destructor and SSO
typedef struct ara_binary {
    ara_binary_sbo sbo;
    size_t size; // if size <= sizeof(storage), SSO is used
} ara_binary;

/******************************************************************************************/

typedef struct ara_shape_alloc {
    size_t* dimensions;
    void (*destructor)(size_t*, uint32_t);
} ara_shape_alloc;

// Either shape pointer or the length itself if rank=1
typedef union ara_shape {
    size_t stack[2];       // 1 or 2 dimensions if rank <= 2
    ara_shape_alloc alloc; // array of more dimensions, with its deleter
} ara_shape;

// Contiguous container similar to a type-erased std::vector (extend to ND shape?)
typedef struct ara_span {
    ara_shape shape;
    ara_index index;   // Type and qualifier of the held type
    void* data;        // Address to the start of the array
    uint32_t rank;     // Rank of the array
    uint32_t item;     // Item size
} ara_span;

// Contiguous container similar to a type-erased std::vector (extend to ND shape?)
typedef struct ara_array {
    ara_span span;
    void *storage; // destructor data, could maybe be folded into destructor in the future
    void (*destructor)(ara_index, void*); // called with previous members
} ara_array;

/******************************************************************************************/

// View-like container of heterogeneous types
typedef struct ara_view {
    ara_ref* data;                               // address to the start of the array
    size_t size;                                 // if shape is given, the rank. otherwise length of array
    void (*destructor)(ara_ref*, size_t); // called with previous members
} ara_view;

// Value container of heterogeneous types
typedef struct ara_tuple {
    ara_ref* data;                               // address to the start of the array
    size_t size;                                 // if shape is given, number of shape. otherwise length of array
    void* storage;
    void (*destructor)(ara_ref*, size_t, void*); // called with previous members (fold in storage?)
} ara_tuple;

/******************************************************************************************/

// Main type used for emplacing a function output
typedef struct ara_target {
    // Requested type index. May be null if no type is requested
    ara_index index;
    // Output storage address. Must satisfy at least void* alignment and size requirements.
    void* output;
    // Bit mask for dependent reference argument indices
    uint64_t lifetime;
    // Output storage capacity in bytes (sizeof)
    ara_code length;
    // Requested qualifier (roughly T, T &, or T const &)
    ara_tag tag;
    // void* allocator_data;
    // void* (*allocator)(Code size, Code alignment) noexcept
} ara_target;

/******************************************************************************************/

// reinterpret_cast is legal C++ for interconversion of uintptr_t and void *
// 32-bit (4 bytes): the pointer must be a multiple of 4 (2 bits are guaranteed 0)
// 64-bit (8 bytes): the pointer must be a multiple of 8 (3 bits are guaranteed 0)
// We have 4 qualifiers, so we use the last 2 bits for the qualifier tagging, i.e. 11 = 3

// Input the pointer's last 2 bits with the qualifier
inline ara_index ara_tag_index(ara_index i, ara_tag q) {
    return (ara_index)( (uintptr_t)(i) | (uintptr_t)(q) );
}

// Get out the last 2 bits as the tag
inline ara_tag ara_get_tag(ara_index i) {
    return (ara_tag)( (uintptr_t)(i) & (uintptr_t)(0x3) );
}

// Get out the pointer minus the tag
inline ara_index ara_get_index(ara_index i) {
    return (ara_index)( (uintptr_t)(i) & ~((uintptr_t)(0x3)) );
}

/******************************************************************************************/

#define ARA_FUNCTION(NAME) ara_define_##NAME

/******************************************************************************************/

#ifdef __cplusplus
}
#endif

/******************************************************************************************/

#endif