#pragma once
#include "Ref.h"
#include <tuple>
#include <string_view>
#include <string>
#include <array>

namespace sfb {

// "An aggregate or union type that includes one of the aforementioned types among its elements or non- static data members (including, recursively, an element or non-static data member of a subaggregate or contained union),"
// When we actually construct the String wherever we may need to instead construct sfb_string and cast back (probably better to anyway)

/******************************************************************************/

union Bool {
    sfb_bool c;
    explicit constexpr Bool(bool b) : c{b} {}

    Bool& operator=(bool b) noexcept {c.value = b; return *this;}

    explicit operator sfb_bool() const noexcept {return c;}
    explicit operator bool() const noexcept {return c.value;}
};

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
    using base_type = sfb_str;
    sfb_str c;

    constexpr Str() : c{nullptr, 0} {}

    Str(char const *data) : c{data, std::strlen(data)} {}

    constexpr Str(char const *data, std::size_t size) : c{data, size} {}
    constexpr Str(std::string_view s) : c{s.data(), s.size()} {}

    constexpr native view() const {return c.data ? native(c.data, c.size) : native();}
    constexpr operator native() const {return view();}

    constexpr std::size_t size() const {return c.size;}
    constexpr char const* data() const {return c.data;}

    operator sfb_str() const & noexcept {return c;}
};

static_assert(std::is_default_constructible_v<Str>);
static_assert(std::is_nothrow_move_constructible_v<Str>);
static_assert(std::is_nothrow_move_assignable_v<Str>);
static_assert(std::is_copy_constructible_v<Str>);
static_assert(std::is_copy_assignable_v<Str>);
static_assert(std::is_destructible_v<Str>);

/******************************************************************************/

union Bin {
    using value_type = std::byte;
    using native = std::basic_string_view<std::byte>;
    sfb_bin c;

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

    operator sfb_bin() const & {return c;}
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

    void allocate_stack(char const *start, std::size_t n) {
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
        if (n < sizeof(c.sbo)) allocate_stack(data, n);
        else allocate(data, n);
        DUMP("new string", size(), this->data());
    }

    sfb_string c;

    String(char const *data, std::size_t n) {overwrite(data, n);}

    String() noexcept : String{{}, 0} {c.sbo.storage[0] = '\0';}

    String(Str s) : String(s.data(), s.size()) {}
    String(std::string_view s) : String(s.data(), s.size()) {}
    String(char const *data) : String(std::string_view(data)) {}

    // template <class Alloc>
    // String(std::basic_string<char, std::char_traits<char>, Alloc> &&t) {
    //     if (t.size() < sizeof(c.sbo)) allocate_stack(t.data(), t.size());
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

    constexpr bool sbo() const {return c.size < sizeof(c.sbo);}

    constexpr char* data() {return sbo() ? c.sbo.storage : c.sbo.alloc.pointer;}
    constexpr char const* data() const {return sbo() ? c.sbo.storage : c.sbo.alloc.pointer;}

    constexpr std::size_t size() const {return c.size;}

    constexpr operator Str() const {return {data(), size()};}
    constexpr operator std::string_view() const {return {data(), size()};}

    explicit operator sfb_string() && noexcept {return move_slice(*this);}
    explicit operator sfb_string() const & {return const_slice(*this);}
};

static_assert(std::is_default_constructible_v<String>);
static_assert(std::is_nothrow_move_constructible_v<String>);
static_assert(std::is_nothrow_move_assignable_v<String>);
static_assert(std::is_copy_constructible_v<String>);
static_assert(std::is_copy_assignable_v<String>);
static_assert(std::is_destructible_v<String>);

/******************************************************************************/

union Binary {
    sfb_binary c;
    Binary() : c{{}, 0} {}
    Binary(Bin s) : Binary() {}

    Binary(Binary &&s) noexcept : Binary() {std::swap(c, s.c);}
    Binary& operator=(Binary &&s) noexcept {std::swap(c, s.c); return *this;}

    Binary(Binary const &) = delete;  // todo
    Binary& operator=(Binary const &s) = delete;

    ~Binary() noexcept {if (!sbo()) c.sbo.alloc.destructor(c.size, c.sbo.alloc.pointer);}

    constexpr bool sbo() const {return c.size <= sizeof(c.sbo);}

    std::byte* data() {return reinterpret_cast<std::byte*>(sbo() ? c.sbo.storage : c.sbo.alloc.pointer);}
    std::byte const* data() const {return reinterpret_cast<std::byte const *>(sbo() ? c.sbo.storage : c.sbo.alloc.pointer);}

