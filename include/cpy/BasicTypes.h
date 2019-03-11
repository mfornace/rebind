#pragma once
#include "Variable.h"
#include "Conversions.h"
#include <cstdlib>

namespace cpy {

/******************************************************************************/

#ifdef INTPTR_MAX
    using Integer = std::intptr_t;
#else
    using Integer = std::ptrdiff_t;
#endif

using Real = double;

using Sequence = Vector<Variable>;

using Dictionary = Vector<std::pair<std::string_view, Variable>>;

/******************************************************************************/

struct ArrayData {
    Vector<Integer> shape, strides;
    TypeIndex type;
    void *data;
    bool mutate;

    template <class T>
    T * target() const {
        if (!mutate && !std::is_const<T>::value) return nullptr;
        if (type != typeid(std::remove_cv_t<T>)) return nullptr;
        return static_cast<T *>(data);
    }

    template <class V=Vector<Integer>, class U=Vector<Integer>>
    ArrayData(void *p, TypeIndex t, bool mut, V const &v, U const &u)
        : shape(std::begin(v), std::end(v)), strides(std::begin(u), std::end(u)), type(t), data(p), mutate(mut) {}

    template <class T, class V=Vector<Integer>, class U=Vector<Integer>>
    ArrayData(T *t, V const &v, U const &u)
        : ArrayData(const_cast<std::remove_cv_t<T> *>(static_cast<T const *>(t)),
                    typeid(std::remove_cv_t<T>), std::is_const_v<T>, v, u) {}
};

/******************************************************************************/

template <>
struct Response<char const *> {
    bool operator()(Variable &out, TypeIndex const &t, char const *s) const {
        if (t.equals<std::string_view>()) {
            out = s ? std::string_view(s) : std::string_view();
            return true;
        } else if (t.equals<std::string>()) {
            if (s) out.emplace(Type<std::string>(), s);
            else out.emplace(Type<std::string>());
            return true;
        } else return false;
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
    bool operator()(Variable &out, TypeIndex const &t, BinaryData const &v) const {
        if (t.equals<BinaryView>()) return out.emplace(Type<BinaryView>(), v.begin(), v.size()), true;
        return false;
    }
};

template <>
struct Response<BinaryView> {
    bool operator()(Variable &out, TypeIndex const &, BinaryView const &v) const {
        return false;
    }
};

template <>
struct Request<BinaryView> {
    std::optional<BinaryView> operator()(Variable const &v, Dispatch &msg) const {
        if (auto p = v.request<BinaryData>()) return BinaryView(p->data(), p->size());
        return msg.error("not convertible to binary view", typeid(BinaryView));
    }
};

template <>
struct Request<BinaryData> {
    std::optional<BinaryData> operator()(Variable const &v, Dispatch &msg) const {
        return msg.error("not convertible to binary data", typeid(BinaryData));
    }
};

/******************************************************************************/

template <class T>
struct Response<T, Value, std::enable_if_t<(std::is_integral_v<T>)>> {
    bool operator()(Variable &out, TypeIndex const &i, T t) const {
        DUMP("response from integer", typeid(T).name(), i.name());
        if (i == typeid(Integer)) return out = static_cast<Integer>(t), true;
        if (i == typeid(Real)) return out = static_cast<Real>(t), true;
        DUMP("no response from integer");
        return false;
    }
};

template <class T>
struct Response<T, Value, std::enable_if_t<(std::is_floating_point_v<T>)>> {
    bool operator()(Variable &out, TypeIndex const &i, T t) const {
        if (i == typeid(Real)) return out = static_cast<Real>(t), true;
        if (i == typeid(Integer)) return out = static_cast<Integer>(t), true;
        return false;
    }
};

template <class T>
struct Request<T, std::enable_if_t<std::is_floating_point_v<T>>> {
    std::optional<T> operator()(Variable const &v, Dispatch &msg) const {
        DUMP("convert to floating");
        if (!std::is_same_v<Real, T>) if (auto p = v.request<Real>()) return static_cast<T>(*p);
        return msg.error("not convertible to floating point", typeid(T));
    }
};

template <class T>
struct Request<T, std::enable_if_t<std::is_integral_v<T>>> {
    std::optional<T> operator()(Variable const &v, Dispatch &msg) const {
        DUMP("trying convert to arithmetic", v.name(), typeid(T).name());
        if (!std::is_same_v<Integer, T>) if (auto p = v.request<Integer>()) return static_cast<T>(*p);
        DUMP("failed to convert to arithmetic", v.name(), typeid(T).name());
        return msg.error("not convertible to integer", typeid(T));
    }
};


template <class T, class Traits, class Alloc>
struct Request<std::basic_string<T, Traits, Alloc>> {
    std::optional<std::basic_string<T, Traits, Alloc>> operator()(Variable const &v, Dispatch &msg) const {
        DUMP("trying to convert to string");
        if (auto p = v.request<std::basic_string_view<T, Traits>>())
            return std::basic_string<T, Traits, Alloc>(std::move(*p));
        if (!std::is_same_v<std::basic_string<T, Traits, Alloc>, std::basic_string<T, Traits>>)
            if (auto p = v.request<std::basic_string<T, Traits>>())
                return std::move(*p);
        return msg.error("not convertible to string", typeid(T));
    }
};

template <class T, class Traits>
struct Request<std::basic_string_view<T, Traits>> {
    std::optional<std::basic_string_view<T, Traits>> operator()(Variable const &v, Dispatch &msg) const {
        return msg.error("not convertible to string view", typeid(T));
    }
};


/******************************************************************************/

template <class V>
struct CompiledSequenceResponse {
    using Array = std::array<Variable, std::tuple_size_v<V>>;

