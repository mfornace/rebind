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

using ArgPack = Vector<Value>;

/******************************************************************************/

struct Function {
    std::function<Value(CallingContext &, ArgPack)> call;

    using function_type = std::function<Value(CallingContext &, ArgPack)>;
    Function() = default;

    template <class F, class ...Ts>
    Function(F fun, Pack<Ts...>, Vector<std::string> kws={}, std::int32_t required=-1);

    template <class F>
    explicit Function(F fun, Vector<std::string> kws={}, std::int32_t required=-1)
        : Function(fun, Signature<F>(), std::move(kws), required) {}

    Vector<std::string> keywords;

    bool has_value() const {return bool(call);}

    Value operator()(CallingContext &ct, ArgPack v) const;

    void reset(function_type &&f, std::int32_t l, std::int32_t r) {
        call = std::move(call);
        m_length = l;
        m_required = r;
    }

    auto length() const {return m_length;}
    auto required() const {return m_required;}

    std::type_index type() const {return call.target_type();}
private:

    std::int32_t m_length=-1, m_required=-1;
};

using Any = std::any;

/******************************************************************************/

struct SequenceConcept {
    virtual char const * shortcut(int &, std::ptrdiff_t &) const {return nullptr;}
    virtual void scan(std::function<void(Value)> const &) const {}
    virtual ~SequenceConcept() {};
};

class Sequence {
    std::shared_ptr<SequenceConcept const> self;
    std::size_t m_size = 0;

public:
    Sequence() = default;

    std::size_t size() const {return self ? m_size : 0;}

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
static_assert(24 == sizeof(Binary));           // 8 start, stop ... buffer?
static_assert(80 == sizeof(Function));         // 32 buffer + 8 pointer + 8 vtable
static_assert(32 == sizeof(Any));              // 8 + 24 buffer I think
static_assert(24 == sizeof(Vector<Value>));    //
static_assert(96 == sizeof(Variant));
static_assert(24 == sizeof(Sequence));

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

template <class T, class>
struct ToValue {
    InPlace<T &&> operator()(T &&t) const {return {static_cast<T &&>(t)};}
    InPlace<T const &> operator()(T const &t) const {return {t};}
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

/// Default behavior for casting a variant to a desired argument type
template <class T, class=void>
struct FromValue {
    DispatchMessage &message;
    // Return casted type T from type U
    template <class U>
    T operator()(U &&u) const {
        static_assert(std::is_rvalue_reference_v<U &&>);
        if constexpr(std::is_convertible_v<U &&, T>) return static_cast<T>(static_cast<U &&>(u));
        else if constexpr(std::is_same_v<T, std::monostate> && std::is_default_constructible_v<T>) return T();
        message.dest = typeid(T);
        message.source = typeid(U);
        throw WrongTypes(std::move(message));
    }

    T operator()(Any &&u) const {
        auto ptr = &u;
        if (auto p = std::any_cast<std::any *>(&u)) ptr = *p;
        if (auto p = std::any_cast<no_qualifier<T>>(ptr))
            return static_cast<T>(*p);
        message.scope = u.has_value() ? "mismatched class" : "object was already moved";
        message.dest = typeid(T);
        message.source = u.type();
        throw WrongTypes(std::move(message));
    }
};

/******************************************************************************/

template <class T>
struct FromValue<Vector<T>> {
    DispatchMessage &message;

    Vector<T> operator()(Sequence &&u) const {
        Vector<T> out;
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
    Vector<T> operator()(U const &) const {throw std::logic_error("expected vector");}
};

/******************************************************************************/

template <class T, class=void>
struct SequenceModel final : SequenceConcept {
    T value;
    SequenceModel(T &&t) : value(static_cast<T &&>(t)) {}
    SequenceModel(T const &t) : value(static_cast<T const &>(t)) {}

    void scan(std::function<void(Value)> const &f) const override {
        for (auto &&v : value) f(Value(static_cast<decltype(v) &&>(v)));
    }
};

// Shortcuts for vector of Value using its contiguity
template <class T, class Alloc>
struct SequenceModel<std::vector<T, Alloc>, std::enable_if_t<(ValuePack::contains<T> || std::is_same_v<T, Value>)>> final : SequenceConcept {
    using V = std::vector<T, Alloc>;
    V value;
    SequenceModel(V &&v) : value(static_cast<V &&>(v)) {}
    SequenceModel(V const &v) : value(static_cast<V const &>(v)) {}

    char const * shortcut(int &n, std::ptrdiff_t &stride) const final {
        n = ValuePack::position<T>;
        stride = sizeof(T) / sizeof(char);
        return reinterpret_cast<char const *>(value.data());
    }
};

template <class T>
Sequence::Sequence(T&&t, std::size_t n) : self(std::make_shared<SequenceModel<no_qualifier<T>>>(t)), m_size(n) {}

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
    if (self) {
        int idx; std::ptrdiff_t stride;
        if (auto p = self->shortcut(idx, stride)) {
            if (idx == -1) raw_scan<Value>(f, p, p + stride * m_size, stride);
            else ValuePack::for_each([&, i=0](auto t) mutable {
                if (i++ == idx) raw_scan<decltype(*t)>(f, p, p + stride * m_size, stride);
            });
        } else self->scan(static_cast<F &&>(f));
    }
}

inline Value Function::operator()(CallingContext &ct, ArgPack v) const {return call(ct, std::move(v));}

/******************************************************************************/

}
