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

struct Output;
struct Input;

using Binary = std::vector<char>;

using Integer = std::ptrdiff_t;

using Real = double;

using Complex = std::complex<double>;

using Any = std::any;

template <class T>
using Vector = std::vector<T>;

/******************************************************************************/

template <class T, class V, class F=Identity>
Vector<T> mapped(V const &v, F &&f) {
    Vector<T> out;
    out.reserve(std::size(v));
    for (auto &&x : v) out.emplace_back(f(x));
    return out;
}

/******************************************************************************/

using ArgPack = Vector<Input>;

using Function = std::function<Output(CallingContext &, ArgPack &)>;

using OutputVariant = std::variant<
    /*0*/ std::monostate,
    /*1*/ bool,
    /*2*/ Integer,
    /*3*/ Real,
    /*4*/ std::string_view,
    /*5*/ std::string,
    /*6*/ std::type_index,
    // /*7*/ Binary,       // ?
    /*8*/ Function,
    /*9*/ Any,     // ?
    // /*0*/ Vector<Output> // ?
>;

using AnyRef = Any const *;
using BinaryRef = Binary const *;

using InputVariant = std::variant<
    /*0*/ std::monostate,
    /*1*/ bool,
    /*2*/ Integer,
    /*3*/ Real,
    /*4*/ std::string_view,
    /*5*/ std::string,
    /*6*/ std::type_index,
    // /*7*/ Binary,       // ?
    /*8*/ Function,
    /*9*/ Any,     // ?
    // /*7*/ BinaryRef,    // ?
    /*9*/ AnyRef,  // ?
    /*0*/ Vector<Input> // ?
>;

static_assert(1 == sizeof(std::monostate));    // 1
static_assert(1 == sizeof(bool));              // 1
static_assert(8 == sizeof(Integer));           // ptrdiff_t
static_assert(8 == sizeof(Real));              // double
static_assert(16 == sizeof(std::string_view)); // start, stop
static_assert(24 == sizeof(std::string));      // start, stop, buffer
static_assert(8 == sizeof(std::type_index));   // size_t
static_assert(24 == sizeof(Binary));           // 8 start, stop ... buffer?
static_assert(48 == sizeof(Function));         // 32 buffer + 8 pointer + 8 vtable
static_assert(32 == sizeof(Any));              // 8 + 24 buffer I think
static_assert(8 == sizeof(BinaryRef));         // pointer
static_assert(8 == sizeof(AnyRef));            // pointer
static_assert(24 == sizeof(Vector<Input>));    //
static_assert(64 == sizeof(InputVariant));

// SCALAR - get() returns any of:
// - std::monostate,
// - bool,
// - Integer,
// - Real,
// - std::string_view,
// - std::type_index,

// Function is probably SCALAR
// - it satisfies the interfaces [Copy, Move, Output(Context, Input)]
// - it might also have a list of keyword arguments (# required?)

// Any
// - it satisfies the interface [Copy, Move, .type()]

// Sequence
// - it satisfies the interface [Copy, Move, begin(), end(), ++it, *it gives Output, ]


template <class T, class=void>
struct ToOutput;

template <class T>
struct InPlace {
    T value;
};


struct Output {
    OutputVariant var;

    Output(Output &&v) noexcept : var(std::move(v.var)) {}
    Output(Output const &v) : var(v.var) {}
    Output(Output &v) : var(v.var) {}

    Output & operator=(Output const &v) {var = v.var; return *this;}
    Output & operator=(Output &&v) noexcept {var = std::move(v.var); return *this;}

    Output(Input &&)              noexcept;
    Output(Input const &);
    Output(std::monostate v={})   noexcept : var(v) {}
    Output(bool v)                noexcept : var(v) {}
    Output(Integer v)             noexcept : var(v) {}
    Output(Real v)                noexcept : var(v) {}
    Output(Function v)            noexcept : var(std::move(v)) {}
    Output(Binary v)              noexcept : var(std::move(v)) {}
    Output(std::string v)         noexcept : var(std::move(v)) {}
    Output(std::string_view v)    noexcept : var(std::move(v)) {}
    Output(std::type_index v)     noexcept : var(std::move(v)) {}
    Output(Vector<Output> v)      noexcept : var(std::move(v)) {}

    template <class T>
    Output(std::in_place_t, T &&t) noexcept : var(std::in_place_type_t<Any>(), static_cast<T &&>(t)) {}

    template <class T>
    Output(InPlace<T> &&t) noexcept : var(std::in_place_type_t<Any>(), static_cast<T>(t.value)) {}

