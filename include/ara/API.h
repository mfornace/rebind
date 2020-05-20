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
enum ara_modes {ara_stack=0, ara_heap=1, ara_read=2, ara_write=3};
typedef uint_fast8_t ara_mode;

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
    3) and the qualifier of the held type (Read, Write, Stack, Heap)
Currently the qualifier is held as a tag on the ara_index pointer.
The logic of the qualifiers is roughly that Read = T const & and Write = T &.
Stack and Heap both refer to temporary references. These are references to objects that will
be deleted soon and may be deleted manually at any time by the user of the reference.
Stack means that ~T() should be used, and Heap means that delete (or equivalent) should be used.
The logic behind tagging the index rather than the data is that maybe the data can hold
the actual value itself if the value is trivial and fits in void *.
*/
typedef struct ara_ref {
    ara_index mode_index;
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

// Contiguous container similar to a type-erased std::span, extended to N dimensions though.
// Having an ara_span lets you access data as & or const &, depending on held qualifier
// Does not let you change the shape of the array
// Cannot access data as && currently because the move can't be handled.
typedef struct ara_span {
    ara_shape shape;
    ara_index index;   // Type and qualifier of the held type, either & or const &
    void* data;        // Address to the start of the array
    uint32_t rank;     // Rank of the array
    uint32_t item;     // Item size
} ara_span;

// Contiguous container similar to a type-erased n-dimensional std::vector
// Includes all capabilities of span: access as & or const &
// Also manages the held values, so std::move(a[i]) is fine
typedef struct ara_array {
    ara_span span;
    void *storage; // destructor data, could maybe be folded into destructor in the future
    void (*destructor)(ara_index, void*); // called with previous members
} ara_array;

// Span and Array leave open the possibility of another container type which allows
// destructive moves. OTOH this is difficult, implies you would need something like a bool
// to tell if the element has been destructed ... seems out of scope.

/******************************************************************************************/

// View-like container of heterogeneous types
// Limitation: the references can only be taken as & or const &, not &&
// I suppose that's OK because the view does not handle resource destruction
// Well...they could be &&, if the allocation is held somewhere else in aligned_storage-like manner ... this is fairly niche though
// OTOH it would have been perfectly fine for C++ semantics since moves aren't destructing
// Is there a good use case for a view containing rvalues ... ? Seems like ... not really?
typedef struct ara_view {
    ara_ref* data;                               // address to the start of the array
    size_t size;                                 // if shape is given, the rank. otherwise length of array
    void (*destructor)(ara_ref*, size_t);        // called with previous members
} ara_view;

// Value container of heterogeneous types
// I suppose std::tuple<int&> is OK to hold here
// Here the references can be taken as &, const &, or && because the resources are managed
// std::tuple<int, double> can be held directly in void* because all members are trivially_destructible
// otherwise usually there will need to be an ara-managed allocation to enable destructive moves
// e.g. storage = new std::tuple<std::string>(); // = bad because can't make destructible reference
//      storage = new aligned_storage<std::tuple<std::string>>[n]; // = ok because we can handle the moves
// both involve an allocation anyway...
typedef struct ara_tuple {
    ara_ref* data;                               // address to the start of the array
    size_t size;                                 // if shape is given, number of shape. otherwise length of array
    void* storage;
    void (*destructor)(ara_ref*, size_t, void*); // called with previous members (fold in storage?)
} ara_tuple;

// Now one question is what the point of "View" is, compared to Tuple with null destructor
// I guess having a Tuple signifies it's a non-aliasing container (for most but not all usages...)
// i.e. you can return a Tuple from a function (assuming held values are not aliasing incorrectly)
// we could restrict Tuple to non-reference types but that doesn't solve non-reference alias types so not a real solution
// you can probably not return a View from a function unless you're in a very niche case (similar to returning std::string_view).
// OTOH you can return a View safely from load(std::tuple<...> const &)
// OTOH you can return a Tuple safely from load(std::tuple<...> &&) or from load(std::tuple<...> const &)
// (latter involves a copy though, so not preferred)

// I think generally you would decay tuples effectively like std::tuple<int &&> -> std::tuple<int>
// Then we only need to handle <T, T &, T const &> which is fine for Tuple
// View is ... still possible to use with std::tuple<int>
// Hmm. Just not sure where View will really be useful. All it does is obviate the destructor need.

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
    // Requested qualifier, see Target.h
    ara_code mode;
    // void* allocator_data;
    // void* (*allocator)(Code size, Code alignment) noexcept
} ara_target;

/******************************************************************************************/

// reinterpret_cast is legal C++ for interconversion of uintptr_t and void *
// 32-bit (4 bytes): the pointer must be a multiple of 4 (2 bits are guaranteed 0)
// 64-bit (8 bytes): the pointer must be a multiple of 8 (3 bits are guaranteed 0)
// We have 4 qualifiers, so we use the last 2 bits for the qualifier tagging, i.e. 11 = 3

// Input the pointer's last 2 bits with the qualifier
inline ara_index ara_mode_index(ara_index i, ara_mode q) {
    return (ara_index)( (uintptr_t)(i) | (uintptr_t)(q) );
}

// Get out the last 2 bits as the tag
inline ara_mode ara_get_mode(ara_index i) {
    return (ara_mode)( (uintptr_t)(i) & (uintptr_t)(0x3) );
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