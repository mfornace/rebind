#pragma once
#include "Value.h"
#include "Conversions.h"
#include <cstdlib>

namespace rebind {

/******************************************************************************/

/// Built-in integer with the largest domain
#ifdef INTPTR_MAX
    using Integer = std::intptr_t;
#else
    using Integer = std::ptrdiff_t;
#endif

/// Built-in floating point with the largest domain -- long double is not used though.
using Real = double;

using Arguments = Vector<Pointer>;

using Sequence = Vector<Value>;

using Dictionary = Vector<std::pair<std::string_view, Value>>;

/******************************************************************************/

template <class T>
struct Request<T *> {
    std::optional<T *> operator()(Pointer const &v, Scope &s) const {
        std::optional<T *> out;
        if (!v || v.request<std::nullptr_t>(s)) out.emplace(nullptr);
        else if (auto p = v.request<T &>(s)) out.emplace(std::addressof(*p));
        return out;
    }
};

/******************************************************************************/

template <>
struct Response<char const *> {
    Value operator()(TypeIndex const &t, char const *s) const {
        if (t.equals<std::string_view>()) {
            return s ? std::string_view(s) : std::string_view();
        } else if (t.equals<std::string>()) {
            if (s) return Value::from<std::string>(s);
            else return Value::from<std::string>();
        } else return {};
    }
};

template <>
struct Request<char const *> {
    std::optional<char const *> operator()(Pointer const &v, Scope &s) const {
        std::optional<char const *> out;
        if (!v || v.request<std::nullptr_t>()) out.emplace(nullptr);
        else if (auto p = v.request<std::string_view>(s)) out.emplace(p->data());
        else if (auto p = v.request<char const &>(s)) out.emplace(std::addressof(*p));
        return out;
    }
};

/******************************************************************************/

template <class T>
struct Response<T, std::enable_if_t<(std::is_integral_v<T>)>> {
    bool operator()(Value &out, TypeIndex const &i, T t) const {
        DUMP("response from integer", type_index<T>(), i.name());
        if (i == typeid(Integer)) return out = static_cast<Integer>(t), true;
        if (i == typeid(Real)) return out = static_cast<Real>(t), true;
        DUMP("no response from integer");
        return false;
    }
};


/*
Default Response for floating point allows conversion to Real or Integer
*/
template <class T>
struct Response<T, std::enable_if_t<(std::is_floating_point_v<T>)>> {
    bool operator()(Value &out, TypeIndex const &i, T t) const {
        if (i == typeid(Real)) return out = static_cast<Real>(t), true;
        if (i == typeid(Integer)) return out = static_cast<Integer>(t), true;
        return false;
    }
};

/*
Default Request for integer type tries to go through double precision
long double is not expected to be a useful route (it's assumed there are not multiple floating types larger than Real)
*/
template <class T>
struct Request<T, std::enable_if_t<std::is_floating_point_v<T>>> {
    std::optional<T> operator()(Pointer const &v, Scope &msg) const {
        DUMP("convert to floating");
        if (!std::is_same_v<Real, T>) if (auto p = v.request<Real>()) return static_cast<T>(*p);
        return msg.error("not convertible to floating point", typeid(T));
    }
};


/*
Default Request for integer type tries to go through Integer
*/
template <class T>
struct Request<T, std::enable_if_t<std::is_integral_v<T>>> {
    std::optional<T> operator()(Pointer const &v, Scope &msg) const {
        DUMP("trying convert to arithmetic", v.index(), type_index<T>());
        if (!std::is_same_v<Integer, T>) if (auto p = v.request<Integer>()) return static_cast<T>(*p);
        DUMP("failed to convert to arithmetic", v.index(), type_index<T>());
        return msg.error("not convertible to integer", typeid(T));
    }
};

/*
Default Request for enum permits conversion from integer types
*/
template <class T>
struct Request<T, std::enable_if_t<std::is_enum_v<T>>> {
    std::optional<T> operator()(Pointer const &v, Scope &msg) const {
        DUMP("trying convert to enum", v.index(), type_index<T>());
        if (auto p = v.request<std::underlying_type_t<T>>()) return static_cast<T>(*p);
        return msg.error("not convertible to enum", typeid(T));
    }
};

/*
Default Response for enum permits conversion to integer types
*/
template <class T>
struct Response<T, std::enable_if_t<(std::is_enum_v<T>)>> {
    bool operator()(Value &out, TypeIndex const &i, T t) const {
        if (i == typeid(std::underlying_type_t<T>))
            return out = static_cast<std::underlying_type_t<T>>(t), true;
        if (i == typeid(Integer))
            return out = static_cast<Integer>(t), true;
        return false;
    }
};

/*
Default Request for string tries to convert from std::string_view and std::string
*/
template <class T, class Traits, class Alloc>
struct Request<std::basic_string<T, Traits, Alloc>> {
    std::optional<std::basic_string<T, Traits, Alloc>> operator()(Pointer const &v, Scope &msg) const {
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
    std::optional<std::basic_string_view<T, Traits>> operator()(Pointer const &v, Scope &msg) const {
        return msg.error("not convertible to string view", typeid(T));
    }
};


/******************************************************************************/

/*
    Response for CompileSequence concept -- a sequence of compile time length
*/
template <class V>
struct CompiledSequenceResponse {
    using Array = std::array<Value, std::tuple_size_v<V>>;

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

