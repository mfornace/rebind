#pragma once
#include <rebind/Variable.h>

// static_assert(std::is_pod_v<std::pair<std::type_info const *, rebind::Qualifier>>);
// static_assert(std::is_trivial_v<rebind::TypeIndex>);
// static_assert(std::is_pod_v<rebind::TypeIndex>);
extern "C" {

typedef struct rebind_variable rebind_variable;
typedef struct rebind_type_index rebind_type_index;

void rebind_destruct(rebind_variable *x);

rebind_variable * rebind_variable_new();

rebind_variable * rebind_variable_copy(rebind_variable *v);


// static_assert(std::is_standard_layout_v<rebind::TypeIndex>);
// static_assert(std::is_trivially_copyable_v<rebind::Qualifier>);
// static_assert(std::is_trivially_copyable_v<std::type_info const *>);
// static_assert(std::is_trivially_copyable_v<std::pair<std::type_info const *, rebind::Qualifier>>);
// static_assert(std::is_trivially_copyable_v<rebind::TypeIndex>);

struct rebind_type_index {
    std::aligned_storage_t<sizeof(rebind::TypeIndex), alignof(rebind::TypeIndex)> blah;
};

rebind_type_index rebind_variable_type(rebind_variable *v);

char const * rebind_type_index_name(rebind_type_index v);

int rebind_add();


}