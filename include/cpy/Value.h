/**
 * @brief C++ type-erased Value object
 * @file Value.h
 */

#pragma once
#include "Error.h"

#include <iostream>
#include <variant>
#include <complex>
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

template <class T>
using no_qualifier = std::remove_cv_t<std::remove_reference_t<T>>;

struct Identity {
    template <class T>
    T const & operator()(T const &t) const {return t;}
};

/******************************************************************************/

// Roughly, a type safe version of std::any but simpler and non-owning
class CallingContext {
    void *metadata = nullptr;
    std::type_index index = typeid(void);
public:
    CallingContext() = default;

    template <class T>
    CallingContext(T *t) : metadata(t), index(typeid(T)) {}

    template <class T>
    T & get() {
        if (index != typeid(T)) throw DispatchError("Invalid context");
        return *static_cast<T *>(metadata);
    }
};

/******************************************************************************/

struct Value;
struct Value;

using Binary = std::vector<char>;

using Integer = std::ptrdiff_t;

using Real = double;

using Complex = std::complex<double>;

// using Any = std::any;

template <class T>
using Vector = std::vector<T>;

struct SequenceConcept {
    virtual std::unique_ptr<SequenceConcept> clone() const = 0;
    virtual void scan(std::function<void(Value)> const &) const = 0;
    virtual void fill(Value *, std::size_t) const = 0;
    virtual ~SequenceConcept() {};
};


class Sequence {
    std::unique_ptr<SequenceConcept> self;
    std::size_t m_size = 0;

public:
    Sequence() = default;

    std::size_t size() const {return m_size;}

    template <class T>
    Sequence(T&&t, std::size_t n);

    template <class T>
    explicit Sequence(T &&t) : Sequence(static_cast<T &&>(t), std::size(t)) {}

    Sequence(Sequence const &s) : self(s.self ? s.self->clone() : std::unique_ptr<SequenceConcept>()), m_size(s.m_size) {}

    Sequence & operator=(Sequence const &s) {
        if (s.self) self = s.self->clone();
        else self.reset();
        m_size = s.m_size;
        return *this;
    }

    void scan(std::function<void(Value)> const &f) const {if (self) self->scan(f);}
    void fill(Value *o, std::size_t n) const {if (self) self->fill(o, n);}

    template <class ...Ts>
    static Sequence vector(Ts &&...ts);

    Sequence(Sequence &&) noexcept = default;
    Sequence & operator=(Sequence &&s) noexcept = default;
};

/******************************************************************************/

template <class T, class V, class F=Identity>
Vector<T> mapped(V const &v, F &&f) {
    Vector<T> out;
    out.reserve(std::size(v));
    for (auto &&x : v) out.emplace_back(f(x));
    return out;
}

/******************************************************************************/

using ArgPack = Vector<Value>;

using Function = std::function<Value(CallingContext &, ArgPack &)>;

struct AnyConcept {
    virtual std::shared_ptr<AnyConcept> clone() const = 0;
    virtual ~AnyConcept() {}
};

template <class T>
struct AnyModel : AnyConcept {
    T value;
    template <class ...Ts>
    explicit AnyModel(Ts &&...ts) : value(static_cast<Ts &&>(ts)...) {}

    virtual std::shared_ptr<AnyConcept> clone() const override {
        return std::make_shared<AnyModel>(value);
    }
};

struct Any {
    std::shared_ptr<AnyConcept const> self;
    std::type_index m_type = typeid(void);

    std::type_index type() const {return m_type;}
    Any() = default;
    Any(Any const &a) : self(a.self ? a.self->clone() : decltype(self)()), m_type(a.m_type) {}
    Any & operator=(Any const &a) {
        if (a.self) self = a.self->clone();
        else self.reset();
        m_type = a.m_type;
        return *this;
    }
    Any(Any &&a) noexcept = default;
    Any & operator=(Any &&a) noexcept = default;

    bool has_value() const {return bool(self);}

    template <class T>
    Any(std::in_place_t, T &&t)
        : self(std::make_shared<AnyModel<no_qualifier<T>>>(static_cast<T &&>(t))),
          m_type(typeid(no_qualifier<T>)) {static_assert(!std::is_same_v<no_qualifier<T>, Any>);}

    template <class T>
    explicit Any(T &&t) : Any(std::in_place_t(), static_cast<T &&>(t)) {}

    // template <class T, class ...Ts>
    // Any(std::in_place_type_t<T>, Ts &&...ts);
};