    bool operator()(Value &out, TypeIndex const &t, V const &v) const {
        auto idx = std::make_index_sequence<std::tuple_size_v<V>>();
        if (t == typeid(Sequence)) return out = sequence(v, idx), true;
        if (t == typeid(Array)) return out = array(v, idx), true;
        return false;
    }

    bool operator()(Value &out, TypeIndex const &t, V &&v) const {
        auto idx = std::make_index_sequence<std::tuple_size_v<V>>();
        if (t == typeid(Sequence)) return out = sequence(std::move(v), idx), true;
        if (t == typeid(Array)) return out = array(std::move(v), idx), true;
        return false;
    }
};

template <class T>
struct Response<T, std::enable_if_t<std::tuple_size<T>::value >= 0>> : CompiledSequenceResponse<T> {};

/******************************************************************************/

template <class V>
struct CompiledSequenceRequest {
    using Array = std::array<Value, std::tuple_size_v<V>>;

    template <class ...Ts>
    static void combine(std::optional<V> &out, Ts &&...ts) {
        DUMP("trying CompiledSequenceRequest combine", bool(ts)...);
        if ((bool(ts) && ...)) out = V{*static_cast<Ts &&>(ts)...};
    }

    template <class S, std::size_t ...Is>
    static void request_each(std::optional<V> &out, S &&s, Scope &msg, std::index_sequence<Is...>) {
        DUMP("trying CompiledSequenceRequest request");
        combine(out, std::move(s[Is]).request(msg, Type<std::tuple_element_t<Is, V>>())...);
    }

    template <class S>
    static void request(std::optional<V> &out, S &&s, Scope &msg) {
        DUMP("trying CompiledSequenceRequest request");
        if (std::size(s) != std::tuple_size_v<V>) {
            msg.error("wrong sequence length", typeid(V), std::tuple_size_v<V>, s.size());
        } else {
            msg.indices.emplace_back(0);
            request_each(out, std::move(s), msg, std::make_index_sequence<std::tuple_size_v<V>>());
            msg.indices.pop_back();
        }
    }

    std::optional<V> operator()(Value r, Scope &msg) const {
        std::optional<V> out;
        DUMP("trying CompiledSequenceRequest", r.name());
        if constexpr(!std::is_same_v<V, Array>) {
            if (auto p = r.request<std::array<Value, std::tuple_size_v<V>>>()) {
                DUMP("trying array CompiledSequenceRequest2", r.name());
                request(out, std::move(*p), msg);
            }
            return out;
        }
        if (auto p = r.request<Sequence>()) {
            DUMP("trying CompiledSequenceRequest2", r.name());
            request(out, std::move(*p), msg);
        } else {
            DUMP("trying CompiledSequenceRequest3", r.name());
            msg.error("expected sequence to make compiled sequence", typeid(V));
        }
        return out;
    }
};

/// The default implementation is to accept convertible arguments or Value of the exact typeid match
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
struct HasData<T, std::enable_if_t<(std::is_pointer_v<decltype(std::data(std::declval<T>()))>)>> : std::true_type {};

/******************************************************************************/

template <class T, class Iter1, class Iter2>
bool range_response(Value &o, TypeIndex const &t, Iter1 b, Iter2 e) {
    if (t.equals<Sequence>()) {
        auto &p = o.emplace(Type<Sequence>());
        p.reserve(std::distance(b, e));
        for (; b != e; ++b) p.emplace_back(Type<T>(), *b);
        return true;
    }
    if (t.equals<Vector<T>>()) return o.emplace(Type<Vector<T>>(), b, e), true;
    return false;
}

template <class V>
struct VectorResponse {
    using T = std::decay_t<typename V::value_type>;

    bool operator()(Value &o, TypeIndex const &t, V const &v) const {
        if (range_response<T>(o, t, std::begin(v), std::end(v))) return true;
        // if constexpr(HasData<V const &>::value)
        //     return o.emplace(Type<ArrayView>(), std::data(v), std::size(v)), true;
        return false;
    }

    bool operator()(Value &o, TypeIndex const &t, V &v) const {
        if (range_response<T>(o, t, std::cbegin(v), std::cend(v))) return true;
        // if constexpr(HasData<V &>::value)
        //     return o.emplace(Type<ArrayView>(), std::data(v), std::size(v)), true;
        return false;
    }

    bool operator()(Value &o, TypeIndex const &t, V &&v) const {
        return range_response<T>(o, t, std::make_move_iterator(std::begin(v)), std::make_move_iterator(std::end(v)));
    }
};

template <class T, class A>
struct Response<std::vector<T, A>, std::enable_if_t<!std::is_same_v<T, Value>>> : VectorResponse<std::vector<T, A>> {};

/******************************************************************************/

template <class V>
struct VectorRequest {
    using T = std::decay_t<typename V::value_type>;

    template <class P>
    static std::optional<V> get(P &pack, Scope &msg) {
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

    std::optional<V> operator()(Pointer const &v, Scope &msg) const {
        // if (auto p = v.request<Vector<T>>()) return get(*p, msg);
        if (!std::is_same_v<V, Sequence>)
            if (auto p = v.request<Sequence>()) return get(*p, msg);
        return msg.error("expected sequence", typeid(V));
    }
};

template <class T, class A>
struct Request<std::vector<T, A>> : VectorRequest<std::vector<T, A>> {};

}
