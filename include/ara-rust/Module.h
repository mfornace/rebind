#pragma once
#include <ara/Value.h>
#include <type_traits>

// #define ARAC(name) ara_##name
// #define ARAM(name, method) ara_##name##_##method

// static_assert(std::is_pod_v<std::pair<std::type_info const *, ara::Qualifier>>);
// static_assert(std::is_trivial_v<ara::Index>);
// static_assert(std::is_pod_v<ara::Index>);
extern "C" {

typedef int ara_bool;

/******************************************************************************/

struct ara_str {
    std::uint8_t const *data;
    unsigned long long size;
};

/******************************************************************************/

struct ara_args {
    ara_ref *data;
    unsigned long long size;
};

/******************************************************************************/

ara_index ara_table_emplace(
    ara_str,
    void const *,
    ara::CTable::drop_t,
    ara::CTable::copy_t,
    ara::CTable::to_value_t,
    ara::CTable::to_ref_t,
    ara::CTable::assign_if_t,
    ara::CTable::from_ref_t
);

ara_bool ara_init() noexcept;

// void ara_table_add_method(ara_table*, char const *, ara_value *);

// void ara_table_add_base(ara_table*, ara_index);

/******************************************************************************/

// static_assert(std::integral_constant<std::size_t, sizeof(ara::Value)>::aaa); //16
// static_assert(std::integral_constant<std::size_t, alignof(ara::Value)>::aaa); //8
// static_assert(std::integral_constant<std::size_t, sizeof(ara::Ref)>::aaa); //24
// static_assert(std::integral_constant<std::size_t, alignof(ara::Ref)>::aaa); //8

static_assert(std::is_standard_layout_v<ara::Ref>);
static_assert(std::is_trivially_copyable_v<ara::Ref>);
static_assert(std::is_trivially_copy_assignable_v<ara::Ref>);
static_assert(std::is_trivially_move_assignable_v<ara::Ref>);
static_assert(std::is_trivially_destructible_v<ara::Ref>);


static_assert(!std::is_trivially_default_constructible_v<ara::Ref>);
static_assert(!std::is_trivial_v<ara::Ref>);


// Destructor
void ara_ref_drop(ara_ref* x) noexcept; // noexcept

// Copy constructor
ara_bool ara_value_copy(ara_value *v, ara_value const *o) noexcept; // noexcept

// Move constructor
// void ara_ref_move(ara_ref*, ara_ref* v); // noexcept

// Method call
ara_ref* ara_ref_method(ara_ref *v, char const *c, ara_ref *, int n) noexcept;

/******************************************************************************/

void ara_value_drop(ara_value *x) noexcept;

// Default constructor
// ara_value* ARAM(Value, new)();

// Move constructor
// ara_value * ara_value_move(ara_value *v);

// Method call
// ara_value* ARAM(Value, method)(ara_value *v, char const *c, Ref *, int n);

/******************************************************************************/

// ara_value* ara_value_move(ara_value *v);

// static_assert(std::is_standard_layout_v<ara::Index>);
// static_assert(std::is_trivially_copyable_v<ara::Qualifier>);
// static_assert(std::is_trivially_copyable_v<std::type_info const *>);
// static_assert(std::is_trivially_copyable_v<std::pair<std::type_info const *, ara::Qualifier>>);
// static_assert(std::is_trivially_copyable_v<ara::Index>);

// ara_index ara_value_index(ara_value const *v);

char const * ara_index_name(ara_index v) noexcept;

/******************************************************************************/

// pub fn ara_lookup(name: StrView) -> &'static Value;
ara_value const * ara_lookup(ara_str s) noexcept;

ara_index ara_table_insert() noexcept;

ara_bool ara_value_call_value(ara_value *out, ara_value const *v, ara_args a) noexcept;

/******************************************************************************/

}
