#ifndef REBIND_API_H
#define REBIND_API_H
#include "stdint.h"

#ifdef __cplusplus

extern "C" {

#endif

typedef int rebind_bool;

typedef unsigned int rebind_tag;

typedef void * rebind_ptr;

typedef struct rebind_args rebind_args;

/******************************************************************************************/

typedef rebind_bool (*rebind_index)(rebind_tag t, void *a, void *b, rebind_args);

/******************************************************************************************/

typedef struct rebind_ref {
    rebind_index ind;
    rebind_ptr tagged_ptr;
    // unsigned char qual;
} rebind_ref;

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

#ifdef __cplusplus
}

namespace rebind {

using Index = rebind_index;
using Tag = rebind_tag;

namespace tag {

static constexpr unsigned int
    check = 0,
    drop = 1,
    copy = 2,
    name = 3,
    info = 4,
    method_to_value = 5,
    method_to_ref = 6,
    call_to_value = 7,
    call_to_ref = 8,
    request_to_value = 9,
    request_to_ref = 10,
    assign_to = 11;
}

}


#endif

#endif