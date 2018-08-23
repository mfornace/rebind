/**
 * @brief C++ type-erased Value object
 * @file Value.h
 */

#pragma once
#include "Error.h"
#include "Signature.h"
#include "Common.h"

#include <iostream>
#include <variant>
#include <string>
#include <vector>
#include <type_traits>
#include <string_view>
#include <any>
#include <memory>
#include <typeindex>

#define CPY_CAT_IMPL(s1, s2) s1##s2
#define CPY_CAT(s1, s2) CPY_CAT_IMPL(s1, s2)

#define CPY_STRING_IMPL(x) #x
#define CPY_STRING(x) CPY_STRING_IMPL(x)

namespace cpy {

/******************************************************************************/

struct Value;

using Binary = std::basic_string<unsigned char>;

using BinaryView = std::basic_string_view<unsigned char>;

using Integer = std::ptrdiff_t;

using Real = double;

using ArgPack = SmallVec<Value>;

/******************************************************************************/

using Function = std::function<Value(CallingContext &, ArgPack)>;

using Any = std::any;

/******************************************************************************/

class Sequence {
    std::function<char const *(std::function<void(Value)> const &, int &, std::ptrdiff_t &)> scan;
    std::size_t m_size = 0;

public:
    using scan_type = decltype(scan);
    Sequence() = default;

    std::size_t size() const {return scan ? m_size : 0;}

    template <class T>
    Sequence(T&&t, std::size_t n);

    Sequence(std::initializer_list<Value> const &v);

    template <class T, std::enable_if_t<!(std::is_same_v<no_qualifier<T>, Sequence>), int> = 0>
    explicit Sequence(T &&t) : Sequence(static_cast<T &&>(t), std::size(t)) {}

    template <class F>
    void scan_functor(F &&f) const;

    void scan_function(std::function<void(Value)> const &) const;

    template <class ...Ts>
    static Sequence vector(Ts &&...ts);
};

/******************************************************************************/

using ValuePack = Pack<
    /* 0 */ std::monostate,
    /* 1 */ bool,
    /* 2 */ Integer,
    /* 3 */ Real,
    /* 4 */ std::string_view,
    /* 5 */ std::string,
    /* 6 */ std::type_index,
    /* 7 */ Binary,       // ?
    /* 8 */ BinaryView,   // ?
    /* 9 */ Function,
    /* 0 */ Any,     // ?
    /* 1 */ Sequence
>;

using Variant = decltype(variant_type(ValuePack()));

static_assert(1  == sizeof(std::monostate));    // 1
static_assert(1  == sizeof(bool));              // 1
static_assert(8  == sizeof(Integer));           // ptrdiff_t
static_assert(8  == sizeof(Real));              // double
static_assert(16 == sizeof(std::string_view)); // start, stop
static_assert(24 == sizeof(std::string));      // start, stop, buffer
static_assert(8  == sizeof(std::type_index));   // size_t
static_assert(24 == sizeof(Binary));           // start, stop buffer?
static_assert(48 == sizeof(Function));         // 24 buffer + 8 pointer + 8 vtable?
static_assert(32 == sizeof(Any));              // 8 + 24 buffer I think
static_assert(24 == sizeof(Vector<Value>));    //
static_assert(80 == sizeof(Variant));
static_assert(64 == sizeof(Sequence));

/******************************************************************************/

template <class T, class=void>
struct ToValue;

template <class T>
struct InPlace {T value;};

struct Value {
    Variant var;

    Value(Value &&v) noexcept : var(std::move(v.var)) {}
    Value(Value const &v) : var(v.var) {}
    Value(Value &v) : var(v.var) {}
    ~Value() = default;

    Value & operator=(Value const &v) {var = v.var; return *this;}
    Value & operator=(Value &&v) noexcept {var = std::move(v.var); return *this;}