    template <std::size_t ...Is>
    static Sequence sequence(V const &v, std::index_sequence<Is...>) {
        Sequence o;
        o.reserve(sizeof...(Is));
        (o.emplace_back(std::get<Is>(v)), ...);
        return o;
    }

    template <std::size_t ...Is>
    static Sequence sequence(V &&v, std::index_sequence<Is...>) {
        Sequence o;
        o.reserve(sizeof...(Is));
        (o.emplace_back(std::get<Is>(std::move(v))), ...);
        return o;
    }

    template <std::size_t ...Is>
    static Array array(V const &v, std::index_sequence<Is...>) {return {std::get<Is>(v)...};}

    template <std::size_t ...Is>
    static Array array(V &&v, std::index_sequence<Is...>) {return {std::get<Is>(std::move(v))...};}

    bool operator()(Variable &out, TypeIndex const &t, V const &v) const {
        auto idx = std::make_index_sequence<std::tuple_size_v<V>>();
        if (t == typeid(Sequence)) return out = sequence(v, idx), true;
        if (t == typeid(Array)) return out = array(v, idx), true;
        return false;
    }

    bool operator()(Variable &out, TypeIndex const &t, V &&v) const {
        auto idx = std::make_index_sequence<std::tuple_size_v<V>>();
        if (t == typeid(Sequence)) return out = sequence(std::move(v), idx), true;
        if (t == typeid(Array)) return out = array(std::move(v), idx), true;
        return false;
    }
};

template <class T>
struct Response<T, Value, std::enable_if_t<std::tuple_size<T>::value >= 0>> : CompiledSequenceResponse<T> {};

/******************************************************************************/
template <class V>
struct CompiledSequenceRequest {
    using Array = std::array<Variable, std::tuple_size_v<V>>;

    template <class ...Ts>
    static void combine(std::optional<V> &out, Ts &&...ts) {
        DUMP("trying CompiledSequenceRequest combine", bool(ts)...);
        if ((bool(ts) && ...)) out = V{*static_cast<Ts &&>(ts)...};
    }

    template <class S, std::size_t ...Is>
    static void request_each(std::optional<V> &out, S &&s, Dispatch &msg, std::index_sequence<Is...>) {
        DUMP("trying CompiledSequenceRequest request");
        combine(out, std::move(s[Is]).request(msg, Type<std::tuple_element_t<Is, V>>())...);
    }

    template <class S>
    static void request(std::optional<V> &out, S &&s, Dispatch &msg) {
        DUMP("trying CompiledSequenceRequest request");
        if (std::size(s) != std::tuple_size_v<V>) {
            msg.error("wrong sequence length", typeid(V), std::tuple_size_v<V>, s.size());
        } else {
            msg.indices.emplace_back(0);
            request_each(out, std::move(s), msg, std::make_index_sequence<std::tuple_size_v<V>>());
            msg.indices.pop_back();
        }
    }

