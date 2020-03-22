#pragma once
#include <rebind/Value.h>
#include <type_traits>

// #define REBINDC(name) rebind_##name
// #define REBINDM(name, method) rebind_##name##_##method

// static_assert(std::is_pod_v<std::pair<std::type_info const *, rebind::Qualifier>>);
// static_assert(std::is_trivial_v<rebind::Index>);
// static_assert(std::is_pod_v<rebind::Index>);
extern "C" {

/******************************************************************************/

rebind_table* rebind_table_emplace(rebind_index);

void rebind_table_set(rebind_table*,
    rebind::CTable::destroy_t,
    rebind::CTable::copy_t,
    rebind::CTable::to_value_t,
    rebind::CTable::to_ref_t,
    rebind::CTable::assign_if_t,
    rebind::CTable::from_ref_t,
    char const *
);

void rebind_table_add_method(rebind_table*, char const *, rebind_value *);

void rebind_table_add_base(rebind_table*, rebind_index);

/******************************************************************************/

// static_assert(std::integral_constant<std::size_t, sizeof(rebind::Value)>::aaa); //16
// static_assert(std::integral_constant<std::size_t, alignof(rebind::Value)>::aaa); //8
// static_assert(std::integral_constant<std::size_t, sizeof(rebind::Ref)>::aaa); //24
// static_assert(std::integral_constant<std::size_t, alignof(rebind::Ref)>::aaa); //8

static_assert(std::is_standard_layout_v<rebind::Ref>);
static_assert(std::is_trivially_copyable_v<rebind::Ref>);
static_assert(std::is_trivially_copy_assignable_v<rebind::Ref>);
static_assert(std::is_trivially_move_assignable_v<rebind::Ref>);
static_assert(std::is_trivially_destructible_v<rebind::Ref>);


static_assert(!std::is_trivially_default_constructible_v<rebind::Ref>);
static_assert(!std::is_trivial_v<rebind::Ref>);


// Destructor
void rebind_ref_destruct(rebind_ref* x); // noexcept

// Copy constructor
bool rebind_value_copy(rebind_value *v, rebind_value const *o); // noexcept

// Move constructor
void rebind_ref_move(rebind_ref*, rebind_ref* v); // noexcept

// Method call
rebind_ref* rebind_ref_method(rebind_ref *v, char const *c, rebind_ref *, int n);

/******************************************************************************/

void rebind_value_destruct(rebind_value *x);

// Default constructor
// rebind_value* REBINDM(Value, new)();

// Move constructor
rebind_value* rebind_value_move(rebind_value *v);

// Method call
// rebind_value* REBINDM(Value, method)(rebind_value *v, char const *c, Ref *, int n);

/******************************************************************************/

// rebind_value* rebind_value_move(rebind_value *v);

// static_assert(std::is_standard_layout_v<rebind::Index>);
// static_assert(std::is_trivially_copyable_v<rebind::Qualifier>);
// static_assert(std::is_trivially_copyable_v<std::type_info const *>);
// static_assert(std::is_trivially_copyable_v<std::pair<std::type_info const *, rebind::Qualifier>>);
// static_assert(std::is_trivially_copyable_v<rebind::Index>);

// rebind_index rebind_value_index(rebind_value const *v);

char const * rebind_index_name(rebind_index v);


}