    Value(std::monostate v={}) noexcept : var(v) {}
    Value(bool v)              noexcept : var(v) {}
    Value(Integer v)           noexcept : var(v) {}
    Value(Real v)              noexcept : var(v) {}
    Value(Function v)          noexcept : var(std::move(v)) {}
    Value(Binary v)            noexcept : var(std::move(v)) {}
    Value(std::string v)       noexcept : var(std::move(v)) {}
    Value(std::string_view v)  noexcept : var(std::move(v)) {}
    Value(std::type_index v)   noexcept : var(std::move(v)) {}
    Value(Sequence v)          noexcept : var(std::move(v)) {}

    template <class T>
    Value(std::in_place_t, T &&t) noexcept : var(std::in_place_type_t<Any>(), static_cast<T &&>(t)) {}

    template <class T>
    Value(InPlace<T> &&t) noexcept : var(std::in_place_type_t<Any>(), static_cast<T>(t.value)) {}

    template <class T, std::enable_if_t<!(std::is_same_v<no_qualifier<T>, Value>), int> = 0>
    Value(T &&t) : Value(ToValue<no_qualifier<T>>()(static_cast<T &&>(t))) {}

    /******************************************************************************/

    friend Value no_view(Value v);
    bool as_bool() const {return std::get<bool>(var);}
    Real as_real() const {return std::get<Real>(var);}
    Integer as_integer() const {return std::get<Integer>(var);}
    std::type_index as_type() const {return std::get<std::type_index>(var);}
    // std::string_view as_view() const & {return std::get<std::string_view>(var);}
    // Any as_any() const & {return std::get<Any>(var);}
    // Any as_any() && {return std::get<Any>(std::move(var));}
    // std::string as_string() const & {
    //     if (auto s = std::get_if<std::string_view>(&var))
    //         return std::string(*s);
    //     return std::get<std::string>(var);
    // }
    // Binary as_binary() const & {return std::get<Binary>(var);}
    // Binary as_binary() && {return std::get<Binary>(std::move(var));}
};

struct KeyPair {
    std::string_view key;
    Value value;
};

/******************************************************************************/

/// The default implementation is to serialize to Any
template <class T, class>
struct ToValue {
    InPlace<T &&> operator()(T &&t) const {return {static_cast<T &&>(t)};}
    InPlace<T const &> operator()(T const &t) const {return {t};}
};

void to_value(std::nullptr_t);

template <class T>
struct ToValue<T, std::void_t<decltype(to_value(Type<T>(), std::declval<T const &>()))>> {
    decltype(auto) operator()(T &&t) const {return to_value(Type<T>(), static_cast<T &&>(t));}
    decltype(auto) operator()(T const &t) const {return to_value(Type<T>(), t);}
};

template <class T>
struct ToValue<T, std::enable_if_t<(std::is_floating_point_v<T>)>> {
    Real operator()(T t) const {return static_cast<Real>(t);}
};

template <class T>
struct ToValue<T, std::enable_if_t<(std::is_integral_v<T>)>> {
    Integer operator()(T t) const {return static_cast<Integer>(t);}
};

template <>
struct ToValue<char const *> {
    std::string operator()(char const *t) const {return std::string(t);}
};

template <class T, class Alloc>
struct ToValue<std::vector<T, Alloc>> {
    Sequence operator()(std::vector<T, Alloc> t) const {return Sequence(std::move(t));}
};

/******************************************************************************/

/// The default implementation is to accept convertible arguments or Any of the exact typeid match
template <class T, class=void>
struct FromValue {
    DispatchMessage &message;
    // Return casted type T from type U
    template <class U>
    T operator()(U &&u) const {
        static_assert(std::is_rvalue_reference_v<U &&>);
        if constexpr(std::is_constructible_v<T, U &&>) return static_cast<T>(static_cast<U &&>(u));
        else if constexpr(std::is_same_v<T, std::monostate> && std::is_default_constructible_v<T>) return T();
        message.dest = typeid(T);
        message.source = typeid(U);
        throw message.error();
    }