    constexpr std::size_t size() const {return c.size;}
    operator Bin() const {return {data(), size()};}

    explicit operator sfb_binary() && noexcept {return move_slice(*this);}
    // explicit operator sfb_binary() const & {return const_slice(*this);}
};

static_assert(std::is_default_constructible_v<Binary>);
static_assert(std::is_nothrow_move_constructible_v<Binary>);
static_assert(std::is_nothrow_move_assignable_v<Binary>);
static_assert(!std::is_copy_constructible_v<Binary>);
static_assert(!std::is_copy_assignable_v<Binary>);
static_assert(std::is_destructible_v<Binary>);

/******************************************************************************/

union Shape {
    sfb_shape c;

    static constexpr sfb_shape Empty{0, (1 << 1) + 1};
    constexpr Shape() noexcept : c{Empty} {}

    constexpr Shape(std::size_t size) noexcept : c{size, (1 << 1) + 1} {}

    template <class B, class E>
    Shape(B begin, E end, bool row_major) {
        c.rank = std::distance(begin, end);
        if (rank() == 0) {
            throw std::invalid_argument("rank 0 not supported");
        } else if (rank() == 1) {
            c.dims.stack = *begin;
        } else if (rank() > 1) {
            c.dims.alloc = sfb_dims_allocate(rank());
            if (!c.dims.alloc) throw std::bad_alloc();
            std::copy(begin, end, c.dims.alloc+1);
            c.dims.alloc[0] = 1;
            for (auto d = c.dims.alloc+1; d != c.dims.alloc+1+rank(); ++d)
                c.dims.alloc[0] *= *d; // not really worth the #include for accumulate
        }
    }

    Shape(std::initializer_list<std::size_t> const &l, bool row_major)
        : Shape(l.begin(), l.end(), row_major) {}

    Shape(Shape &&s) noexcept : Shape() {std::swap(c, s.c);}
    Shape& operator=(Shape &&s) noexcept {std::swap(c, s.c); return *this;}

    ~Shape() noexcept {
        DUMP("deleting shape");
        if (rank() > 1) {
            DUMP("rank size dim0 dim1", rank(), c.dims.alloc[0], c.dims.alloc[1], c.dims.alloc[2]);
            sfb_dims_deallocate(c.dims.alloc, rank());
        }
    }

    /**************************************************************************/

    constexpr std::size_t operator[](std::size_t i) const noexcept {return data()[i];}
    constexpr std::size_t& operator[](std::size_t i) noexcept {return data()[i];}

    constexpr std::size_t rank() const noexcept {return c.rank;}

    constexpr std::size_t const* data() const noexcept {
        return rank() < 2 ? &c.dims.stack : c.dims.alloc + 1;
    }

    constexpr std::size_t const* begin() const noexcept {return data();}
    constexpr std::size_t const* end() const noexcept {return data() + rank();}


    constexpr std::size_t* data() noexcept {
        return rank() < 2 ? &c.dims.stack : c.dims.alloc + 1;
    }

    constexpr std::size_t* begin() noexcept {return data();}
    constexpr std::size_t* end() noexcept {return data() + rank();}


    constexpr std::size_t size() const noexcept {
        switch (rank()) {
            case 0: return 0; // ?
            case 1: return c.dims.stack;
            default: return c.dims.alloc[0];
        }
    }

    constexpr bool empty() const noexcept {return !size();}
};

/******************************************************************************/

// The only resource management to do is that for objects of > 2 dimensions,
// the shape is stored on the heap
union Span {
    sfb_span c;

    /**************************************************************************/

    constexpr Span() noexcept : c{nullptr, nullptr, Shape::Empty, 0} {}

    // One dimensional raw constructor
    Span(Tagged<Mode> i, void* data, Shape shape, std::uint32_t item) noexcept
        : c{i.base, data, move_slice(shape), item} {}

    template <class T>
    Span(T* data, Shape shape) noexcept
        : c{Tagged<Mode>(Index::of<T>(), std::is_const_v<T> ? Mode::Read : Mode::Write).base,
            const_cast<void*>(static_cast<void const *>(data)),
            move_slice(shape), sizeof(T)} {}

    /**************************************************************************/

    Span(Span &&s) noexcept : Span() {std::swap(c, s.c);}
    Span& operator=(Span &&s) noexcept {std::swap(c, s.c); return *this;}

    Span(Span const &s) = delete;
    Span& operator=(Span const &) = delete;

