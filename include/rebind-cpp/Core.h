#pragma once
#include "Arrays.h"
#include "Value.h"
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

using Sequence = Vector<Value>;

using Dictionary = Vector<std::pair<std::string_view, Value>>;

/******************************************************************************/

template <class T>
struct Loadable<T *> {
    std::optional<T *> operator()(Ref &v, Scope &s) const {
        std::optional<T *> out;
        if (!v || v.load<std::nullptr_t>(s)) {
            out.emplace(nullptr);
        } else {
            if constexpr(!std::is_function_v<T>) {
                if (auto p = v.load<T &>(s)) out.emplace(std::addressof(*p));
            }
        }
        return out;
    }
};

template <>
struct Loadable<void *> {
    std::optional<void *> operator()(Ref &v, Scope &s) const {
        std::optional<void *> out;
        // if (v.qualifier() != Const) out.emplace(v.address());
        return out;
    }
};

template <>
struct Loadable<void const *> {
    std::optional<void const *> operator()(Ref &v, Scope &s) const {
        // return v.address();
        return {};
    }
};

template <>
struct Loadable<char const *> {
    std::optional<char const *> operator()(Ref &v, Scope &s) const {
        std::optional<char const *> out;
        if (!v || v.load<std::nullptr_t>(s)) out.emplace(nullptr);
        else if (auto p = v.load<std::string_view>(s)) out.emplace(p->data());
        else if (auto p = v.load<char const &>(s)) out.emplace(std::addressof(*p));
        return out;
    }
};

/*
Default Loadable for integer type tries to go through double precision
long double is not expected to be a useful route (it's assumed there are not multiple floating types larger than Real)
*/
template <class T>
struct Loadable<T, std::enable_if_t<std::is_floating_point_v<T>>> {
    std::optional<T> operator()(Ref &v, Scope &s) const {
        DUMP("convert to floating");
        if (!std::is_same_v<Real, T>) if (auto p = v.load<Real>(s)) return static_cast<T>(*p);
        return s.error("not convertible to floating point", Index::of<T>());
    }
};


/*
Default Loadable for integer type tries to go through Integer
*/
template <class T>
struct Loadable<T, std::enable_if_t<std::is_integral_v<T>>> {
    std::optional<T> operator()(Ref &v, Scope &s) const {
        DUMP("trying convert to integer ", v.name(), " ", Index::of<T>());
        if (!std::is_same_v<Integer, T>) if (auto p = v.load<Integer>(s)) return static_cast<T>(*p);
        DUMP("failed to convert to integer", v.name(), Index::of<T>());
        return s.error("not convertible to integer", Index::of<T>());
    }
};

/*
Default Loadable for enum permits conversion from integer types
*/
template <class T>
struct Loadable<T, std::enable_if_t<std::is_enum_v<T>>> {
    std::optional<T> operator()(Ref &v, Scope &s) const {
        DUMP("trying convert to enum", v.name(), Index::of<T>());
        if (auto p = v.load<std::underlying_type_t<T>>(s)) return static_cast<T>(*p);
        return s.error("not convertible to enum", Index::of<T>());
    }
};

/*
Default Loadable for string tries to convert from std::string_view and std::string
*/
template <class T, class Traits, class Alloc>
struct Loadable<std::basic_string<T, Traits, Alloc>> {
    std::optional<std::basic_string<T, Traits, Alloc>> operator()(Ref &v, Scope &s) const {
        DUMP("trying to convert to string");
        if (auto p = v.load<std::basic_string_view<T, Traits>>(s))
            return std::basic_string<T, Traits, Alloc>(std::move(*p));
        if (!std::is_same_v<std::basic_string<T, Traits, Alloc>, std::basic_string<T, Traits>>)
            if (auto p = v.load<std::basic_string<T, Traits>>(s))
                return std::move(*p);
        return s.error("not convertible to string", Index::of<T>());
    }
};

