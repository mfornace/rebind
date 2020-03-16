#pragma once
#include <rebind/Value.h>

#define REBINDC(name) rebind_##name

// static_assert(std::is_pod_v<std::pair<std::type_info const *, rebind::Qualifier>>);
// static_assert(std::is_trivial_v<rebind::Index>);
// static_assert(std::is_pod_v<rebind::Index>);
extern "C" {

typedef struct REBINDC(Value) REBINDC(Value);
typedef struct REBINDC(Index) REBINDC(Index);

void rebind_destruct(REBINDC(Value) *x);

REBINDC(Value) * REBINDC(Value_new)();

REBINDC(Value) * REBINDC(Value_copy)(REBINDC(Value) *v);


// static_assert(std::is_standard_layout_v<rebind::Index>);
// static_assert(std::is_trivially_copyable_v<rebind::Qualifier>);
// static_assert(std::is_trivially_copyable_v<std::type_info const *>);
// static_assert(std::is_trivially_copyable_v<std::pair<std::type_info const *, rebind::Qualifier>>);
// static_assert(std::is_trivially_copyable_v<rebind::Index>);

struct REBINDC(Index) {
    std::aligned_storage_t<sizeof(rebind::Index), alignof(rebind::Index)> blah;
};

REBINDC(Index) REBINDC(Value_type)(REBINDC(Value) *v);

char const * REBINDC(TypeIndex_name)(REBINDC(Index) v);

int rebind_add();


}