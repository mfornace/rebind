#pragma once
#include <ara/Core.h>
#include <vector>
#include <string>

namespace ara {

/******************************************************************************/

/*
Default Loadable for string tries to convert from std::string_view and std::string
*/
template <class T, class Traits, class Alloc>
struct Loadable<std::basic_string<T, Traits, Alloc>> {
    std::optional<std::basic_string<T, Traits, Alloc>> operator()(Ref &v) const {
        DUMP("trying to convert to string");
        if (auto p = v.load<std::basic_string_view<T, Traits>>())
            return std::basic_string<T, Traits, Alloc>(std::move(*p));
        if (!std::is_same_v<std::basic_string<T, Traits, Alloc>, std::basic_string<T, Traits>>)
            if (auto p = v.load<std::basic_string<T, Traits>>())
                return std::move(*p);
        return {}; //s.error("not convertible to string", Index::of<T>());
    }
};

template <class T, class Traits>
struct Loadable<std::basic_string_view<T, Traits>> {
    std::optional<std::basic_string_view<T, Traits>> operator()(Ref &v) const {
        return {}; //s.error("not convertible to string view", Index::of<T>());
    }
};

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
    //     return v.set_if(std::move());
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

template <class V>
struct LoadVector {
    using T = std::decay_t<typename V::value_type>;

    template <class P>
    static std::optional<V> get(P &pack) {
        V out;
        out.reserve(pack.size());
        // s.indices.emplace_back(0);
        for (auto &x : pack) {
            if (auto p = std::move(x).load(Type<T>()))
                out.emplace_back(std::move(*p));
            else return {}; //s.error();
            // ++s.indices.back();
        }
        // s.indices.pop_back();
        return out;
    }

    std::optional<V> operator()(Ref &v) const {
        // if (auto p = v.load<Vector<T>>()) return get(*p, s);
        // if constexpr (!std::is_same_v<V, Sequence> && !std::is_same_v<T, Ref>)
        //     if (auto p = v.load<Sequence>()) return get(*p);
        return {}; //s.error("expected sequence", Index::of<V>());
    }
};

/******************************************************************************/

template <class T, class A>
struct Dumpable<std::vector<T, A>, std::enable_if_t<!std::is_same_v<T, Value>>> : DumpVector<std::vector<T, A>> {};

template <class T, class A>
struct Loadable<std::vector<T, A>> : LoadVector<std::vector<T, A>> {};

/******************************************************************************/

}