template <class T>
T anycast(Any &&a) {
    static_assert(std::is_same_v<T, no_qualifier<T>>);
    if (a.type() != typeid(T)) throw DispatchError("any");
    auto ptr = static_cast<AnyModel<T> const *>(a.self.get());
    if (!a.self.unique()) return ptr->value;
    auto tmp = std::move(a);
    return std::move(const_cast<AnyModel<T> *>(ptr)->value);
}

template <class T>
T const & anycast(Any const &a) {
    static_assert(std::is_same_v<T, no_qualifier<T>>);
    return static_cast<AnyModel<T> const *>(a.self.get())->value;
}

using Variant = std::variant<
    /*0*/ std::monostate,
    /*1*/ bool,
    /*2*/ Integer,
    /*3*/ Real,
    /*4*/ std::string_view,
    /*5*/ std::string,
    /*6*/ std::type_index,
    /*7*/ Binary,       // ?
    /*8*/ Function,
    /*9*/ Any,     // ?
          Sequence
    // /*0*/ Vector<Value> // ?
>;
// One idea for Any is instead to make it
// shared_ptr<AnyConcept const>

// using AnyRef = Any const *;
// using BinaryRef = Binary const *;

using Variant = std::variant<
    /*0*/ std::monostate,
    /*1*/ bool,
    /*2*/ Integer,
    /*3*/ Real,
    /*4*/ std::string_view,
    /*5*/ std::string,
    /*6*/ std::type_index,
    /*7*/ Binary,       // ?
    /*8*/ Function,
    /*9*/ Any,     // ?
    // /*7*/ BinaryRef,    // ?
    // /*9*/ AnyRef,  // ?
    /*0*/ Sequence // ?
>;

static_assert(1 ==  sizeof(std::monostate));    // 1
static_assert(1 ==  sizeof(bool));              // 1
static_assert(8 ==  sizeof(Integer));           // ptrdiff_t
static_assert(8 ==  sizeof(Real));              // double
static_assert(16 == sizeof(std::string_view)); // start, stop
static_assert(24 == sizeof(std::string));      // start, stop, buffer
static_assert(8 ==  sizeof(std::type_index));   // size_t
static_assert(24 == sizeof(Binary));           // 8 start, stop ... buffer?
static_assert(48 == sizeof(Function));         // 32 buffer + 8 pointer + 8 vtable
static_assert(24 == sizeof(Any));              // 8 + 24 buffer I think
// static_assert(8 == sizeof(BinaryRef));         // pointer
// static_assert(8 == sizeof(AnyRef));            // pointer
static_assert(24 == sizeof(Vector<Value>));    //
static_assert(64 == sizeof(Variant));
static_assert(16 == sizeof(Sequence));

// SCALAR - get() returns any of:
// - std::monostate,
// - bool,
// - Integer,
// - Real,
// - std::string_view,
// - std::type_index,

// Function is probably SCALAR
// - it satisfies the interfaces [Copy, Move, Value(Context, Value)]
// - it might also have a list of keyword arguments (# required?)

// Any
// - it satisfies the interface [Copy, Move, .type()]

// Sequence
// - it satisfies the interface [Copy, Move, begin(), end(), ++it, *it gives Value, ]


template <class T, class=void>
struct ToValue;

template <class T>
struct InPlace {
    T value;
};


struct Value {
    Variant var;

    Value(Value &&v) noexcept : var(std::move(v.var)) {}
    Value(Value const &v) : var(v.var) {}
    Value(Value &v) : var(v.var) {}

    Value & operator=(Value const &v) {var = v.var; return *this;}
    Value & operator=(Value &&v) noexcept {var = std::move(v.var); return *this;}

    Value(std::monostate v={})   noexcept : var(v) {}
    Value(bool v)                noexcept : var(v) {}
    Value(Integer v)             noexcept : var(v) {}
    Value(Real v)                noexcept : var(v) {}
    Value(Function v)            noexcept : var(std::move(v)) {}
    Value(Binary v)              noexcept : var(std::move(v)) {}
    Value(std::string v)         noexcept : var(std::move(v)) {}
    Value(std::string_view v)    noexcept : var(std::move(v)) {}
    Value(std::type_index v)     noexcept : var(std::move(v)) {}
    Value(Sequence v)            noexcept : var(std::move(v)) {}

    template <class T>
    Value(std::in_place_t, T &&t) noexcept : var(std::in_place_type_t<Any>(), static_cast<T &&>(t)) {}

