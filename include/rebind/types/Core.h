#pragma once
#include "Arrays.h"
#include <cstdlib>

namespace rebind {

/******************************************************************************/

/// Built-in integer with the largest domain
#ifdef INTPTR_MAX
    using Integer = std::intptr_t;
#else
    using Integer = std::ptrdiff_t;
#endif

/// Built-in floating point with the largest domain -- long double is not used though
using Real = double;

// using ArgVec = Vector<Ref>;

using Sequence = Vector<Value>;

using Dictionary = Vector<std::pair<std::string_view, Value>>;

/******************************************************************************/

template <class T>
struct FromRef<T *> {
    std::optional<T *> operator()(Ref const &v, Scope &s) const {
        std::optional<T *> out;
        if (!v || v.request<std::nullptr_t>(s)) {
            out.emplace(nullptr);
        } else {
            if constexpr(!std::is_function_v<T>) {
                if (auto p = v.request<T &>(s)) out.emplace(std::addressof(*p));
            }
        }
        return out;
    }
};

template <>
struct FromRef<void *> {
    std::optional<void *> operator()(Ref const &v, Scope &s) const {
        std::optional<void *> out;
        if (v.qualifier() != Const) out.emplace(v.address());
        return out;
    }
};

template <>
struct FromRef<void const *> {
    std::optional<void const *> operator()(Ref const &v, Scope &s) const {return v.address();}
};

template <>
struct FromRef<char const *> {
    std::optional<char const *> operator()(Ref const &v, Scope &s) const {
        std::optional<char const *> out;
        if (!v || v.request<std::nullptr_t>()) out.emplace(nullptr);
        else if (auto p = v.request<std::string_view>(s)) out.emplace(p->data());
        else if (auto p = v.request<char const &>(s)) out.emplace(std::addressof(*p));
        return out;
    }
};

/*
Default FromRef for integer type tries to go through double precision
long double is not expected to be a useful route (it's assumed there are not multiple floating types larger than Real)
*/
template <class T>
struct FromRef<T, std::enable_if_t<std::is_floating_point_v<T>>> {
    std::optional<T> operator()(Ref const &v, Scope &s) const {
        DUMP("convert to floating");
        if (!std::is_same_v<Real, T>) if (auto p = v.request<Real>()) return static_cast<T>(*p);
        return s.error("not convertible to floating point", fetch<T>());
    }
};


/*
Default FromRef for integer type tries to go through Integer
*/
template <class T>
struct FromRef<T, std::enable_if_t<std::is_integral_v<T>>> {
    std::optional<T> operator()(Ref const &v, Scope &s) const {
        DUMP("trying convert to arithmetic", v.name(), fetch<T>());
        if (!std::is_same_v<Integer, T>) if (auto p = v.request<Integer>()) return static_cast<T>(*p);
        DUMP("failed to convert to arithmetic", v.name(), fetch<T>());
        return s.error("not convertible to integer", fetch<T>());
    }
};

/*
Default FromRef for enum permits conversion from integer types
*/
template <class T>
struct FromRef<T, std::enable_if_t<std::is_enum_v<T>>> {
    std::optional<T> operator()(Ref const &v, Scope &s) const {
        DUMP("trying convert to enum", v.name(), fetch<T>());
        if (auto p = v.request<std::underlying_type_t<T>>()) return static_cast<T>(*p);
        return s.error("not convertible to enum", fetch<T>());
    }
};

/*
Default FromRef for string tries to convert from std::string_view and std::string
*/
template <class T, class Traits, class Alloc>
struct FromRef<std::basic_string<T, Traits, Alloc>> {
    std::optional<std::basic_string<T, Traits, Alloc>> operator()(Ref const &v, Scope &s) const {
        DUMP("trying to convert to string");
        if (auto p = v.request<std::basic_string_view<T, Traits>>())
            return std::basic_string<T, Traits, Alloc>(std::move(*p));
        if (!std::is_same_v<std::basic_string<T, Traits, Alloc>, std::basic_string<T, Traits>>)
            if (auto p = v.request<std::basic_string<T, Traits>>())
                return std::move(*p);
        return s.error("not convertible to string", fetch<T>());
    }
};

template <class T, class Traits>
struct FromRef<std::basic_string_view<T, Traits>> {
    std::optional<std::basic_string_view<T, Traits>> operator()(Ref const &v, Scope &s) const {
        return s.error("not convertible to string view", fetch<T>());
    }
};

/******************************************************************************/

template <class V>
struct VectorFromRef {
    using T = std::decay_t<typename V::value_type>;

