#pragma once
#include "Value.h"
#include <cstdlib>

namespace cpy {

/******************************************************************************/

template <>
struct ToValue<char const *> {
    Value operator()(char const *t) const {return t ? std::string_view(t) : std::string_view();}
};

using Binary = std::basic_string<unsigned char>;

using BinaryView = std::basic_string_view<unsigned char>;

class BinaryData {
    unsigned char *m_begin=nullptr;
    unsigned char *m_end=nullptr;
public:
    constexpr BinaryData() = default;
    constexpr BinaryData(unsigned char *b, std::size_t n) : m_begin(b), m_end(b + n) {}
    constexpr auto begin() const {return m_begin;}
    constexpr auto data() const {return m_begin;}
    constexpr auto end() const {return m_end;}
    constexpr std::size_t size() const {return m_end - m_begin;}
    operator BinaryView() const {return {m_begin, size()};}
};

template <class V>
Binary make_binary(V const &v) {
    using T = no_qualifier<decltype(*std::begin(v))>;
    static_assert(__STDCPP_DEFAULT_NEW_ALIGNMENT__ >= alignof(T));
    static_assert(std::is_trivially_destructible_v<T>);
    return {
        reinterpret_cast<unsigned char const *>(std::addressof(*std::begin(v))),
        reinterpret_cast<unsigned char const *>(std::addressof(*std::end(v))),
    };
}

template <>
struct ToValue<BinaryView> {
    Value operator()(BinaryView v) const {return Binary(v.begin(), v.end());}
};

template <>
struct ToValue<BinaryData> {
    Value operator()(BinaryData v) const {return Binary(v.begin(), v.end());}
};

template <>
struct ToArg<BinaryData> : ToArgFromAny {};

template <>
struct ToArg<BinaryView> : ToArgFromAny {};

template <>
struct FromArg<BinaryView> {
    BinaryView operator()(Arg &out, Arg &&in, Dispatch &msg) const {
        if (auto p = std::any_cast<Reference<Binary &>>(&in))
            return {p->get().data(), p->get().size()};
        if (auto p = std::any_cast<Reference<Binary const &>>(&in))
            return {p->get().data(), p->get().size()};
        if (auto p = std::any_cast<BinaryData>(&in))
            return {p->data(), p->size()};
        return FromArg<BinaryView, bool>()(out, std::move(in), msg);
    }
};

template <>
struct FromArg<BinaryData> {
    BinaryData operator()(Arg &out, Arg &&in, Dispatch &msg) const {
        if (auto p = std::any_cast<Reference<Binary &>>(&in))
            return {p->get().data(), p->get().size()};
        return FromArg<BinaryData, bool>()(out, std::move(in), msg);
    }
};

/******************************************************************************/

#ifdef INTPTR_MAX
using Integer = std::intptr_t;
#else
using Integer = std::ptrdiff_t;
#endif

using Real = double;

template <class T>
struct ToValue<T, std::enable_if_t<(std::is_integral_v<T>)>> {
    Value operator()(T t) const {return Value::from_any(static_cast<Integer>(t));}
};

template <class T>
struct ToValue<T, std::enable_if_t<(std::is_floating_point_v<T>)>> {
    Value operator()(T t) const {return Value::from_any(static_cast<Real>(t));}
};

template <class T>
struct FromValue<T, std::enable_if_t<std::is_arithmetic_v<T>>> {
    T operator()(Value const &u, Dispatch &message) const {
        auto const &t = u.type();
        if (t == typeid(bool)) return static_cast<T>(std::any_cast<bool>(u));
        if (t == typeid(Integer)) return static_cast<T>(std::any_cast<Integer>(u));
        if (t == typeid(Real)) return static_cast<T>(std::any_cast<Real>(u));
        throw message.error("not convertible to arithmetic value", u.type(), typeid(T));
    }
};

/******************************************************************************/

template <class T>
struct Sequence {
    Vector<T> contents;
    // Vector<std::size_t> shape;
    Sequence() = default;
    Sequence(std::initializer_list<T> const &v) : contents(v) {}

