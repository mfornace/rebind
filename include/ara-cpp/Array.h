#pragma once
#include <ara/Core.h>
#include <vector>
#include <string>

namespace ara {

/******************************************************************************/

// template <class B, class E>
// bool array_major(B begin, E end) {
//     for (std::ptrdiff_t expected = 1; begin != end; ++begin) {
//         if (begin->first < 2) continue;
//         if (begin->second != expected) return false;
//         expected = begin->first * begin->second;
//     }
//     return true;
// }

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

    static void load_span(std::optional<V> &o, Span& span) {
        if (span.rank() != 1) return;
        o.emplace();
        o->reserve(span.size());

        if (auto data = span.target<T>()) {
            std::copy(data, data + span.size(), std::back_inserter(*o));
        } else {
            bool ok = span.map([&](Ref& r) {
                if (auto p = r.get<T>()) return o->emplace_back(std::move(*p)), true;
                else return false;
            });
            if (!ok) o.reset();
        }
    }

    static void load_view(std::optional<V> &o, View& view) {
        o.emplace();
        o->reserve(view.size());

        for (auto &ref : view) {
            if (auto p = ref.get<T>()) o->emplace_back(std::move(*p));
            else {o.reset(); return;}
        }
    }

    static std::optional<V> load(Ref& v) {
        std::optional<V> out;
        if (auto p = v.get<Span>()) load_span(out, *p);
        else if (auto p = v.get<Array>()) load_span(out, p->span());
        else if (auto p = v.get<View>()) load_view(out, *p);
        return out;
    }
};

/******************************************************************************/

template <class T, class A>
struct Impl<std::vector<T, A>> : Default<std::vector<T, A>>, DumpVector<std::vector<T, A>>, LoadVector<std::vector<T, A>> {};

/******************************************************************************/

}