template <class T, class Traits>
struct Loadable<std::basic_string_view<T, Traits>> {
    std::optional<std::basic_string_view<T, Traits>> operator()(Ref &v, Scope &s) const {
        return s.error("not convertible to string view", Index::of<T>());
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
            if (auto p = std::move(x).load(s, Type<T>()))
                out.emplace_back(std::move(*p));
            else return s.error();
            ++s.indices.back();
        }
        s.indices.pop_back();
        return out;
    }

    std::optional<V> operator()(Ref &v, Scope &s) const {
        // if (auto p = v.load<Vector<T>>()) return get(*p, s);
        if constexpr (!std::is_same_v<V, Sequence> && !std::is_same_v<T, Ref>)
            if (auto p = v.load<Sequence>(s)) return get(*p, s);
        return s.error("expected sequence", Index::of<V>());
    }
};

template <class T, class A>
struct Loadable<std::vector<T, A>> : VectorFromRef<std::vector<T, A>> {};

/******************************************************************************/

template <class V>
struct LoadCompiledSequence {
    using Array = std::array<Value, std::tuple_size_v<V>>;

    template <class ...Ts>
    static void combine(std::optional<V> &out, Ts &&...ts) {
        DUMP("trying LoadCompiledSequence combine", bool(ts)...);
        if ((bool(ts) && ...)) out = V{*static_cast<Ts &&>(ts)...};
    }

    template <class T, std::size_t ...Is>
    static void request_each(std::optional<V> &out, T &&t, Scope &s, std::index_sequence<Is...>) {
        DUMP("trying LoadCompiledSequence load");
        combine(out, std::move(t[Is]).load(s, Type<std::tuple_element_t<Is, V>>())...);
    }

    template <class T>
    static void load(std::optional<V> &out, T &&t, Scope &s) {
        DUMP("trying LoadCompiledSequence load");
        if (std::size(t) != std::tuple_size_v<V>) {
            s.error("wrong sequence length", Index::of<V>(), std::tuple_size_v<V>, std::size(t));
        } else {
            s.indices.emplace_back(0);
            request_each(out, std::move(t), s, std::make_index_sequence<std::tuple_size_v<V>>());
            s.indices.pop_back();
        }
    }

    std::optional<V> operator()(Ref &r, Scope &s) const {
        std::optional<V> out;
        DUMP("trying LoadCompiledSequence", r.name());
        if constexpr(!std::is_same_v<V, Array>) {
            if (auto p = r.load<std::array<Value, std::tuple_size_v<V>>>(s)) {
                DUMP("trying array CompiledSequenceRequest2", r.name());
                load(out, std::move(*p), s);
            }
            return out;
        }
        if (auto p = r.load<Sequence>(s)) {
            DUMP("trying CompiledSequenceRequest2", r.name());
            load(out, std::move(*p), s);
        } else {
            DUMP("trying CompiledSequenceRequest3", r.name());
            s.error("expected sequence to make compiled sequence", Index::of<V>());
        }
        return out;
    }
};

/// The default implementation is to accept convertible arguments or Value of the exact typeid match
template <class T>
struct Loadable<T, std::enable_if_t<std::tuple_size<T>::value >= 0>> : LoadCompiledSequence<T> {};

/******************************************************************************/

template <>
struct Dumpable<char const *> {
    bool operator()(Target &v, char const *s) const {
        if (v.accepts<std::string_view>())
            return v.set_if(s ? std::string_view(s) : std::string_view());

        if (v.accepts<std::string>()) {
            if (s) return v.emplace_if<std::string>(s);
            return v.emplace_if<std::string>();
        }
        return false;
    }
};

template <class T>
struct Dumpable<T, std::enable_if_t<(std::is_integral_v<T>)>> {
    bool operator()(Target &v, T t) const {
        DUMP("Dumpable ", type_name<T>(), " to ", v.name());
        if (v.accepts<Integer>()) return v.emplace_if<Integer>(t);
        if (v.accepts<Real>()) return v.emplace_if<Real>(t);
        DUMP("Dumpable ", type_name<T>(), " to ", v.name(), " failed");
        return false;
    }
};


/// Default Dumpable for floating point allows conversion to Real or Integer
template <class T>
struct Dumpable<T, std::enable_if_t<(std::is_floating_point_v<T>)>> {
    bool operator()(Target &v, T t) const {
        if (v.accepts<Real>()) return v.emplace_if<Real>(t);
        if (v.accepts<Integer>()) return v.emplace_if<Integer>(t);
        return false;
    }
};

