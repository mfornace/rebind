#pragma once
#include <rebind/Value.h>

#define REBINDC(name) rebind_##name
#define REBINDM(name, method) rebind_##name##_##method

// static_assert(std::is_pod_v<std::pair<std::type_info const *, rebind::Qualifier>>);
// static_assert(std::is_trivial_v<rebind::Index>);
// static_assert(std::is_pod_v<rebind::Index>);
extern "C" {

/******************************************************************************/

// static_assert(std::integral_constant<std::size_t, sizeof(rebind::Value)>::aaa); //16
// static_assert(std::integral_constant<std::size_t, alignof(rebind::Value)>::aaa); //8
// static_assert(std::integral_constant<std::size_t, sizeof(rebind::Ref)>::aaa); //24
// static_assert(std::integral_constant<std::size_t, alignof(rebind::Ref)>::aaa); //8

static_assert(std::is_standard_layout_v<rebind::Erased>);
static_assert(!std::is_standard_layout_v<rebind::Ref>);

static_assert(std::is_trivially_copyable_v<rebind::Ref>);
static_assert(std::is_trivially_copy_assignable_v<rebind::Ref>);
static_assert(std::is_trivially_move_assignable_v<rebind::Ref>);
static_assert(std::is_trivially_destructible_v<rebind::Ref>);

static_assert(!std::is_trivially_default_constructible_v<rebind::Ref>);
static_assert(!std::is_trivial_v<rebind::Ref>);
static_assert(!std::is_trivial_v<rebind::Erased>);

typedef struct REBINDC(Ref) REBINDC(Ref);

// Destructor
void REBINDM(Ref, destruct)(REBINDC(Ref)* x); // noexcept

// Default constructor
void REBINDM(Ref, new)(REBINDC(Ref)*); // noexcept

// Copy constructor
void REBINDM(Ref, copy)(REBINDC(Ref)*, REBINDC(Ref)* v); // noexcept

// Move constructor
void REBINDM(Ref, move)(REBINDC(Ref)*, REBINDC(Ref)* v); // noexcept

// Method call
REBINDC(Ref)* REBINDM(Ref, method)(REBINDC(Ref) *v, char const *c, REBINDC(Ref) *, int n);

/******************************************************************************/

typedef struct REBINDC(Value) REBINDC(Value);

void REBINDM(Value, destruct)(REBINDC(Value) *x);

// Default constructor
REBINDC(Value)* REBINDM(Value, new)();

// Copy constructor (may throw)
REBINDC(Value)* REBINDM(Value, copy)(REBINDC(Value) *v);

// Move constructor
REBINDC(Value)* REBINDM(Value, move)(REBINDC(Value) *v);

// Method call
// REBINDC(Value)* REBINDM(Value, method)(REBINDC(Value) *v, char const *c, Ref *, int n);

/******************************************************************************/

// REBINDC(Value)* REBINDM(Value, move)(REBINDC(Value) *v);

// static_assert(std::is_standard_layout_v<rebind::Index>);
// static_assert(std::is_trivially_copyable_v<rebind::Qualifier>);
// static_assert(std::is_trivially_copyable_v<std::type_info const *>);
// static_assert(std::is_trivially_copyable_v<std::pair<std::type_info const *, rebind::Qualifier>>);
// static_assert(std::is_trivially_copyable_v<rebind::Index>);

typedef struct REBINDC(Index) REBINDC(Index);

struct REBINDC(Index) {
    std::aligned_storage_t<sizeof(rebind::Index), alignof(rebind::Index)> blah;
};

REBINDC(Index) REBINDC(Value_type)(REBINDC(Value) *v);

char const * REBINDC(TypeIndex_name)(REBINDC(Index) v);

int rebind_add();


}