    std::optional<V> operator()(Variable r, Dispatch &msg) const {
        std::optional<V> out;
        DUMP("trying CompiledSequenceRequest", r.type().name());
        if constexpr(!std::is_same_v<V, Array>) {
            if (auto p = r.request<std::array<Variable, std::tuple_size_v<V>>>()) {
                DUMP("trying array CompiledSequenceRequest2", r.type().name());
                request(out, std::move(*p), msg);
            }
            return out;
        }
        if (auto p = r.request<Sequence>()) {
            DUMP("trying CompiledSequenceRequest2", r.type().name());
            request(out, std::move(*p), msg);
        } else {
            DUMP("trying CompiledSequenceRequest3", r.type().name());
            msg.error("expected sequence to make compiled sequence", typeid(V));
        }
        return out;
    }
};

/// The default implementation is to accept convertible arguments or Variable of the exact typeid match
template <class T>
struct Request<T, std::enable_if_t<std::tuple_size<T>::value >= 0>> : CompiledSequenceRequest<T> {};

/******************************************************************************/

template <class R, class V>
R from_iters(V &&v) {return R(std::make_move_iterator(std::begin(v)), std::make_move_iterator(std::end(v)));}

template <class R, class V>
R from_iters(V const &v) {return R(std::begin(v), std::end(v));}

/******************************************************************************/

template <class T, class=void>
struct HasData : std::false_type {};

template <class T>
struct HasData<T, std::enable_if_t<(!std::is_pointer_v<decltype(std::data(std::declval<T>()))>)>> : std::true_type {};

/******************************************************************************/

template <class V>
class RuntimeReference {
    V *ptr;
    Qualifier qual;

public:
    Qualifier qualifier() const {return qual;}

    RuntimeReference(V &v) : ptr(std::addressof(v)), qual(Lvalue) {}
    RuntimeReference(V &&v) : ptr(std::addressof(v)), qual(Rvalue) {}
    RuntimeReference(V const &v) : ptr(const_cast<V *>(std::addressof(v))), qual(Const) {}

    template <Qualifier Q>
    std::remove_reference_t<qualified<V, Q>> *target() {return qual == Q ? ptr : nullptr;}
};

/******************************************************************************/

/// wraps e.g. vector const &, vector &, and vector && into one convenient package for subsequent conversions
/// assuming that the container lifetime is the same as the contents lifetime
template <class V>
struct ValueContainer : RuntimeReference<V> {
    using RuntimeReference<V>::RuntimeReference;

    template <class O, std::enable_if_t<std::is_constructible_v<O, decltype(std::begin(std::declval<V const &>())), decltype(std::end(std::declval<V const &>()))>, int> = 0>
    explicit operator O() && {
        if (auto p = this->template target<Rvalue>())
            return {std::make_move_iterator(std::begin(std::move(*p))), std::make_move_iterator(std::end(std::move(*p)))};
        else return {std::begin(*p), std::end(*p)};
    }
};

/******************************************************************************/

template <class V>
struct VectorResponse {
    using T = unqualified<typename V::value_type>;

    bool operator()(Variable &out, TypeIndex const &t, ValueContainer<V> &&v) const {
        if (t.equals<Sequence>()) return out.emplace(Type<Sequence>(), std::move(v)), true;
        if (t.equals<Vector<T>>()) return out.emplace(Type<Vector<T>>(), std::move(v)), true;
        if (v.qualifier() != Rvalue && t.equals<ArrayData>()) {
            // e.g. guard against std::vector<bool>
            if constexpr(HasData<V const &>::value) if (auto p = v.template target<Const>())
                return out.emplace(Type<ArrayData>(), std::data(*p), {Integer(std::size(*p))}, {Integer(1)}), true;
            if constexpr(HasData<V &>::value) if (auto p = v.template target<Lvalue>())
                return out.emplace(Type<ArrayData>(), std::data(*p), {Integer(std::size(*p))}, {Integer(1)}), true;
        }
        return false;
    }
};

template <class T, class A>
struct Response<std::vector<T, A>, Value, std::enable_if_t<!std::is_same_v<T, Variable>>> : VectorResponse<std::vector<T, A>> {};

/******************************************************************************/

template <class V>
struct VectorRequest {
    using T = unqualified<typename V::value_type>;

    template <class P>
    static std::optional<V> get(P &pack, Dispatch &msg) {
        V out;
        out.reserve(pack.size());
        msg.indices.emplace_back(0);
        for (auto &x : pack) {
            if (auto p = std::move(x).request(msg, Type<T>()))
                out.emplace_back(std::move(*p));
            else return msg.error();
            ++msg.indices.back();
        }
        msg.indices.pop_back();
        return out;
    }

    std::optional<V> operator()(Variable const &v, Dispatch &msg) const {
        // if (auto p = v.request<Vector<T>>()) return get(*p, msg);
        if (!std::is_same_v<V, Sequence>)
            if (auto p = v.request<Sequence>()) return get(*p, msg);
        return msg.error("expected sequence", typeid(V));
    }
};

template <class T, class A>
struct Request<std::vector<T, A>> : VectorRequest<std::vector<T, A>> {};

}