/// Default Dumpable for enum permits conversion to integer types
template <class T>
struct Dumpable<T, std::enable_if_t<(std::is_enum_v<T>)>> {
    bool operator()(Target &v, T t) const {
        if (v.accepts<std::underlying_type_t<T>>())
            return v.emplace_if<std::underlying_type_t<T>>(t);
        if (v.accepts<Integer>())
            return v.emplace_if<Integer>(t);
        return false;
    }
};

/******************************************************************************/

/// Dumpable for CompileSequence concept -- a sequence of compile time length
template <class T>
struct DumpCompiledSequence {
    using Array = std::array<Value, std::tuple_size_v<T>>;

    // template <std::size_t ...Is>
    // static Sequence sequence(T const &t, std::index_sequence<Is...>) {
    //     Sequence o;
    //     o.reserve(sizeof...(Is));
    //     (o.emplace_back(std::get<Is>(t)), ...);
    //     return o;
    // }

    template <std::size_t ...Is>
    static Sequence sequence(T &&t, std::index_sequence<Is...>) {
        Sequence o;
        o.reserve(sizeof...(Is));
        (o.emplace_back(std::get<Is>(std::move(t))), ...);
        return o;
    }

    // template <std::size_t ...Is>
    // static Array array(T const &t, std::index_sequence<Is...>) {return {std::get<Is>(t)...};}

    template <std::size_t ...Is>
    static Array array(T &&t, std::index_sequence<Is...>) {return {std::get<Is>(std::move(t))...};}

    bool operator()(Target &v, T const &t) const {
        auto idx = std::make_index_sequence<std::tuple_size_v<T>>();
        // if (v.accepts<Sequence>()) return v.set_if(sequence(t, idx));
        // if (v.accepts<Array>()) return v.set_if(array(t, idx));
        return false;
    }

    bool operator()(Target &v, T &&t) const {
        auto idx = std::make_index_sequence<std::tuple_size_v<T>>();
        if (v.accepts<Sequence>()) return v.set_if(sequence(std::move(t), idx));
        if (v.accepts<Array>()) return v.set_if(array(std::move(t), idx));
        return false;
    }
};

template <class T>
struct Dumpable<T, std::enable_if_t<std::tuple_size<T>::value >= 0>> : DumpCompiledSequence<T> {};

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
bool dump_range(Target &v, Iter1 b, Iter2 e) {
    // if (v.accepts<Sequence>()) {
    //     Sequence s;
    //     s.reserve(std::distance(b, e));
    //     for (; b != e; ++b) {
    //         if constexpr(std::is_same_v<T, Value>) s.emplace_back(*b);
    //         else if constexpr(!std::is_same_v<T, Ref>) s.emplace_back(Type<T>(), *b);
    //     }
    //     return v.set_if(std::move(s));
    // }
    // if (v.accepts<Vector<T>>()) {
    //     return v.emplace_if<Vector<T>>(b, e);
    // }
    return false;
}

template <class T>
struct DumpVector {
    using E = std::decay_t<typename T::value_type>;

    bool operator()(Target &v, T const &t) const {
        if (dump_range<E>(v, std::begin(t), std::end(t))) {
            return true;
        }
        // if constexpr(HasData<T const &>::value) {
        //     if (v.accepts<ArrayView>()) return v.emplace_if<ArrayView>(std::data(t), std::size(t));
        // }
        return false;
    }

    bool operator()(Target &v, T &t) const {
        if (dump_range<E>(v, std::cbegin(t), std::cend(t))) return true;
        // if constexpr(HasData<T &>::value)
        //     if (v.accepts<ArrayView>()) return v.emplace_if<ArrayView>(std::data(t), std::size(t));
        return false;
    }

    bool operator()(Target &v, T &&t) const {
        return dump_range<E>(v, std::make_move_iterator(std::begin(t)), std::make_move_iterator(std::end(t)));
    }
};

template <class T, class A>
struct Dumpable<std::vector<T, A>, std::enable_if_t<!std::is_same_v<T, Value>>> : DumpVector<std::vector<T, A>> {};


}