    template <class T>
    Output(InPlace<T> const &t) noexcept : var(std::in_place_type_t<Any>(), static_cast<T>(t.value)) {}

    template <class T, std::enable_if_t<!(std::is_same_v<no_qualifier<T>, Output>), int> = 0>
    Output(T &&t) : Output(ToOutput<no_qualifier<T>>()(static_cast<T &&>(t))) {}

    /******************************************************************************/

    bool as_bool() const & {return std::get<bool>(var);}
    Integer as_integer() const & {return std::get<Integer>(var);}
    Real as_real() const & {return std::get<Real>(var);}
    std::string_view as_view() const & {return std::get<std::string_view>(var);}
    std::type_index as_index() const & {return std::get<std::type_index>(var);}
    Any as_any() const & {return std::get<Any>(var);}
    Any as_any() && {return std::get<Any>(std::move(var));}
    std::string as_string() const & {
        if (auto s = std::get_if<std::string_view>(&var))
            return std::string(*s);
        return std::get<std::string>(var);
    }
    Vector<Output> as_vector() const & {return std::get<Vector<Output>>(var);}
    Vector<Output> as_vector() && {return std::get<Vector<Output>>(std::move(var));}
    Binary as_binary() const & {return std::get<Binary>(var);}
    Binary as_binary() && {return std::get<Binary>(std::move(var));}
    ~Output() = default;
};

struct Input {
    InputVariant var;

    Input(Output &&) noexcept;
    Input(Output const &);

    Input(Input &&v) noexcept : var(std::move(v.var)) {}
    Input(Input const &v) : var(v.var) {}
    Input & operator=(Input const &v) {var = v.var; return *this;}
    Input & operator=(Input &&v) noexcept {var = std::move(v.var); return *this;}

    Input(std::monostate v={})   noexcept : var(v) {}
    Input(bool v)                noexcept : var(v) {}
    Input(Integer v)             noexcept : var(v) {}
    Input(Real v)                noexcept : var(v) {}
    Input(Function v)            noexcept : var(std::move(v)) {}
    Input(Binary v)              noexcept : var(std::move(v)) {}
    Input(std::string v)         noexcept : var(std::move(v)) {}
    Input(std::string_view v)    noexcept : var(std::move(v)) {}
    Input(std::type_index v)     noexcept : var(std::move(v)) {}
    Input(Vector<Input> v)       noexcept : var(std::move(v)) {}

    template <class T>
    Input(InPlace<T> &&t) noexcept : var(std::in_place_type_t<Any>(), static_cast<T &&>(t.value)) {}

    template <class T>
    Input(InPlace<T> const &t) noexcept : var(std::in_place_type_t<Any>(), static_cast<T &&>(t.value)) {}

    template <class T>
    Input(std::in_place_t, T &&t) noexcept : var(std::in_place_type_t<Any>(), static_cast<T &&>(t)) {}

    /******************************************************************************/

    bool as_bool() const & {return std::get<bool>(var);}
    Integer as_integer() const & {return std::get<Integer>(var);}
    Real as_real() const & {return std::get<Real>(var);}

    std::string_view as_view() const & {return std::get<std::string_view>(var);}

    std::type_index as_index() const & {return std::get<std::type_index>(var);}

    std::string as_string() const & {
        if (auto s = std::get_if<std::string_view>(&var)) return std::string(*s);
        return std::get<std::string>(var);
    }

    std::string as_string() && {
        if (auto s = std::get_if<std::string_view>(&var)) return std::string(*s);
        return std::get<std::string>(std::move(var));
    }

    Vector<Input> & as_vector() & {return std::get<Vector<Input>>(var);}
    Vector<Input> const & as_vector() const & {return std::get<Vector<Input>>(var);}
    Vector<Input> as_vector() && {return std::get<Vector<Input>>(std::move(var));}

    ~Input() = default;

    Binary const & as_binary() const & {
        if (auto s = std::get_if<BinaryRef>(&var)) return **s;
        return std::get<Binary>(var);
    }
    Binary as_binary() && {
        if (auto s = std::get_if<BinaryRef>(&var)) return **s;
        return std::get<Binary>(std::move(var));
    }
    Any const & as_any() const & {
        if (auto s = std::get_if<AnyRef>(&var)) return **s;
        return std::get<Any>(var);
    }
    Any as_any() && {
        if (auto s = std::get_if<AnyRef>(&var)) return **s;
        return std::get<Any>(std::move(var));
    }
};

struct KeyPair {
    std::string_view key;
    Output value;
};

/******************************************************************************/

WrongTypes wrong_types(ArgPack const &v);