    ~Span() noexcept {shape().~Shape();}

    /**************************************************************************/

    Shape& shape() {return reinterpret_cast<Shape&>(c.shape);}
    Shape const& shape() const {return reinterpret_cast<Shape const&>(c.shape);}

    auto rank() const {return shape().rank();}

    std::size_t length(std::uint32_t axis) const {
        if (axis >= rank()) throw std::out_of_range("Span dimension out of range");
        return shape().data()[axis];
    }

    std::size_t size() const noexcept {return shape().size();}
    bool empty() const noexcept {return shape().empty();}

    Index index() const {return sfb_get_index(c.index);}
    Mode mode() const {return static_cast<Mode>(sfb_get_mode(c.index));}

    template <class F>
    bool map(F &&f) const {
        auto p = static_cast<char*>(c.data);
        auto const end = p + size() * c.item;
        Ref ref;
        ref.c.mode_index = c.index;
        for (; p != end; p += c.item) {
            ref.c.pointer = p;
            if (!f(ref)) return false;
        }
        return true;
    }

    template <class T>
    T const* target() const {return index() == Index::of<T>() ? static_cast<T const *>(c.data) : nullptr;}

    explicit operator sfb_span() && noexcept {return move_slice(*this);}
    // explicit operator sfb_span() const & {return const_slice(*this);}

    static void deallocate_shape(std::size_t* p, std::uint32_t rank) noexcept {delete[] p;}
};

/******************************************************************************/

// Array manages both the possibly allocated shape (like Span) but also the array allocation itself
union Array {
    sfb_array c;

    /**************************************************************************/

    Array() noexcept : c{{nullptr, nullptr, Shape::Empty, 0}, nullptr, nullptr} {}

    template <class S, class Alloc>
    Array(Span s, std::unique_ptr<S, Alloc> storage) noexcept
        : c{move_slice(s), nullptr, nullptr} {
        if (!span().empty()) {
            c.storage = storage.release();
            c.destructor = &drop<S, Alloc>;
        }
    }

    Array(Array &&s) noexcept : Array() {std::swap(c, s.c);}
    Array& operator=(Array &&s) noexcept {std::swap(c, s.c); return *this;}

    Array(Array const &s) = delete;
    Array& operator=(Array const &) = delete;

    ~Array() noexcept {
        if (c.destructor) c.destructor(c.span.index, c.storage);
        span().~Span();
    }

    /**************************************************************************/

    Span& span() noexcept {return reinterpret_cast<Span&>(c.span);}
    Span const& span() const noexcept {return reinterpret_cast<Span const&>(c.span);}

    explicit operator sfb_array() && noexcept {return move_slice(*this);}

    template <class S, class Alloc>
    static void drop(sfb_index, void* storage) {
        DUMP("deleting", storage, type_name<S>());
        std::unique_ptr<S, Alloc>(static_cast<std::remove_extent_t<S>*>(storage));
    }
};

/******************************************************************************/

// View is basically like a std::vector<Ref>, so it manages a static Ref allocation
union View {
    sfb_view c;

    /**************************************************************************/

    // Simple move-only heap allocation to use in View constructor
    // struct Alloc {
    //     std::unique_ptr<Ref[]> data;
    //     Shape shape;

    //     Alloc() noexcept = default;
    //     Alloc(std::size_t n);// : data(std::make_unique<Ref[]>(n)), size(n) {}

    //     auto begin() const {return data.get();}
    //     auto end() const {return data.get() + shape.size();}
    // };

    /**************************************************************************/

    constexpr View() noexcept : c{nullptr, Shape::Empty} {}

    template <class F>
    View(Shape shape, F&& fill) {
        if (shape.empty()) {
            c.data = nullptr;
        } else {
            c.data = sfb_view_allocate(shape.size());
            if (!c.data) throw std::bad_alloc();
            auto current = reinterpret_cast<Ref*>(c.data);
            try {
                fill(current, static_cast<Shape const&>(shape));
            } catch (...) {
                for (auto d = reinterpret_cast<Ref*>(c.data); d != current; ++d) d->~Ref();
                sfb_view_deallocate(c.data, &shape.c);
                throw;
            }
        }
        c.shape = move_slice(shape);
    }

    // View(Alloc alloc) noexcept
    //     : c{reinterpret_cast<sfb_ref*>(alloc.data.release()), move_slice(alloc.shape)} {}

