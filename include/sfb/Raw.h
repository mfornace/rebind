#pragma once
#include "API.h"
#include <type_traits>

/******************************************************************************************/

// define just provides the definition based on a specified type, nothing more.
#define SFB_DEFINE(NAME, TYPE) sfb_stat SFB_FUNCTION(NAME)(sfb_input i, void* s, void* p, void* a) noexcept {return sfb::Switch<TYPE>::invoke(i,s,p,a);}

// declare declares the function with a given name
#define SFB_DECLARE(NAME) extern "C" sfb_stat SFB_FUNCTION(NAME)(sfb_input, void*, void*, void*) noexcept;

#define SFB_DELEGATE(NAME, TYPE) namespace sfb {template<> struct Lookup<TYPE> {static constexpr sfb_index invoke = SFB_FUNCTION(NAME);};}

/******************************************************************************************/

namespace sfb {

// C++ version of sfb_tags enum
enum class Mode : sfb_mode {Stack=sfb_stack, Heap=sfb_heap, Read=sfb_read, Write=sfb_write};

static char const * TagNames[4] = {"stack", "heap", "const", "mutable"};

// C++ alias of sfb_code
using Context = sfb_context;
using Code    = sfb_code;
using Stat    = sfb_stat;
using Idx     = sfb_index;
using Integer = sfb_integer;
using Float   = sfb_float;

/******************************************************************************************/

// Alias<T> is used in special cases to allow reinterpret_cast between an exposed C type
// and an exposed C++ type without disobeying strict aliasing.

template <class T>
struct AliasType {using type = T;};

template <class T>
using Alias = typename AliasType<T>::type;

/******************************************************************************************/

// is_dependent<T> marks whether a type holds a reference to something else (to do ... clarify)

template <class T>
struct Dependent : std::is_reference<T> {};

template <class T>
static constexpr bool is_dependent = Dependent<T>::value;

/******************************************************************************************/

static_assert(sizeof(sfb_input) == 2 * sizeof(sfb_code));

namespace code {

enum codes {
    check      = 0,
    destruct   = 1,
    deallocate = 2,
    copy       = 3,
    swap       = 4,
    name       = 5,
    info       = 6,
    relocate   = 7,
    call       = 8,
    dump       = 9,
    load       = 10,
    attribute  = 11,
    element    = 12,
    hash       = 13,
    compare    = 14,
    equal      = 15
    // unary
    // binary
    // print
    // increment/decrement
};

inline constexpr bool valid(Code c) {return c < 16;}

}

inline char const * code_name(Code c) {
    switch (static_cast<code::codes>(c)) {
        case code::check :      return "check";
        case code::destruct :   return "destruct";
        case code::deallocate : return "deallocate";
        case code::copy :       return "copy";
        case code::swap :       return "swap";
        case code::hash :       return "hash";
        case code::name :       return "name";
        case code::info :       return "info";
        case code::relocate :   return "relocate";
        case code::call :       return "call";
        case code::dump :       return "dump";
        case code::load :       return "load";
        case code::attribute :  return "attribute";
        case code::element :    return "element";
        case code::compare :    return "compare";
        case code::equal :      return "equal";
        default:                return "unknown";
    }
}

/******************************************************************************/

struct ArgView;
union Ref;
union Target;
union Str;
union Bin;
union String;
union Binary;
union Tuple;
struct Index;
union Shape;
union Array;
union Span;
union View;

template <class T>
static constexpr bool is_implementable = true
    && !std::is_void_v<T>
    && !std::is_const_v<T>
    && !std::is_reference_v<T>
    && !std::is_volatile_v<T>
    && !std::is_same_v<T, Ref>
    && !std::is_same_v<T, Target>
    && !std::is_null_pointer_v<T>;
    // && !is_type_t<T>::value;
    // && !std::is_function_v<T>

/******************************************************************************/

template <class T>
constexpr void assert_implementable() {
    // static_assert(std::is_nothrow_destructible_v<T>);
    static_assert(!std::is_void_v<T>);
    static_assert(!std::is_const_v<T>);
    static_assert(!std::is_reference_v<T>);
    static_assert(!std::is_volatile_v<T>);
    static_assert(!std::is_same_v<T, Ref>);
    static_assert(!std::is_same_v<T, Target>);
    static_assert(!std::is_null_pointer_v<T>);
    // static_assert(!is_type_t<T>::value);
}

/******************************************************************************/

template <class T>
struct Lookup;

}

#define SFB_DECLARE_1(NAME, TYPE) SFB_DECLARE(NAME); SFB_DELEGATE(NAME, TYPE);
#define SFB_DECLARE_2(NAME, TYPE1, TYPE2) SFB_DECLARE(NAME); SFB_DELEGATE(NAME, TYPE1); SFB_DELEGATE(NAME, TYPE2);


SFB_DECLARE_1(void, void);
SFB_DECLARE_1(cpp_bool, bool);
SFB_DECLARE_1(bool, sfb_bool);
SFB_DECLARE_1(char, char);
SFB_DECLARE_1(uchar, unsigned char);
SFB_DECLARE_1(int, int);
SFB_DECLARE_1(long, long);
SFB_DECLARE_1(longlong, long long);
SFB_DECLARE_1(ulonglong, unsigned long long);
SFB_DECLARE_1(unsigned, unsigned);
SFB_DECLARE_1(float, float);
SFB_DECLARE_1(double, double);

SFB_DECLARE_2(str,    sfb_str,    sfb::Str);
SFB_DECLARE_2(bin,    sfb_bin,    sfb::Bin);
SFB_DECLARE_2(string, sfb_string, sfb::String   );
SFB_DECLARE_2(binary, sfb_binary, sfb::Binary   );
SFB_DECLARE_2(span,   sfb_span,   sfb::Span );
SFB_DECLARE_2(array,  sfb_array,  sfb::Array  );
SFB_DECLARE_2(tuple,  sfb_tuple,  sfb::Tuple  );
SFB_DECLARE_2(view,   sfb_view,   sfb::View );
SFB_DECLARE_2(shape,  sfb_shape,  sfb::Shape  );
SFB_DECLARE_2(index,  sfb_index,  sfb::Index  );
