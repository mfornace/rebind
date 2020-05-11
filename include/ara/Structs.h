#pragma once
#include "API.h"
#include <tuple>
#include <string_view>
#include <string>

namespace ara {

// "An aggregate or union type that includes one of the aforementioned types among its elements or non- static data members (including, recursively, an element or non-static data member of a subaggregate or contained union),"
// When we actually construct the String wherever we may need to instead construct ara_string and cast back (probably better to anyway)

/******************************************************************************/

// Release the given type into the raw C representation
template <class T>
auto const_slice(T const &t) {
    storage_like<T> tmp; // so that destructor not called
    new(&tmp) T(t); // throw here is OK
    return storage_cast<T>(tmp).c;
}

template <class T>
auto move_slice(T &t) noexcept {
    storage_like<T> tmp; // so that destructor not called
    new(&tmp) T(std::move(t));
    return storage_cast<T>(tmp).c;
}

/******************************************************************************/

union Str {
    using value_type = char;
    using native = std::string_view;
    using base_type = ara_str;
    ara_str c;

    constexpr Str() : c{nullptr, 0} {}
    constexpr Str(char const *data, std::size_t size) : c{data, size} {}
    constexpr Str(std::string_view s) : c{s.data(), s.size()} {}

    constexpr native view() const {return c.data ? native() : native(c.data, c.size);}
    constexpr operator native() const {return view();}

    explicit operator ara_str() && noexcept {return move_slice(*this);}
    explicit operator ara_str() const & {return const_slice(*this);}
};

static_assert(std::is_default_constructible_v<Str>);
static_assert(std::is_nothrow_move_constructible_v<Str>);
static_assert(std::is_nothrow_move_assignable_v<Str>);
static_assert(std::is_copy_constructible_v<Str>);
static_assert(std::is_copy_assignable_v<Str>);
static_assert(std::is_destructible_v<Str>);

/******************************************************************************/

// target.emplace<Str>(...): can actually emplace CStr I guess? Then it returns CStr* instead...not too bad I guess
// object.target<Str>(): ? could also return CStr*
// object.load<Str>(): would return Str
// object.load<Str &>(): probably disallow
// dump(T &, Target, Index::of<Str>()): would put CStr into Target
// dump(CStr &, Target, Index::of<T>()): acts normally
// Index::of<Str>() same as Index::of<CStr>() same as Index::of<ara_str>()

/******************************************************************************/

union Bin {
    using value_type = std::byte;
    using native = std::basic_string_view<std::byte>;
    ara_bin c;

    constexpr Bin(std::byte const *data, std::size_t size)
        : c{bit_cast<unsigned char const *>(data), size} {}

    constexpr Bin() : Bin{nullptr, 0} {}
    constexpr Bin(native s) : Bin{s.data(), s.size()} {}

    constexpr native view() const {return c.data ? native() : native(bit_cast<std::byte const *>(c.data), c.size);}

    constexpr operator native() const {return view();}

    constexpr std::basic_string_view<unsigned char> chars() const {
        return c.data ? std::basic_string_view<unsigned char>()
            : std::basic_string_view<unsigned char>(c.data, c.size);
    }

    explicit operator ara_bin() && noexcept {return move_slice(*this);}
    explicit operator ara_bin() const & {return const_slice(*this);}
};

static_assert(std::is_default_constructible_v<Bin>);
static_assert(std::is_nothrow_move_constructible_v<Bin>);
static_assert(std::is_nothrow_move_assignable_v<Bin>);
static_assert(std::is_copy_constructible_v<Bin>);
static_assert(std::is_copy_assignable_v<Bin>);
static_assert(std::is_destructible_v<Bin>);

/******************************************************************************/

union String {
    static void deallocate(char *p, std::size_t n) noexcept {delete[] p;}

    void allocate(char const *start, std::size_t size) {
        c.sbo.alloc.pointer = new char[size + 1];
        c.sbo.alloc.destructor = &deallocate;
        std::memcpy(c.sbo.alloc.pointer, start, size);
        c.sbo.alloc.pointer[size] = '\0';
    }

    ara_string c;
    String() : c{{}, 0} {}
    String(Str s);
    String(std::string_view s) : String(Str(s)) {}

    String(String &&s) noexcept {std::swap(*this, s);}
    String& operator=(String &&s) noexcept {std::swap(*this, s); return *this;}

    String(String const &s) = delete;
    // : c{s.c} { if (!sbo()) allocate(data(), size()); }
    String& operator=(String const &s) = delete;;

    ~String() noexcept {if (!sbo()) c.sbo.alloc.destructor(c.sbo.alloc.pointer, c.size);}

    bool sbo() const {return c.size < sizeof(c.sbo);}

    char* data() {return sbo() ? c.sbo.storage : c.sbo.alloc.pointer;}
    char const* data() const {return sbo() ? c.sbo.storage : c.sbo.alloc.pointer;}

    std::size_t size() const {return c.size;}

    constexpr operator Str() const {return {data(), size()};}
    constexpr operator std::string_view() const {return {data(), size()};}

    explicit operator ara_string() && noexcept {return move_slice(*this);}
    // explicit operator ara_string() const & {return const_slice(*this);}
};

static_assert(std::is_default_constructible_v<String>);
static_assert(std::is_nothrow_move_constructible_v<String>);
static_assert(std::is_nothrow_move_assignable_v<String>);
static_assert(!std::is_copy_constructible_v<String>);
static_assert(!std::is_copy_assignable_v<String>);
static_assert(std::is_destructible_v<String>);

/******************************************************************************/

union Binary {
    ara_binary c;
    Binary() : c{{}, 0} {}

