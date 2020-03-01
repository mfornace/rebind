#pragma once
#include <rebind/Value.h>

#define REBINDC(name) rebind_##name

// static_assert(std::is_pod_v<std::pair<std::type_info const *, rebind::Qualifier>>);
// static_assert(std::is_trivial_v<rebind::TypeIndex>);
// static_assert(std::is_pod_v<rebind::TypeIndex>);
extern "C" {

typedef struct REBINDC(Value) REBINDC(Value);
typedef struct REBINDC(TypeIndex) REBINDC(TypeIndex);

void rebind_destruct(REBINDC(Value) *x);

REBINDC(Value) * REBINDC(Value_new)();

REBINDC(Value) * REBINDC(Value_copy)(REBINDC(Value) *v);


// static_assert(std::is_standard_layout_v<rebind::TypeIndex>);
// static_assert(std::is_trivially_copyable_v<rebind::Qualifier>);
// static_assert(std::is_trivially_copyable_v<std::type_info const *>);
// static_assert(std::is_trivially_copyable_v<std::pair<std::type_info const *, rebind::Qualifier>>);
// static_assert(std::is_trivially_copyable_v<rebind::TypeIndex>);

struct REBINDC(TypeIndex) {
    std::aligned_storage_t<sizeof(rebind::TypeIndex), alignof(rebind::TypeIndex)> blah;
};

REBINDC(TypeIndex) REBINDC(Value_type)(REBINDC(Value) *v);

char const * REBINDC(TypeIndex_name)(REBINDC(TypeIndex) v);

int rebind_add();


}