/******************************************************************************/

template <class T, class>
struct ToOutput {
    InPlace<T &&> operator()(T &&t) const {return {static_cast<T &&>(t)};}
    InPlace<T const &> operator()(T const &t) const {return {t};}
};

template <class T>
struct ToOutput<T, std::enable_if_t<(std::is_floating_point_v<T>)>> {
    Real operator()(T t) const {return static_cast<Real>(t);}
};

template <class T>
struct ToOutput<T, std::enable_if_t<(std::is_integral_v<T>)>> {
    Integer operator()(T t) const {return static_cast<Integer>(t);}
};

template <>
struct ToOutput<char const *> {
    std::string operator()(char const *t) const {return std::string(t);}
};

template <class T, class Alloc>
struct ToOutput<std::vector<T, Alloc>> {
    Vector<Output> operator()(std::vector<T, Alloc> t) const {
        Vector<Output> vec;
        vec.reserve(t.size());
        for (auto &&i : t) vec.emplace_back(static_cast<decltype(i) &&>(i));
        return vec;
    }
};

/******************************************************************************/

static char const *cast_bug_message = "FromInput().check() returned false but FromInput()() was still called";

// should make this into struct of T, U?
/// Default behavior for casting a variant to a desired argument type
template <class T, class=void>
struct FromInput {
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

    bool check(Any const &u) const {return std::any_cast<no_qualifier<T>>(&u);}
    bool check(AnyRef u) const {return std::any_cast<no_qualifier<T>>(u);}

    bool check(BinaryRef u) const {return std::is_convertible_v<Binary const &, T>;}

    T operator()(BinaryRef u) const {
        if constexpr(std::is_convertible_v<Binary const &, T>) return static_cast<T>(*u);
        else throw std::logic_error(cast_bug_message);
    }

    T operator()(Any &&u) const {return std::any_cast<T &&>(std::move(u));}

    T operator()(AnyRef u) const {return static_cast<T>(std::any_cast<T>(*u));}
};

/******************************************************************************/

template <class T>
struct FromInput<Vector<T>, std::enable_if_t<!std::is_same_v<T, Input>>> {
    template <class U>
    bool check(U const &) const {return false;}

    bool check(Vector<Input> const &u) const {
        for (auto const &x : u)
            if (!std::visit([&](auto &x) {return FromInput<T>().check(x);}, x.var)) return false;
        return true;
    }

    Vector<T> operator()(Vector<Input> &&u) const {
        Vector<T> out;
        out.reserve(u.size());
        for (auto &x : u)
            std::visit([&](auto &x) {out.emplace_back(FromInput<T>()(std::move(x)));}, x.var);
        return out;
    }

    template <class U>
    Vector<T> operator()(U const &) const {throw std::logic_error("shouldn't be used");}
};

/******************************************************************************/

struct SequenceConcept {
    virtual std::unique_ptr<SequenceConcept> clone() const = 0;
    virtual void scan(std::function<void(Output)> const &) const = 0;
    virtual void fill(Output *) const = 0;
    virtual ~SequenceConcept() {};
};

template <class T>
struct SequenceModel : SequenceConcept {
    T value;
    SequenceModel(T &&t) : value(static_cast<T &&>(t)) {}
    SequenceModel(T const &t) : value(static_cast<T const &>(t)) {}

    virtual std::unique_ptr<SequenceConcept> clone() const override {
        return std::make_unique<SequenceModel>(*this);
    }
    virtual void scan(std::function<void(Output)> const &f) const override {
        for (auto &&v : value) f(Output(v));
    }
    virtual void fill(Output *o) const override {
        for (auto &&v : value) *(o++) = Output(v);
    }
};

class Sequence {
    std::unique_ptr<SequenceConcept> self;
    std::size_t m_size = 0;

public:
    Sequence() = default;

    std::size_t size() const {return m_size;}

    template <class T>
    Sequence(T&&t, std::size_t n) : self(std::make_unique<SequenceModel<no_qualifier<T>>>(t)), m_size(n) {}

    template <class T>
    explicit Sequence(T &&t) : Sequence(static_cast<T &&>(t), std::size(t)) {}

    Sequence(Sequence const &s) : self(s.self ? std::unique_ptr<SequenceConcept>(): s.self->clone()), m_size(s.m_size) {}

    Sequence & operator=(Sequence const &s) {
        if (s.self) self = s.self->clone();
        else self.reset();
        m_size = s.m_size;
        return *this;
    }

    Sequence(Sequence &&) noexcept = default;
    Sequence & operator=(Sequence &&s) noexcept = default;
};













}
