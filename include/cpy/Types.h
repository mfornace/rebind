#pragma once
#include "Variable.h"
#include "Conversions.h"
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
struct Response<char const *> {
    void operator()(Variable &out, char const *s, std::type_index const &t) const {
        if (t == typeid(std::string_view))
            out = s ? std::string_view(s) : std::string_view();
        else if (t == typeid(std::string))
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

template <>
struct Response<BinaryData> {
    void operator()(Variable &out, BinaryData const &v, std::type_index t) const {
        if (t == typeid(BinaryView)) out = BinaryView(v.begin(), v.size());
        else out = Binary(v.begin(), v.size());
    }
};

template <>
struct Response<BinaryView> {
    void operator()(Variable &out, BinaryView const &v, std::type_index) const {
        out = Binary(v.begin(), v.size());
    }
};

template <>
struct Request<BinaryView> {
    std::optional<BinaryView> operator()(Variable const &v, Dispatch &msg) const {
        if (auto p = v.request<BinaryData>()) return BinaryView(p->data(), p->size());
        return msg.error("not convertible to binary view", v.type(), typeid(BinaryView));
    }
};

template <>
struct Request<BinaryData> {
    std::optional<BinaryData> operator()(Variable const &v, Dispatch &msg) const {
        return msg.error("not convertible to binary data", v.type(), typeid(BinaryData));
    }
};

/******************************************************************************/

template <class T>
struct Response<T, std::enable_if_t<(std::is_integral_v<T>)>> {
    void operator()(Variable &out, T t, std::type_index i) const {
        if (i == typeid(Integer)) out = static_cast<Integer>(t);
        if (i == typeid(Real)) out = static_cast<Real>(t);
    }
};

template <class T>
struct Response<T, std::enable_if_t<(std::is_floating_point_v<T>)>> {
    void operator()(Variable &out, T t, std::type_index i) const {
        if (i == typeid(Real)) out = static_cast<Real>(t);
        if (i == typeid(Integer)) out = static_cast<Integer>(t);
    }
};

template <class T>
struct Request<T, std::enable_if_t<std::is_floating_point_v<T>>> {
    std::optional<T> operator()(Variable const &v, Dispatch &msg) const {
        if (Debug) std::cout << "convert to floating" << std::endl;
        if (!std::is_same_v<Real, T>)    if (auto p = v.request<Real>())    return static_cast<T>(*p);
        return msg.error("not convertible to floating point", v.type(), typeid(T));
    }
};

template <class T>
struct Request<T, std::enable_if_t<std::is_integral_v<T>>> {
    std::optional<T> operator()(Variable const &v, Dispatch &msg) const {
        if (Debug) std::cout << "convert to arithmetic" << std::endl;
        if (!std::is_same_v<Integer, T>) if (auto p = v.request<Integer>()) return static_cast<T>(*p);
        return msg.error("not convertible to integer", v.type(), typeid(T));
    }
};


template <class T, class Traits, class Alloc>
struct Request<std::basic_string<T, Traits, Alloc>> {
    std::optional<std::basic_string<T, Traits, Alloc>> operator()(Variable const &v, Dispatch &msg) const {
        if (Debug) std::cout << "trying to convert to string" << std::endl;
        if (auto p = v.request<std::basic_string_view<T, Traits>>())
            return std::basic_string<T, Traits, Alloc>(std::move(*p));
        if (!std::is_same_v<std::basic_string<T, Traits, Alloc>, std::basic_string<T, Traits>>)
            if (auto p = v.request<std::basic_string<T, Traits>>())
                return std::move(*p);
        return msg.error("not convertible to string", v.type(), typeid(T));
    }
};

template <class T, class Traits>
struct Request<std::basic_string_view<T, Traits>> {
    std::optional<std::basic_string_view<T, Traits>> operator()(Variable const &v, Dispatch &msg) const {
        return msg.error("not convertible to string view", v.type(), typeid(T));
    }
};

/******************************************************************************/

using ArgPack = Vector<Variable>;

/******************************************************************************/

template <class V>
struct SimplifyVector {
    void operator()(Variable &out, V v, std::type_index t) const {
        if (t == typeid(Vector<Variable>)) {
            Vector<Variable> o;
            o.reserve(std::size(v));
            for (auto &&x : v) o.emplace_back(static_cast<decltype(x) &&>(x));
            out = std::move(o);
        } else if (t == typeid(Vector<Variable>)) {
            out = Vector<Variable>(std::begin(v), std::end(v));
        }
    }
};

template <class T>
struct Response<Vector<T>, std::enable_if_t<!std::is_same_v<T, Variable>>> : SimplifyVector<Vector<T>> {};

/******************************************************************************/

template <class V>
struct VectorRequest {
    using T = no_qualifier<typename V::value_type>;

    template <class P>
    static std::optional<V> get(P &pack, Dispatch &msg) {
        V out;
        out.reserve(pack.size());
        msg.indices.emplace_back(0);
        for (auto &x : pack) {
            if (auto p = std::move(x).request(msg, true, Type<T>()))
                out.emplace_back(std::move(*p));
            else return msg.error();
            ++msg.indices.back();
        }
        msg.indices.pop_back();
        return out;
    }

    std::optional<V> operator()(Variable const &v, Dispatch &msg) const {
        if (auto p = v.request<Vector<Variable>>()) return get(*p, msg);
        return msg.error("expected sequence", v.type(), typeid(V));
    }
};

template <class T>
struct Request<Vector<T>> : VectorRequest<Vector<T>> {};

}