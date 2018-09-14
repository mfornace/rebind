#pragma once
#include "Value.h"
#include <cstdlib>

namespace cpy {

/******************************************************************************/

template <>
struct ToValue<char const *> {
    Value operator()(char const *s, TypeRequest const &t) const {
        if (t.contains(typeid(std::string_view)))
            return s ? std::string_view() : std::string_view(s);
        return s ? std::string(s) : std::string();
    }
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
struct ToValue<BinaryData> {
    Value operator()(BinaryData const &v, TypeRequest const &t) const {
        if (t.contains(typeid(BinaryView))) return BinaryView(v.begin(), v.size());
        return Binary(v.begin(), v.size());
    }
};

template <>
struct ToValue<BinaryView> {
    Value operator()(BinaryView const &v, TypeRequest const &) const {return Binary(v.begin(), v.size());}
};

template <>
struct FromReference<BinaryView> {
    BinaryView operator()(Reference const &r, Dispatch &msg) const {
        auto v = r.value({typeid(BinaryView), typeid(BinaryData)});
        if (auto p = std::any_cast<BinaryView>(&v))
            return std::move(*p);
        if (auto p = std::any_cast<BinaryData>(&v))
            return {p->data(), p->size()};
        throw msg.error("not convertible to BinaryView");
    }
};

template <>
struct FromReference<BinaryData> {
    BinaryData operator()(Reference const &r, Dispatch &msg) const {
        auto v = r.value({typeid(BinaryData)});
        if (auto p = std::any_cast<BinaryData>(&v))
            return {p->data(), p->size()};
        throw msg.error("not convertible to BinaryData");
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
    Value operator()(T t, TypeRequest const &req) const {
        return Value::from_any(static_cast<Integer>(t));
    }
};

template <class T>
struct ToValue<T, std::enable_if_t<(std::is_floating_point_v<T>)>> {
    Value operator()(T t, TypeRequest const &) const {
        return Value::from_any(static_cast<Real>(t));
    }
};

template <class T>
struct FromReference<T, std::enable_if_t<std::is_arithmetic_v<T>>> {
    T operator()(Reference const &r, Dispatch &msg) const {
        std::cout << "convert to arithmetic" << std::endl;
        Value v = r.value({typeid(Real), typeid(Integer), typeid(bool)});
        if (auto p = std::any_cast<Real>(&v))    return static_cast<T>(*p);
        if (auto p = std::any_cast<Integer>(&v)) return static_cast<T>(*p);
        if (auto p = std::any_cast<bool>(&v))    return static_cast<T>(*p);
        throw msg.error("not convertible to arithmetic value", r.type(), typeid(T));
    }
};

template <class T, class Traits, class Alloc>
struct FromReference<std::basic_string<T, Traits, Alloc>> {
    std::basic_string<T, Traits, Alloc> operator()(Reference const &r, Dispatch &msg) const {
        std::cout << "trying to convert to string" << std::endl;
        Value v = r.value({typeid(std::basic_string<T, Traits, Alloc>),
                           typeid(std::basic_string_view<T, Traits>),
                           typeid(std::basic_string<T, Traits>)});
        if (auto p = std::any_cast<std::basic_string<T, Traits, Alloc>>(&v))
            return std::move(*p);
        if (auto p = std::any_cast<std::basic_string_view<T, Traits>>(&v))
            return std::basic_string<T, Traits, Alloc>(std::move(*p));
        if (auto p = std::any_cast<std::basic_string<T, Traits>>(&v))
            return std::move(*p);
        throw msg.error("not convertible to arithmetic value", r.type(), typeid(T));
    }
};

template <class T, class Traits>
struct FromReference<std::basic_string_view<T, Traits>> {
    std::basic_string_view<T, Traits> operator()(Reference const &r, Dispatch &msg) const {
        Value v = r.value({typeid(std::basic_string_view<T, Traits>)});
        if (auto p = std::any_cast<std::basic_string_view<T, Traits>>(&v))
            return std::move(*p);
        throw msg.error("not convertible to string view", r.type(), typeid(T));
    }
};

/******************************************************************************/

// template <class T>
// struct Sequence {
//     Vector<T> contents;
//     // Vector<std::size_t> shape;
//     Sequence() = default;
//     Sequence(std::initializer_list<T> const &v) : contents(v) {}

//     auto size() const {return contents.size();}

//     template <class V, std::enable_if_t<std::is_constructible_v<T, decltype(*std::begin(std::declval<V &&>()))>, int> = 0>
//     explicit Sequence(V &&v) {
//         contents.reserve(std::size(v));
//         for (auto &&x : v) contents.emplace_back(static_cast<decltype(x) &&>(x));
//     }

//     // template <class T, class ...Keys>
//     // T unzip(Dispatch &msg, Keys &&...keys) const & {
//     //     if (shape.size() < 2) {
//     //         if (contents.size() != sizeof...(Keys)) throw msg.error("wrong number of keys");
//     //         for (auto const &c : contents) {
//     //             auto s = std::any_cast<Sequence>(&c.any);
//     //             if (!s) throw msg.error("not sequence");
//     //             if (s->size() != 2) throw msg.error("should be length 2");
//     //         }
//     //         auto get = [](auto const &k) {return std::find_if(contents.begin(), contents.end(), []() {
//     //             return std::any_cast<Sequence>()
//     //         })}
//     //         return T{}
//     //     }
//     //     throw msg.error("not implemented");
//     // }
// };

using ArgPack = Vector<Reference>;
// struct ArgPack : Sequence<Reference> {
//     using Sequence<Reference>::Sequence;

//     template <class ...Ts>
//     static ArgPack from_values(Ts &&...ts) {
//         ArgPack out;
//         out.contents.reserve(sizeof...(Ts));
//         (out.contents.emplace_back(static_cast<Ts &&>(ts)), ...);
//         return out;
//     }
// };

// struct ValuePack : Sequence<Value> {
//     using Sequence<Value>::Sequence;

//     operator ArgPack() const & {
//         ArgPack p;
//         p.contents.reserve(this->contents.size());
//         for (auto const &x : this->contents) p.contents.emplace_back(x);
//         return p;
//     }

//     operator ArgPack() && {
//         ArgPack p;
//         p.contents.reserve(this->contents.size());
//         for (auto &x : this->contents) p.contents.emplace_back(std::move(x));
//         return p;
//     }

//     template <class ...Ts>
//     static ValuePack from_values(Ts &&...ts) {
//         ValuePack out;
//         out.contents.reserve(sizeof...(Ts));
//         (out.contents.emplace_back(static_cast<Ts &&>(ts)), ...);
//         return out;
//     }
// };

/******************************************************************************/

// template <class T, class Alloc>
// struct ToValue<std::vector<T, Alloc>> {
//     Value operator()(std::vector<T, Alloc> t) const {return {Type<ValuePack>(), std::move(t)};}
// };

// template <class T>
// struct FromReference<T, std::void_t<decltype(from_reference(+Type<T>(), ValuePack(), std::declval<Dispatch &>()))>> {
//     // The common return type between the following 2 visitor member functions
//     using out_type = std::remove_cv_t<decltype(false ? std::declval<T &&>() :
//         from_reference(Type<T>(), std::declval<Value &&>(), std::declval<Dispatch &>()))>;

//     out_type operator()(Value u, Dispatch &msg) const {
//         msg.source = u.type();
//         msg.dest = typeid(T);
//         return from_reference(+Type<T>(), std::move(u), msg);
//     }
// };

/******************************************************************************/

// template <class V>
// struct VectorFromArg {
//     using T = no_qualifier<typename V::value_type>;

//     template <class P>
//     static V get(P &&pack, Dispatch &msg) {
//         V out;
//         out.reserve(pack.contents.size());
//         msg.indices.emplace_back(0);
//         for (auto &&x : pack.contents) {
//             using X0 = std::remove_reference_t<decltype(x)>;
//             using X = std::conditional_t<std::is_const_v<X0>, std::remove_cv_t<X0>, X0 &&>;
//             out.emplace_back(downcast<T>(static_cast<X>(x), msg));
//             ++msg.indices.back();
//         }
//         msg.indices.pop_back();
//         return out;
//     }

//     V operator()(Reference &outarg, Reference &&in, Dispatch &msg) const {
//         if (auto p = std::any_cast<ArgPack>(&in)) return get(std::move(*p), msg);
//         if (auto p = std::any_cast<ValuePack>(&in)) return get(std::move(*p), msg);
//         throw msg.error("expected sequence", in.type(), typeid(V));
//     }

//     V operator()(Reference &outarg, Reference const &in, Dispatch &msg) const {
//         if (auto p = std::any_cast<ArgPack>(&in)) return get(*p, msg);
//         if (auto p = std::any_cast<ValuePack>(&in)) return get(*p, msg);
//         throw msg.error("expected sequence", in.type(), typeid(V));
//     }
// };

// template <class T>
// struct FromReference<Vector<T>> : VectorFromArg<Vector<T>> {};

}