    auto size() const {return contents.size();}

    template <class V, std::enable_if_t<std::is_constructible_v<T, decltype(*std::begin(std::declval<V &&>()))>, int> = 0>
    explicit Sequence(V &&v) {
        contents.reserve(std::size(v));
        for (auto &&x : v) contents.emplace_back(static_cast<decltype(x) &&>(x));
    }

    // template <class T, class ...Keys>
    // T unzip(Dispatch &msg, Keys &&...keys) const & {
    //     if (shape.size() < 2) {
    //         if (contents.size() != sizeof...(Keys)) throw msg.error("wrong number of keys");
    //         for (auto const &c : contents) {
    //             auto s = std::any_cast<Sequence>(&c.any);
    //             if (!s) throw msg.error("not sequence");
    //             if (s->size() != 2) throw msg.error("should be length 2");
    //         }
    //         auto get = [](auto const &k) {return std::find_if(contents.begin(), contents.end(), []() {
    //             return std::any_cast<Sequence>()
    //         })}
    //         return T{}
    //     }
    //     throw msg.error("not implemented");
    // }
};

struct ArgPack : Sequence<Arg> {
    using Sequence<Arg>::Sequence;

    template <class ...Ts>
    static ArgPack from_values(Ts &&...ts) {
        ArgPack out;
        out.contents.reserve(sizeof...(Ts));
        (out.contents.emplace_back(static_cast<Ts &&>(ts)), ...);
        return out;
    }
};

struct ValuePack : Sequence<Value> {
    using Sequence<Value>::Sequence;

    operator ArgPack() const & {
        ArgPack p;
        p.contents.reserve(this->contents.size());
        for (auto const &x : this->contents) p.contents.emplace_back(x);
        return p;
    }

    operator ArgPack() && {
        ArgPack p;
        p.contents.reserve(this->contents.size());
        for (auto &x : this->contents) p.contents.emplace_back(std::move(x));
        return p;
    }

    template <class ...Ts>
    static ValuePack from_values(Ts &&...ts) {
        ValuePack out;
        out.contents.reserve(sizeof...(Ts));
        (out.contents.emplace_back(static_cast<Ts &&>(ts)), ...);
        return out;
    }
};

/******************************************************************************/

template <class T, class Alloc>
struct ToValue<std::vector<T, Alloc>> {
    Value operator()(std::vector<T, Alloc> t) const {return {Type<ValuePack>(), std::move(t)};}
};

template <class T>
struct FromValue<T, std::void_t<decltype(from_value(+Type<T>(), ValuePack(), std::declval<Dispatch &>()))>> {
    // The common return type between the following 2 visitor member functions
    using out_type = std::remove_cv_t<decltype(false ? std::declval<T &&>() :
        from_value(Type<T>(), std::declval<Value &&>(), std::declval<Dispatch &>()))>;

    out_type operator()(Value u, Dispatch &msg) const {
        msg.source = u.type();
        msg.dest = typeid(T);
        return from_value(+Type<T>(), std::move(u), msg);
    }
};

/******************************************************************************/

template <class V>
struct VectorFromArg {
    using T = no_qualifier<typename V::value_type>;

    V operator()(Arg &outarg, Arg &&in, Dispatch &msg) const {
        auto v = std::any_cast<ArgPack>(&in);
        if (!v) throw msg.error("expected sequence", in.type(), typeid(V));
        V out;
        out.reserve(v->contents.size());
        msg.indices.emplace_back(0);
        for (auto &x : v->contents) {
            out.emplace_back(cast_value<T>(std::move(x), msg));
            ++msg.indices.back();
        }
        msg.indices.pop_back();
        return out;
    }
};

template <class T>
struct FromArg<Vector<T>> : VectorFromArg<Vector<T>> {};

}