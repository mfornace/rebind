#ifndef REBIND_API_H
#define REBIND_API_H
#include "stdint.h"

#ifdef __cplusplus

extern "C" {

#endif

typedef int rebind_stat;

typedef unsigned int rebind_tag;

typedef void * rebind_ptr;

typedef struct rebind_args rebind_args;

/******************************************************************************************/

typedef rebind_stat (*rebind_index)(rebind_tag t, void *a, void *b, rebind_args);

/******************************************************************************************/

typedef struct rebind_ref {
    rebind_index ind;
    rebind_ptr tagged_ptr;
    // unsigned char qual;
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

#ifdef __cplusplus
}

#include <type_traits>

namespace rebind {

using Index = rebind_index;
using Tag = rebind_tag;
using Stat = rebind_stat;

namespace tag {
    static constexpr Tag const
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

namespace stat {
    enum class copy : Stat {ok, unavailable, exception};
    enum class drop : Stat {ok, unavailable};
    enum class info : Stat {ok, unavailable};
    enum class from_ref : Stat {ok, none, exception, unavailable};
    enum class request : Stat {ok, none, null, exception, unavailable};
    enum class assign_if : Stat {ok, none, null, exception, unavailable};
    enum class call : Stat {ok, exception, unavailable, wrong_number, wrong_type};

    template <class T>
    Stat put(T t) {
        static_assert(std::is_enum_v<T>);
        return static_cast<Stat>(t);
    }
    // enum class assign_if {error};
}


inline char const * tag_name(rebind_tag t) {
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

}

#endif

#endif