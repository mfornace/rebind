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

    Str(char const *data) : c{data, std::strlen(data)} {}

    constexpr Str(char const *data, std::size_t size) : c{data, size} {}
    constexpr Str(std::string_view s) : c{s.data(), s.size()} {}

    constexpr native view() const {return c.data ? native(c.data, c.size) : native();}
    constexpr operator native() const {return view();}

    std::size_t size() const {return c.size;}
    char const* data() const {return c.data;}

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
    static void deallocate(char *p, std::size_t) noexcept {delete[] p;}

    void allocate_in_place(char const *start, std::size_t n) {
        if (n) std::memcpy(c.sbo.storage, start, n);
        c.sbo.storage[n] = '\0';
        c.size = n;
    }

    void allocate(char const *start, std::size_t n) {
        c.sbo.alloc.pointer = new char[n + 1];
        c.sbo.alloc.destructor = &deallocate;
        std::memcpy(c.sbo.alloc.pointer, start, n);
        c.sbo.alloc.pointer[n] = '\0';
        c.size = n;
    }

    void overwrite(char const *data, std::size_t n) {
        if (n < sizeof(c.sbo)) allocate_in_place(data, n);
        else allocate(data, n);
        DUMP("new string", size(), this->data());
    }

    ara_string c;

    String(char const *data, std::size_t n) {overwrite(data, n);}

    String() noexcept : String{{}, 0} {c.sbo.storage[0] = '\0';}

    String(Str s) : String(s.data(), s.size()) {}
    String(std::string_view s) : String(s.data(), s.size()) {}
    String(char const *data) : String(std::string_view(data)) {}

    // template <class Alloc>
    // String(std::basic_string<char, std::char_traits<char>, Alloc> &&t) {
    //     if (t.size() < sizeof(c.sbo)) allocate_in_place(t.data(), t.size());
    //     else {

    //     }
    // }

    String(String &&s) noexcept : String() {std::swap(c, s.c);}
    String& operator=(String &&s) noexcept {std::swap(c, s.c); return *this;}

    String(String const &s) : String(s.data(), s.size()) {}

    String& operator=(String const &s) {
        reset();
        overwrite(s.data(), s.size());
        return *this;
    }

    ~String() noexcept {
        DUMP("sbo", sbo(), size());
        if (!sbo()) {
            DUMP(reinterpret_cast<void const *>(c.sbo.alloc.destructor));
            c.sbo.alloc.destructor(c.sbo.alloc.pointer, c.size);
        }
    }

    void reset() noexcept {this->~String(); c.size = 0;}

    bool sbo() const {return c.size < sizeof(c.sbo);}

    char* data() {return sbo() ? c.sbo.storage : c.sbo.alloc.pointer;}
    char const* data() const {return sbo() ? c.sbo.storage : c.sbo.alloc.pointer;}

    std::size_t size() const {return c.size;}

    constexpr operator Str() const {return {data(), size()};}
    constexpr operator std::string_view() const {return {data(), size()};}

    explicit operator ara_string() && noexcept {return move_slice(*this);}
    explicit operator ara_string() const & {return const_slice(*this);}
};

static_assert(std::is_default_constructible_v<String>);
static_assert(std::is_nothrow_move_constructible_v<String>);
static_assert(std::is_nothrow_move_assignable_v<String>);
static_assert(std::is_copy_constructible_v<String>);
static_assert(std::is_copy_assignable_v<String>);
static_assert(std::is_destructible_v<String>);

/******************************************************************************/

union Binary {
    ara_binary c;
    Binary() : c{{}, 0} {}
    Binary(Bin s) : Binary() {}

    Binary(Binary &&s) noexcept : Binary() {std::swap(c, s.c);}
    Binary& operator=(Binary &&s) noexcept {std::swap(c, s.c); return *this;}

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
    static void deallocate(std::size_t* p, std::uint32_t rank) {delete[] p;}

    ara_span c;
    Span() noexcept : c{{0, 0}, nullptr, nullptr, 0, 0} {}

    Span(Span &&s) noexcept : Span() {std::swap(c, s.c);}
    Span& operator=(Span &&s) noexcept {std::swap(c, s.c); return *this;}

    Span(void* data, std::size_t size, Tagged<Tag> i, std::uint32_t item) noexcept
        : c{{size, 0}, i.base, data, 1, item} {}