    template <class P>
    static std::optional<V> get(P &pack, Scope &s) {
        V out;
        out.reserve(pack.size());
        s.indices.emplace_back(0);
        for (auto &x : pack) {
            if (auto p = std::move(x).request(s, Type<T>()))
                out.emplace_back(std::move(*p));
            else return s.error();
            ++s.indices.back();
        }
        s.indices.pop_back();
        return out;
    }

    std::optional<V> operator()(Ref const &v, Scope &s) const {
        // if (auto p = v.request<Vector<T>>()) return get(*p, s);
        if constexpr (!std::is_same_v<V, Sequence> && !std::is_same_v<T, Ref>)
            if (auto p = v.request<Sequence>()) return get(*p, s);
        return s.error("expected sequence", fetch<V>());
    }
};

template <class T, class A>
struct FromRef<std::vector<T, A>> : VectorFromRef<std::vector<T, A>> {};

/******************************************************************************/

template <class V>
struct CompiledSequenceFromRef {
    using Array = std::array<Value, std::tuple_size_v<V>>;

    template <class ...Ts>
    static void combine(std::optional<V> &out, Ts &&...ts) {
        DUMP("trying CompiledSequenceFromRef combine", bool(ts)...);
        if ((bool(ts) && ...)) out = V{*static_cast<Ts &&>(ts)...};
    }

    template <class T, std::size_t ...Is>
    static void request_each(std::optional<V> &out, T &&t, Scope &s, std::index_sequence<Is...>) {
        DUMP("trying CompiledSequenceFromRef request");
        combine(out, std::move(t[Is]).request(s, Type<std::tuple_element_t<Is, V>>())...);
    }

    template <class T>
    static void request(std::optional<V> &out, T &&t, Scope &s) {
        DUMP("trying CompiledSequenceFromRef request");
        if (std::size(t) != std::tuple_size_v<V>) {
            s.error("wrong sequence length", fetch<V>(), std::tuple_size_v<V>, std::size(t));
        } else {
            s.indices.emplace_back(0);
            request_each(out, std::move(t), s, std::make_index_sequence<std::tuple_size_v<V>>());
            s.indices.pop_back();
        }
    }

    std::optional<V> operator()(Value r, Scope &s) const {
        std::optional<V> out;
        DUMP("trying CompiledSequenceFromRef", r.name());
        if constexpr(!std::is_same_v<V, Array>) {
            if (auto p = r.request<std::array<Value, std::tuple_size_v<V>>>()) {
                DUMP("trying array CompiledSequenceRequest2", r.name());
                request(out, std::move(*p), s);
            }
            return out;
        }
        if (auto p = r.request<Sequence>()) {
            DUMP("trying CompiledSequenceRequest2", r.name());
            request(out, std::move(*p), s);
        } else {
            DUMP("trying CompiledSequenceRequest3", r.name());
            s.error("expected sequence to make compiled sequence", fetch<V>());
        }
        return out;
    }
};

/// The default implementation is to accept convertible arguments or Value of the exact typeid match
template <class T>
struct FromRef<T, std::enable_if_t<std::tuple_size<T>::value >= 0>> : CompiledSequenceFromRef<T> {};

/******************************************************************************/

template <>
struct ToValue<char const *> {
    bool operator()(Output &v, char const *s) const {
        if (v.matches<std::string_view>())
            return v.set_if(s ? std::string_view(s) : std::string_view());

        if (v.matches<std::string>()) {
            if (s) return v.emplace_if<std::string>(s);
            return v.emplace_if<std::string>();
        }
        return false;
    }
};

template <class T>
struct ToValue<T, std::enable_if_t<(std::is_integral_v<T>)>> {
    bool operator()(Output &v, T t) const {
        DUMP("response from integer", fetch<T>(), v.name());
        if (v.matches<Integer>()) return v.emplace_if<Integer>(t);
        if (v.matches<Real>()) return v.emplace_if<Real>(t);
        DUMP("no response from integer");
        return false;
    }
};


/// Default ToValue for floating point allows conversion to Real or Integer
template <class T>
struct ToValue<T, std::enable_if_t<(std::is_floating_point_v<T>)>> {
    bool operator()(Output &v, T t) const {
        if (v.matches<Real>()) return v.emplace_if<Real>(t);
        if (v.matches<Integer>()) return v.emplace_if<Integer>(t);
        return false;
    }
};

/// Default ToValue for enum permits conversion to integer types
template <class T>
struct ToValue<T, std::enable_if_t<(std::is_enum_v<T>)>> {
    bool operator()(Output &v, T t) const {
        if (v.matches<std::underlying_type_t<T>>())
            return v.emplace_if<std::underlying_type_t<T>>(t);
        if (v.matches<Integer>())
            return v.emplace_if<Integer>(t);
        return false;
    }
};

/******************************************************************************/

/// ToValue for CompileSequence concept -- a sequence of compile time length
template <class T>
struct CompiledSequenceToValue {
    using Array = std::array<Value, std::tuple_size_v<T>>;