    View(View &&s) noexcept : View() {std::swap(c, s.c);}
    View& operator=(View &&s) noexcept {std::swap(c, s.c); return *this;}

    View(View const &s) = delete;
    View& operator=(View const &) = delete;

    ~View() noexcept {
        if (c.data) sfb_view_deallocate(c.data, &c.shape);
        shape().~Shape();
    }

    /**************************************************************************/

    Shape& shape() {return reinterpret_cast<Shape&>(c.shape);}
    Shape const& shape() const {return reinterpret_cast<Shape const&>(c.shape);}

    std::size_t size() const noexcept {return shape().size();}
    constexpr bool empty() const noexcept {return c.data == nullptr;}

    Ref* begin() const {return reinterpret_cast<Ref*>(c.data);}
    Ref* end() const {return begin() + shape().size();}

    template <class F>
    bool map(F &&f) const {
        DUMP("dims:", shape().rank());
        for (auto i : shape()) DUMP(i);
        for (auto &ref : *this) if (!f(ref)) return false;
        return true;
    }

    explicit operator sfb_view() && noexcept {return move_slice(*this);}
};

/******************************************************************************/

// Tuple is like View but also manages the underlying object lifetime
union Tuple {
    sfb_tuple c;
    constexpr Tuple() noexcept
        : c{{nullptr, Shape::Empty}, nullptr, nullptr} {}

    template <class S, class Alloc>
    Tuple(View &&v, std::unique_ptr<S, Alloc> storage, bool empty) noexcept
        : c{move_slice(v), empty ? nullptr : storage.release(), empty ? nullptr : &drop<S, Alloc>} {}

    template <class S, class Alloc>
    Tuple(View v, std::unique_ptr<S, Alloc> storage) noexcept
        : Tuple(std::move(v), std::move(storage), v.empty()) {}

    Tuple(Tuple &&s) noexcept : Tuple() {std::swap(c, s.c);}
    Tuple& operator=(Tuple &&s) noexcept {std::swap(c, s.c); return *this;}

    Tuple(Tuple const &s) = delete;
    Tuple& operator=(Tuple const &) = delete;

    View& view() noexcept {return reinterpret_cast<View&>(c.view);}
    View const& view() const noexcept {return reinterpret_cast<View const&>(c.view);}

    explicit operator sfb_tuple() && noexcept {return move_slice(*this);}

    template <class S, class Alloc>
    static void drop(sfb_view const*, void* storage) noexcept {
        std::unique_ptr<S, Alloc>(static_cast<S*>(storage));
    }

    ~Tuple() noexcept {
         if (c.destructor) c.destructor(&c.view, c.storage);
         view().~View();
    }
};

/******************************************************************************/

std::ostream& operator<<(std::ostream&, Qualifier);
std::ostream& operator<<(std::ostream&, Mode);
std::ostream& operator<<(std::ostream&, Bool const &);
std::ostream& operator<<(std::ostream&, Str const &);
std::ostream& operator<<(std::ostream&, Bin const &);
std::ostream& operator<<(std::ostream&, String const &);
std::ostream& operator<<(std::ostream&, Binary const &);
std::ostream& operator<<(std::ostream&, Span const &);
std::ostream& operator<<(std::ostream&, Array const &);
std::ostream& operator<<(std::ostream&, Tuple const &);
std::ostream& operator<<(std::ostream&, View const &);
std::ostream& operator<<(std::ostream&, Shape const &);

/******************************************************************************/

template <> struct AliasType<Bool>   {using type = sfb_bool;};
template <> struct AliasType<Str>    {using type = sfb_str;};
template <> struct AliasType<Bin>    {using type = sfb_bin;};
template <> struct AliasType<String> {using type = sfb_string;};
template <> struct AliasType<Binary> {using type = sfb_binary;};
template <> struct AliasType<Span>   {using type = sfb_span;};
template <> struct AliasType<Array>  {using type = sfb_array;};
template <> struct AliasType<Tuple>  {using type = sfb_tuple;};
template <> struct AliasType<View>   {using type = sfb_view;};
template <> struct AliasType<Shape>   {using type = sfb_shape;};

/******************************************************************************************/

template <class T>
Name::stat Name::Default<T>::name(Str& s) noexcept {
    s = Str(TypeName<T>::name.data(), TypeName<T>::name.size());
    return OK;
}

/******************************************************************************************/

inline std::string_view Index::name() const noexcept {
    if (!has_value()) return "null";
    Str out;
    Name::invoke(*this, out);
    return out;
}

/******************************************************************************************/

}