    Span(void* data, std::size_t* size, Tagged<Tag> i, std::uint32_t item, std::uint32_t rank)
        : c{{}, i.base, data, rank, item} {
            if (rank < 3) {
                std::copy(size, size+rank, c.shape.stack);
            } else {
                c.shape.alloc.dimensions = new std::size_t[rank];
                c.shape.alloc.destructor = &deallocate;
                std::copy(size, size+rank, c.shape.alloc.dimensions);
            }
        }

    template <class T>
    Span(T const* data, std::size_t size) noexcept
        : c{{size, 0},
            Tagged<Tag>(Index::of<T>(), std::is_const_v<T> ? Tag::Const : Tag::Mutable).base,
            const_cast<void*>(static_cast<void const *>(data)),
            1, sizeof(T)} {}

    Span(Span const &s) = delete;
    Span& operator=(Span const &) = delete;

    ~Span() noexcept {
        if (c.rank > 2) c.shape.alloc.destructor(c.shape.alloc.dimensions, c.rank);
    }

    explicit operator ara_span() && noexcept {return move_slice(*this);}
    // explicit operator ara_span() const & {return const_slice(*this);}
};

/******************************************************************************/

union Array {
    ara_array c;

    Array() : c{{{0, 0}, nullptr, nullptr, 1, 0}, nullptr, nullptr} {}
    Array(Array &&s) noexcept : Array() {std::swap(c, s.c);}
    Array& operator=(Array &&s) noexcept {std::swap(c, s.c); return *this;}

    Array(Array const &s) = delete;
    Array& operator=(Array const &) = delete;

    ~Array() noexcept {if (c.destructor) c.destructor(c.span.index, c.span.data);}

    explicit operator ara_array() && noexcept {return move_slice(*this);}
};

/******************************************************************************/

union View {
    ara_view c;

    constexpr View() noexcept : c{nullptr, 0, nullptr} {}

    View(View &&s) noexcept : View() {std::swap(c, s.c);}
    View& operator=(View &&s) noexcept {std::swap(c, s.c); return *this;}

    View(View const &s) = delete;
    View& operator=(View const &) = delete;

    ~View() noexcept {if (c.destructor) c.destructor(c.data, c.size);}

    std::size_t size() const {return c.size;}

    Ref* begin() const {return reinterpret_cast<Ref*>(c.data);}
    Ref* end() const {return begin() + size();}

    explicit operator ara_view() && noexcept {return move_slice(*this);}
};

/******************************************************************************/

union Tuple {
    ara_tuple c;
    constexpr Tuple() noexcept : c{nullptr, 0, nullptr, nullptr} {}

    Tuple(Tuple &&s) noexcept : Tuple() {std::swap(c, s.c);}
    Tuple& operator=(Tuple &&s) noexcept {std::swap(c, s.c); return *this;}

    Tuple(Tuple const &s) = delete;
    Tuple& operator=(Tuple const &) = delete;

    ~Tuple() noexcept {if (c.destructor) c.destructor(c.data, c.size, c.storage);}

    explicit operator ara_tuple() && noexcept {return move_slice(*this);}
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

template <class Mod>
struct Module {
    static void init(Caller caller={}) {
        parts::call<void, 0>(fetch(Type<Mod>()), Tag::Const, Pointer::from(nullptr), caller);
    }

    template <class T, int N=1, class ...Ts>
    static T call(Str name, Caller caller, Ts&& ...ts) {
        return parts::call<T, N>(fetch(Type<Mod>()), Tag::Const, Pointer::from(nullptr), caller, name, std::forward<Ts>(ts)...);
    }

    template <class T, int N=1, class ...Ts>
    static T get(Str name, Caller caller, Ts&& ...ts) {
        return parts::get<T, N>(fetch(Type<Mod>()), Tag::Const, Pointer::from(nullptr), caller, name, std::forward<Ts>(ts)...);
    }
};

/******************************************************************************************/

}

ARA_DECLARE(str, ara_str);
ARA_DECLARE(bin, ara_bin);
ARA_DECLARE(string, ara_string);
ARA_DECLARE(binary, ara_binary);
ARA_DECLARE(span, ara_span);
ARA_DECLARE(array, ara_array);
ARA_DECLARE(tuple, ara_tuple);
ARA_DECLARE(view, ara_view);