    T operator()(Any &&u) const {
        auto ptr = &u;
        if (auto p = std::any_cast<std::any *>(&u)) ptr = *p;
        if (auto p = std::any_cast<no_qualifier<T>>(ptr)) return static_cast<T>(*p);
        message.scope = u.has_value() ? "mismatched class" : "object was already moved";
        message.dest = typeid(T);
        message.source = u.type();
        throw message.error();
    }
};

template <class T>
struct FromValue<T, std::void_t<decltype(from_value(+Type<T>(), Sequence(), std::declval<DispatchMessage &>()))>> {
    DispatchMessage &message;
    using O = std::remove_const_t<
        decltype(1 ? std::declval<T>() : from_value(+Type<T>(), std::declval<Any &&>(), std::declval<DispatchMessage &>()))
    >;

    O operator()(Any &&u) const {
        auto ptr = &u;
        if (auto p = std::any_cast<std::any *>(&u)) ptr = *p;
        auto p = std::any_cast<no_qualifier<T>>(ptr);
        message.source = u.type();
        message.dest = typeid(T);
        return p ? static_cast<T>(*p) : from_value(+Type<T>(), std::move(*ptr), message);
    }

    template <class U>
    O operator()(U &&u) const {
        message.source = typeid(U);
        message.dest = typeid(T);
        return from_value(+Type<T>(), static_cast<U &&>(u), message);
    }
};

/******************************************************************************/

template <class V>
struct ContiguousFromValue {
    using T = typename V::value_type;
    DispatchMessage &message;

    V operator()(Sequence &&u) const {
        V out;
        out.reserve(u.size());
        message.indices.emplace_back(0);
        u.scan_functor([&](Value x) {
            std::visit([&](auto &x) {out.emplace_back(FromValue<T>{message}(std::move(x)));}, x.var);
            ++message.indices.back();
        });
        message.indices.pop_back();
        return out;
    }

    template <class U>
    V operator()(U const &) const {
        message.scope = "expected sequence";
        message.dest = typeid(V);
        message.source = typeid(U);
        throw message.error();
    }
};

template <class T>
struct FromValue<Vector<T>> : ContiguousFromValue<Vector<T>> {};

/******************************************************************************/

template <class T, class=void>
struct SequenceModel {
    T value;

    char const * operator()(std::function<void(Value)> const &f, int const &, std::ptrdiff_t const &) const {
        for (auto &&v : value) f(Value(static_cast<decltype(v) &&>(v)));
        return nullptr;
    }
};

// Shortcuts for vector of Value using its contiguity
template <class V>
struct ContiguousValueModel {
    using T = no_qualifier<decltype(*std::begin(std::declval<V>()))>;
    V value;

    char const * operator()(std::function<void(Value)> const &, int &n, std::ptrdiff_t &stride) const {
        n = ValuePack::position<T>;
        stride = sizeof(T) / sizeof(char);
        return reinterpret_cast<char const *>(value.data());
    }
};

template <class T, class Alloc>
struct SequenceModel<std::vector<T, Alloc>,
    std::enable_if_t<(ValuePack::contains<T> || std::is_same_v<T, Value>)>>
    : ContiguousValueModel<std::vector<T, Alloc>> {};

template <class T>
Sequence::Sequence(T &&t, std::size_t n)
    : scan(SequenceModel<no_qualifier<T>>{static_cast<T &&>(t)}), m_size(n) {}

template <class ...Ts>
Sequence Sequence::vector(Ts &&...ts) {
    Vector<Value> vec;
    vec.reserve(sizeof...(Ts));
    (vec.emplace_back(static_cast<Ts &&>(ts)), ...);
    return Sequence(std::move(vec));
}

template <class T, class F>
void raw_scan(F &&f, char const *p, char const *e, std::ptrdiff_t stride) {
    for (; p != e; p += stride) f(reinterpret_cast<T const *>(p));
}

template <class F>
void Sequence::scan_functor(F &&f) const {
    if (scan) {
        int idx; std::ptrdiff_t stride;
        auto const data = scan(static_cast<F &&>(f), idx, stride);
        if (data) {
            auto const end = data + stride * m_size;
            if (idx == -1) raw_scan<Value>(f, data, end, stride);
            else ValuePack::for_each([&, i=0](auto t) mutable {
                if (i++ == idx) raw_scan<decltype(*t)>(f, data, end, stride);
            });
        }
    }
}

/******************************************************************************/

}
