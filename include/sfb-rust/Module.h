#pragma once
#include <sfb/Value.h>
#include <type_traits>

// #define SFBC(name) sfb_##name
// #define SFBM(name, method) sfb_##name##_##method

// static_assert(std::is_pod_v<std::pair<std::type_info const *, sfb::Qualifier>>);
// static_assert(std::is_trivial_v<sfb::Index>);
// static_assert(std::is_pod_v<sfb::Index>);
extern "C" {

typedef int sfb_bool;

/******************************************************************************/

struct sfb_str {
    std::uint8_t const *data;
    unsigned long long size;
};

/******************************************************************************/

struct sfb_args {
    sfb_ref *data;
    unsigned long long size;
};

/******************************************************************************/

sfb_index sfb_table_emplace(
    sfb_str,
    void const *,
    sfb::CTable::drop_t,
    sfb::CTable::copy_t,
    sfb::CTable::to_value_t,
    sfb::CTable::to_ref_t,
    sfb::CTable::assign_if_t,
    sfb::CTable::from_ref_t
);

sfb_bool sfb_init() noexcept;

// void sfb_table_add_method(sfb_table*, char const *, sfb_value *);

// void sfb_table_add_base(sfb_table*, sfb_index);

/******************************************************************************/

// static_assert(std::integral_constant<std::size_t, sizeof(sfb::Value)>::aaa); //16
// static_assert(std::integral_constant<std::size_t, alignof(sfb::Value)>::aaa); //8
// static_assert(std::integral_constant<std::size_t, sizeof(sfb::Ref)>::aaa); //24
// static_assert(std::integral_constant<std::size_t, alignof(sfb::Ref)>::aaa); //8

static_assert(std::is_standard_layout_v<sfb::Ref>);
static_assert(std::is_trivially_copyable_v<sfb::Ref>);
static_assert(std::is_trivially_copy_assignable_v<sfb::Ref>);
static_assert(std::is_trivially_move_assignable_v<sfb::Ref>);
static_assert(std::is_trivially_destructible_v<sfb::Ref>);


static_assert(!std::is_trivially_default_constructible_v<sfb::Ref>);
static_assert(!std::is_trivial_v<sfb::Ref>);


// Destructor
void sfb_ref_drop(sfb_ref* x) noexcept; // noexcept

// Copy constructor
sfb_bool sfb_value_copy(sfb_value *v, sfb_value const *o) noexcept; // noexcept

// Move constructor
// void sfb_ref_move(sfb_ref*, sfb_ref* v); // noexcept

// Method call
sfb_ref* sfb_ref_method(sfb_ref *v, char const *c, sfb_ref *, int n) noexcept;

/******************************************************************************/

void sfb_value_drop(sfb_value *x) noexcept;

// Default constructor
// sfb_value* SFBM(Value, new)();

// Move constructor
// sfb_value * sfb_value_move(sfb_value *v);

// Method call
// sfb_value* SFBM(Value, method)(sfb_value *v, char const *c, Ref *, int n);

/******************************************************************************/

// sfb_value* sfb_value_move(sfb_value *v);

// static_assert(std::is_standard_layout_v<sfb::Index>);
// static_assert(std::is_trivially_copyable_v<sfb::Qualifier>);
// static_assert(std::is_trivially_copyable_v<std::type_info const *>);
// static_assert(std::is_trivially_copyable_v<std::pair<std::type_info const *, sfb::Qualifier>>);
// static_assert(std::is_trivially_copyable_v<sfb::Index>);

// sfb_index sfb_value_index(sfb_value const *v);

char const * sfb_index_name(sfb_index v) noexcept;

/******************************************************************************/

// pub fn sfb_lookup(name: StrView) -> &'static Value;
sfb_value const * sfb_lookup(sfb_str s) noexcept;

sfb_index sfb_table_insert() noexcept;

sfb_bool sfb_value_call_value(sfb_value *out, sfb_value const *v, sfb_args a) noexcept;

/******************************************************************************/

}
