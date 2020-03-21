#ifndef REBIND_RAW_H
#define REBIND_RAW_H

#ifdef __cplusplus

#include "Table.h"

using rebind_table = rebind::Table;

using rebind_index = rebind::Index;

using rebind_function = rebind::Function;

extern "C" {

#else

typedef struct rebind_table rebind_table;

typedef rebind_table const * rebind_index;

typedef struct rebind_function rebind_function;

// struct rebind_index {
//     std::aligned_storage_t<sizeof(rebind::Index), alignof(rebind::Index)> blah;
// };


#endif

/******************************************************************************************/

typedef struct rebind_ref {
    rebind_index ind;
    void *ptr;
    unsigned char qual;
} rebind_ref;

/******************************************************************************************/

typedef struct rebind_value {
    rebind_index ind;
    void *ptr;
} rebind_value;

/******************************************************************************************/

#ifdef __cplusplus
}

/******************************************************************************************/

//     // template <class T>
//     // void set(T &t) {
//     //     ind = fetch<T>();
//     //     ptr = static_cast<void *>(std::addressof(t));
//     //     return t;
//     // }

//     template <class T>
//     T * reset(T *t) noexcept {ptr = t; ind = fetch<T>(); return t;}

namespace rebind::raw {

/**************************************************************************************/

template <class T>
bool matches(Index ind, Type<T> t={}) noexcept {
    static_assert(is_usable<T>);
    return ind == fetch<unqualified<T>>();
}

template <class T>
T *target(Index i, void *p) noexcept {
    return (p && i == fetch<unqualified<T>>()) ? static_cast<T *>(p) : nullptr;
}

inline std::string_view name(Index i) noexcept {
    if (i) return i->name();
    else return "null";
}

/**************************************************************************************/

template <class T, class ...Args>
T * alloc(Args &&...args) {
    assert_usable<T>();
    if constexpr(std::is_constructible_v<T, Args &&...>) {
        return new T(static_cast<Args &&>(args)...);
    } else {
        return new T{static_cast<Args &&>(args)...};
    }
}

/**************************************************************************************/

inline bool assign_if(Index i, void *p, Ref const &r) {
    return p && i->m_assign_if(p, r);
}

/**************************************************************************************/

template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
std::remove_reference_t<T> * request(Index i, void *p, Scope &s, Type<T>, Qualifier q);

template <class T, std::enable_if_t<!std::is_reference_v<T>, int> = 0>
std::optional<T> request(Index i, void *p, Scope &s, Type<T>, Qualifier q);

bool request_to(Index t, void *p, Value &, Qualifier q);

bool request_to(Index t, void *p, Ref &, Qualifier q);

/**************************************************************************************/

}

#endif

#endif