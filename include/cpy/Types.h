#pragma once
#include "Value.h"
#include <cstdlib>

namespace cpy {


#ifdef INTPTR_MAX
    using Integer = std::intptr_t;
#else
    using Integer = std::ptrdiff_t;
#endif

using Real = double;

struct ArrayData {
    Vector<Integer> shape, strides;
    std::type_index type;
    void *data;
    bool mutate;

    template <class T>
    T * target() const {
        if (!mutate && !std::is_const<T>::value) return nullptr;
        if (type != typeid(std::remove_cv_t<T>)) return nullptr;
        return static_cast<T *>(data);
    }

    template <class V, class U>
    ArrayData(void *p, std::type_index t, bool mut, V const &v, U const &u)
        : shape(std::begin(v), std::end(v)), strides(std::begin(u), std::end(u)), type(t), data(p), mutate(mut) {}

    template <class T, class V, class U>
    ArrayData(T *t, V const &v, U const &u)
        : ArrayData(const_cast<std::remove_cv_t<T> *>(static_cast<T const *>(t)),
                    typeid(std::remove_cv_t<T>), std::is_const_v<T>, v, u) {}
};

/******************************************************************************/

template <>
struct Simplify<char const *> {
    void operator()(Value &out, char const *s, std::type_index const &t) const {
        if (t == typeid(std::string_view))
            out = s ? std::string_view() : std::string_view(s);
        else
            out = s ? std::string(s) : std::string();
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

template <class V>
BinaryView make_binary_view(V const &v) {
    using T = no_qualifier<decltype(*std::begin(v))>;
    static_assert(__STDCPP_DEFAULT_NEW_ALIGNMENT__ >= alignof(T));
    static_assert(std::is_trivially_destructible_v<T>);
    auto b = reinterpret_cast<unsigned char const *>(std::addressof(*std::begin(v)));
    return {b, static_cast<std::size_t>(reinterpret_cast<unsigned char const *>(std::addressof(*std::end(v))) - b)};
}

template <>
struct Simplify<BinaryData> {
    void operator()(Value &out, BinaryData const &v, std::type_index t) const {
        if (t == typeid(BinaryView)) out = BinaryView(v.begin(), v.size());
        else out = Binary(v.begin(), v.size());
    }
};

template <>
struct Simplify<BinaryView> {
    void operator()(Value &out, BinaryView const &v, std::type_index) const {
        out = Binary(v.begin(), v.size());
    }
};

template <>
struct FromReference<BinaryView> {
    BinaryView operator()(Reference const &r, Dispatch &msg) const {
        if (auto p = r.request<BinaryView>())
            return std::move(*p);
        if (auto p = r.request<BinaryData>())
            return {p->data(), p->size()};
        throw msg.error("not convertible to BinaryView");
    }
};

template <>
struct FromReference<BinaryData> {
    BinaryData operator()(Reference const &r, Dispatch &msg) const {
        if (auto p = r.request<BinaryData>())
            return {p->data(), p->size()};
        throw msg.error("not convertible to BinaryData");
    }
};

/******************************************************************************/

template <class T>
struct Simplify<T, std::enable_if_t<(std::is_integral_v<T>)>> {
    void operator()(Value &out, T t, std::type_index i) const {
        if (i == typeid(Integer)) out = static_cast<Integer>(t);
    }
};

template <class T>
struct Simplify<T, std::enable_if_t<(std::is_floating_point_v<T>)>> {
    void operator()(Value &out, T t, std::type_index i) const {
        if (i == typeid(Real)) out = static_cast<Real>(t);
    }
};

template <class T>
struct FromReference<T, std::enable_if_t<std::is_arithmetic_v<T>>> {
    T operator()(Reference const &r, Dispatch &msg) const {
        if (Debug) std::cout << "convert to arithmetic" << std::endl;
        if (auto p = r.request<Real>())    return static_cast<T>(*p);
        if (auto p = r.request<Integer>()) return static_cast<T>(*p);
        if (auto p = r.request<bool>())    return static_cast<T>(*p);
        throw msg.error("not convertible to arithmetic value", r.type(), typeid(T));
    }
};

template <class T, class Traits, class Alloc>
struct FromReference<std::basic_string<T, Traits, Alloc>> {
    std::basic_string<T, Traits, Alloc> operator()(Reference const &r, Dispatch &msg) const {
        if (Debug) std::cout << "trying to convert to string" << std::endl;
        if (auto p = r.request<std::basic_string<T, Traits, Alloc>>())
            return std::move(*p);
        if (auto p = r.request<std::basic_string_view<T, Traits>>())
            return std::basic_string<T, Traits, Alloc>(std::move(*p));
        if (auto p = r.request<std::basic_string<T, Traits>>())
            return std::move(*p);
        throw msg.error("not convertible to arithmetic value", r.type(), typeid(T));
    }
};

template <class T, class Traits>
struct FromReference<std::basic_string_view<T, Traits>> {
    std::basic_string_view<T, Traits> operator()(Reference const &r, Dispatch &msg) const {
        if (auto p = r.request<std::basic_string_view<T, Traits>>())
            return std::move(*p);
        throw msg.error("not convertible to string view", r.type(), typeid(T));
    }
};

/******************************************************************************/

using ArgPack = Vector<Reference>;

/******************************************************************************/

template <class V>
struct SimplifyVector {
    void operator()(Value &out, V v, std::type_index t) const {
        if (t == typeid(Vector<Value>)) {
            Vector<Value> o;
            o.reserve(std::size(v));
            for (auto &&x : v) o.emplace_back(static_cast<decltype(x) &&>(x));
            out = std::move(o);
        } else if (t == typeid(Vector<Reference>)) {
            out = Vector<Reference>(std::begin(v), std::end(v));
        }
    }
};

template <class T>
struct Simplify<Vector<T>, std::enable_if_t<!std::is_same_v<T, Reference>>> : SimplifyVector<Vector<T>> {};

/******************************************************************************/

template <class V>
struct VectorFromReference {
    using T = no_qualifier<typename V::value_type>;

    template <class P>
    static V get(P &pack, Dispatch &msg) {
        V out;
        out.reserve(pack.size());
        msg.indices.emplace_back(0);
        for (auto &x : pack) {
            out.emplace_back(downcast<T>(Reference(std::move(x)), msg));
            ++msg.indices.back();
        }
        msg.indices.pop_back();
        return out;
    }

    V operator()(Reference const &r, Dispatch &msg) const {
        if (auto p = r.request<Vector<Reference>>()) return get(*p, msg);
        if (auto p = r.request<Vector<Value>>()) return get(*p, msg);
        throw msg.error("expected sequence", r.type(), typeid(V));
    }
};

template <class T>
struct FromReference<Vector<T>> : VectorFromReference<Vector<T>> {};

}