    Binary(Binary &&s) noexcept {std::swap(*this, s);}
    Binary& operator=(Binary &&s) noexcept {std::swap(*this, s); return *this;}

    Binary(Binary const &) = delete;  // todo
    Binary& operator=(Binary const &s) = delete;

    ~Binary() noexcept {if (!sbo()) c.sbo.alloc.destructor(c.size, c.sbo.alloc.pointer);}

    bool sbo() const {return c.size <= sizeof(c.sbo);}

    // char* data() {return sbo() ? c.sbo.storage : c.sbo.alloc.pointer;}
    // char const* data() const {return sbo() ? c.sbo.storage : c.sbo.alloc.pointer;}

    // std::size_t size() const {return c.size;}

    // constexpr operator Bin() const {return {data(), size()};}

    explicit operator ara_binary() && noexcept {return move_slice(*this);}
    // explicit operator ara_binary() const & {return const_slice(*this);}
};

static_assert(std::is_default_constructible_v<Binary>);
static_assert(std::is_nothrow_move_constructible_v<Binary>);
static_assert(std::is_nothrow_move_assignable_v<Binary>);
static_assert(!std::is_copy_constructible_v<Binary>);
static_assert(!std::is_copy_assignable_v<Binary>);
static_assert(std::is_destructible_v<Binary>);

/******************************************************************************/

union Span {
    ara_span c;
    Span() noexcept : c{nullptr, nullptr, 1, 0, 0} {}
    Span(Span &&s) noexcept {std::swap(*this, s);}
    Span& operator=(Span &&s) noexcept {std::swap(*this, s); return *this;}

    Span(Span const &s) = delete;
    Span& operator=(Span const &) = delete;

    ~Span() noexcept {if (c.rank > 1) delete[] reinterpret_cast<size_t*>(c.shape);}

    explicit operator ara_span() && noexcept {return move_slice(*this);}
    // explicit operator ara_span() const & {return const_slice(*this);}
};

/******************************************************************************/

union Array {
    ara_array c;

    Array() : c{{nullptr, nullptr, 1, 0, 0}, nullptr, nullptr} {}
    Array(Array &&s) noexcept {std::swap(*this, s);}
    Array& operator=(Array &&s) noexcept {std::swap(*this, s); return *this;}

    Array(Array const &s) = delete;
    Array& operator=(Array const &) = delete;

    ~Array() noexcept {if (c.destructor) c.destructor(c.span.index, c.span.data);}

    explicit operator ara_array() && noexcept {return move_slice(*this);}
    // explicit operator ara_array() const & {return const_slice(*this);}
};

/******************************************************************************/

union Tuple {
    ara_tuple c;
    Tuple();

    Tuple(Tuple &&s) noexcept {std::swap(*this, s);}
    Tuple& operator=(Tuple &&s) noexcept {std::swap(*this, s); return *this;}

    Tuple(Tuple const &s) = delete;
    Tuple& operator=(Tuple const &) = delete;

    ~Tuple() noexcept {if (c.destructor) c.destructor(c.data, c.size, c.storage);}

    explicit operator ara_tuple() && noexcept {return move_slice(*this);}
    // explicit operator ara_tuple() const & {return const_slice(*this);}
};

/******************************************************************************/

union View {
    ara_view c;

    constexpr View() noexcept : c{nullptr, 0, nullptr, nullptr, nullptr} {}

    View(View &&s) noexcept {std::swap(*this, s);}
    View& operator=(View &&s) noexcept {std::swap(*this, s); return *this;}

    View(View const &s) = delete;
    View& operator=(View const &) = delete;

    ~View() noexcept {if (c.destructor) c.destructor(c.data, c.size, c.storage);}

    std::size_t size() const {
        if (!c.shape) return c.size;
        if (!c.size) return 0;
        std::size_t out = 1;
        for (auto i = c.shape; i != c.shape + c.size; ++i) out *= *i;
        return out;
    }

    Ref* begin() const {return reinterpret_cast<Ref*>(c.data);}
    Ref* end() const {return begin() + size();}

    explicit operator ara_view() && noexcept {return move_slice(*this);}
    // explicit operator ara_view() const & {return const_slice(*this);}
};

/******************************************************************************/

template <> struct AliasType<Str>    {using type = ara_str;};
template <> struct AliasType<Bin>    {using type = ara_bin;};
template <> struct AliasType<String> {using type = ara_string;};
template <> struct AliasType<Binary> {using type = ara_binary;};
template <> struct AliasType<Span>   {using type = ara_span;};
template <> struct AliasType<Array>  {using type = ara_array;};
template <> struct AliasType<Tuple>  {using type = ara_tuple;};
template <> struct AliasType<View>   {using type = ara_view;};

/******************************************************************************/

}

ARA_DECLARE(str, ara_str);
ARA_DECLARE(bin, ara_bin);
ARA_DECLARE(string, ara_string);
ARA_DECLARE(binary, ara_binary);
ARA_DECLARE(span, ara_span);
ARA_DECLARE(array, ara_array);
ARA_DECLARE(tuple, ara_tuple);
ARA_DECLARE(view, ara_view);