    template <std::size_t ...Is>
    static Sequence sequence(T const &t, std::index_sequence<Is...>) {
        Sequence o;
        o.reserve(sizeof...(Is));
        (o.emplace_back(std::get<Is>(t)), ...);
        return o;
    }

    template <std::size_t ...Is>
    static Sequence sequence(T &&t, std::index_sequence<Is...>) {
        Sequence o;
        o.reserve(sizeof...(Is));
        (o.emplace_back(std::get<Is>(std::move(t))), ...);
        return o;
    }

    template <std::size_t ...Is>
    static Array array(T const &t, std::index_sequence<Is...>) {return {std::get<Is>(t)...};}

    template <std::size_t ...Is>
    static Array array(T &&t, std::index_sequence<Is...>) {return {std::get<Is>(std::move(t))...};}

    bool operator()(Output &v, T const &t) const {
        auto idx = std::make_index_sequence<std::tuple_size_v<T>>();
        if (v.matches<Sequence>()) return v.set_if(sequence(t, idx));
        if (v.matches<Array>()) return v.set_if(array(t, idx));
        return false;
    }

    bool operator()(Output &v, T &&t) const {
        auto idx = std::make_index_sequence<std::tuple_size_v<T>>();
        if (v.matches<Sequence>()) return v.set_if(sequence(std::move(t), idx));
        if (v.matches<Array>()) return v.set_if(array(std::move(t), idx));
        return false;
    }
};

template <class T>
struct ToValue<T, std::enable_if_t<std::tuple_size<T>::value >= 0>> : CompiledSequenceToValue<T> {};

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
bool range_to_value(Output &v, Iter1 b, Iter2 e) {
    if (v.matches<Sequence>()) {
        Sequence s;
        s.reserve(std::distance(b, e));
        for (; b != e; ++b) {
            if constexpr(std::is_same_v<T, Value>) s.emplace_back(*b);
            else if constexpr(!std::is_same_v<T, Ref>) s.emplace_back(Type<T>(), *b);
        }
        return v.set_if(std::move(s));
    }
    if (v.matches<Vector<T>>()) {
        return v.emplace_if<Vector<T>>(b, e);
    }
    return false;
}

template <class T>
struct VectorToValue {
    using E = std::decay_t<typename T::value_type>;

    bool operator()(Output &v, T const &t) const {
        if (range_to_value<E>(v, std::begin(t), std::end(t))) {
            return true;
        }
        // if constexpr(HasData<T const &>::value) {
        //     if (v.matches<ArrayView>()) return v.emplace_if<ArrayView>(std::data(t), std::size(t));
        // }
        return false;
    }

    bool operator()(Output &v, T &t) const {
        if (range_to_value<E>(v, std::cbegin(t), std::cend(t))) return true;
        // if constexpr(HasData<T &>::value)
        //     if (v.matches<ArrayView>()) return v.emplace_if<ArrayView>(std::data(t), std::size(t));
        return false;
    }

    bool operator()(Output &v, T &&t) const {
        return range_to_value<E>(v, std::make_move_iterator(std::begin(t)), std::make_move_iterator(std::end(t)));
    }
};

template <class T, class A>
struct ToValue<std::vector<T, A>, std::enable_if_t<!std::is_same_v<T, Value>>> : VectorToValue<std::vector<T, A>> {};


}