    template <class T>
    Value(InPlace<T> &&t) noexcept : var(std::in_place_type_t<Any>(), static_cast<T>(t.value)) {}

    template <class T, std::enable_if_t<!(std::is_same_v<no_qualifier<T>, Value>), int> = 0>
    Value(T &&t) : Value(ToValue<no_qualifier<T>>()(static_cast<T &&>(t))) {}

    /******************************************************************************/

    bool as_bool() const {return std::get<bool>(var);}
    Real as_real() const {return std::get<Real>(var);}
    Integer as_integer() const {return std::get<Integer>(var);}
};
    // std::string_view as_view() const & {return std::get<std::string_view>(var);}
    // std::type_index as_index() const & {return std::get<std::type_index>(var);}
    // Any as_any() const & {return std::get<Any>(var);}
    // Any as_any() && {return std::get<Any>(std::move(var));}
    // std::string as_string() const & {
    //     if (auto s = std::get_if<std::string_view>(&var))
    //         return std::string(*s);
    //     return std::get<std::string>(var);
    // }
    // Vector<Value> as_vector() const & {return std::get<Vector<Value>>(var);}
    // Vector<Value> as_vector() && {return std::get<Vector<Value>>(std::move(var));}
    // Binary as_binary() const & {return std::get<Binary>(var);}
    // Binary as_binary() && {return std::get<Binary>(std::move(var));}
    // ~Value() = default;

struct KeyPair {
    std::string_view key;
    Value value;
};

/******************************************************************************/

WrongTypes wrong_types(ArgPack const &v);

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

static char const *cast_bug_message = "FromValue().check() returned false but FromValue()() was still called";

// should make this into struct of T, U?
/// Default behavior for casting a variant to a desired argument type
template <class T, class=void>
struct FromValue {
    // Return true if type T can be cast from type U
    template <class U>
    constexpr bool check(U const &) const {
        return std::is_convertible_v<U &&, T> ||
            (std::is_same_v<T, std::monostate> && std::is_default_constructible_v<T>);
    }
    // Return casted type T from type U
    template <class U>
    T operator()(U &&u) const {
        static_assert(std::is_rvalue_reference_v<U &&>);
        if constexpr(std::is_convertible_v<U &&, T>) return static_cast<T>(static_cast<U &&>(u));
        else if constexpr(std::is_default_constructible_v<T>) return T(); // only hit if U == std::monostate
        else throw std::logic_error(cast_bug_message); // never get here
    }

    bool check(Any const &u) const {return u.type() == typeid(T);}

    T operator()(Any &&u) const {return anycast<no_qualifier<T>>(u);}
};

/******************************************************************************/

template <class T>
struct FromValue<Vector<T>> {
    template <class U>
    bool check(U const &) const {return false;}

    bool check(Sequence const &u) const {
        bool ok = true;
        u.scan([&](Value const &x) {
            ok = ok && std::visit([&](auto &x) {return FromValue<T>().check(x);}, x.var);
        });
        return ok;
    }

    Vector<T> operator()(Sequence &&u) const {
        Vector<T> out;
        out.reserve(u.size());
        u.scan([&](Value x) {
            std::visit([&](auto &x) {out.emplace_back(FromValue<T>()(std::move(x)));}, x.var);
        });
        return out;
    }

    template <class U>
    Vector<T> operator()(U const &) const {throw std::logic_error("shouldn't be used");}
};

/******************************************************************************/



template <class T>
struct SequenceModel : SequenceConcept {
    T value;
    SequenceModel(T &&t) : value(static_cast<T &&>(t)) {}
    SequenceModel(T const &t) : value(static_cast<T const &>(t)) {}

    virtual std::unique_ptr<SequenceConcept> clone() const override {
        return std::make_unique<SequenceModel>(*this);
    }
    virtual void scan(std::function<void(Value)> const &f) const override {
        for (auto &&v : value) f(Value(v));
    }
    virtual void fill(Value *o, std::size_t n) const override {
        for (auto &&v : value) {
            if (n--) return;
            *(o++) = Value(v);
        }
    }
};







template <class T>
Sequence::Sequence(T&&t, std::size_t n) : self(std::make_unique<SequenceModel<no_qualifier<T>>>(t)), m_size(n) {}

template <class ...Ts>
Sequence Sequence::vector(Ts &&...ts) {
    std::vector<Value> vec;
    vec.reserve(sizeof...(Ts));
    (vec.emplace_back(static_cast<Ts &&>(ts)), ...);
    return Sequence(std::move(vec));
}



}
