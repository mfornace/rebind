#pragma once
#include "Array.h"
#include <tuple>
#include <utility>
#include <array>
#include <optional>
#include <variant>
#include <map>

namespace ara {

template <>
struct Impl<std::string_view> : Default<std::string_view> {
    static bool dump(Target &a, std::string_view t) {
        if (a.accepts<Str>()) return a.emplace<Str>(t);
        if (a.accepts<String>()) return a.emplace<String>(t);
        return false;
    }

    static auto load(Ref &r) {
        std::optional<std::string_view> out;
        if (auto p = r.load<Str>()) out.emplace(*p);
        return out;
    }
};

/******************************************************************************/

template <class Alloc>
struct Impl<std::basic_string<char, std::char_traits<char>, Alloc>> : Default<std::basic_string<char, std::char_traits<char>, Alloc>> {
    using S = std::basic_string<char, std::char_traits<char>, Alloc>;

    static bool dump(Target &a, S &&t) {
        DUMP("dumping std::string");
        if (a.accepts<String>()) return a.emplace<String>(std::move(t));
        return false;
    }
    static bool dump(Target &a, S const &t) {
        DUMP("dumping std::string");
        if (a.accepts<Str>()) return a.emplace<Str>(std::string_view(t));
        if (a.accepts<String>()) return a.emplace<String>(std::string_view(t));
        return false;
    }

    static auto load(Ref &r) {
        DUMP("loading std::string");
        std::optional<S> out;
        if (auto p = r.load<Str>()) out.emplace(*p);
        if (auto p = r.load<String>()) out.emplace(std::move(*p));
        return out;
    }
};

/******************************************************************************/

template <class T>
struct Impl<std::optional<T>> : Default<std::optional<T>> {
    static bool dump(Target &a, std::optional<T> &t) {
        return t ? Impl<T>::dump(a, *t) : false;
    }
    static bool dump(Target &a, std::optional<T> &&t) {
        return t ? Impl<T>::dump(a, std::move(*t)) : false;
    }
    static bool dump(Target &a, std::optional<T> const &t) {
        return t ? Impl<T>::dump(a, *t) : false;
    }

    static auto load(Ref &r) {
        std::optional<std::optional<T>> out;
        if (!r || r.load<std::nullptr_t>()) {
            out.emplace();
        } else if (auto p = r.load<std::remove_cv_t<T>>()) {
            out.emplace(std::move(*p));
        }
        return out;
    }
};

/******************************************************************************/

template <class T>
struct Impl<std::shared_ptr<T>> : Default<std::shared_ptr<T>> {
    static bool dump(Target &a, std::shared_ptr<T> const &p) {
        // DUMP("shared_ptr", t, Index::of<T>(), bool(p), Q);
        // return p && get_response<Q>(out, std::move(t), *p);
        return false;
    }

    static auto load(Ref &r) {
        std::optional<std::shared_ptr<T>> out;
        if (!r || r.load<std::nullptr_t>()) out.emplace();
        else if (auto p = r.load<std::remove_cv_t<T>>())
            out.emplace(std::make_shared<T>(std::move(*p)));
        return out;
    }
};

/******************************************************************************/

template <class ...Ts>
struct Impl<std::variant<Ts...>> : Default<std::variant<Ts...>> {
    static bool dump(Target &a, std::variant<Ts...> const &) {
        return false;
        // return std::visit([&](auto &&x) {return get_response<Q>(out, t, static_cast<decltype(x) &&>(x));}, static_cast<V &&>(v));
    }

    template <class T>
    static bool put(std::optional<std::variant<Ts...>> &out, Ref &r) {
        if (auto p = r.load<T>()) return out.emplace(std::move(*p)), true;
        return false;
    }

    static auto load(Ref &r) {
        std::optional<std::variant<Ts...>> out;
        (void) (put<Ts>(out, r) || ...);
        return out;
    }
};

/******************************************************************************/

template <class V>
struct DumpMap {
    using T = std::pair<typename V::key_type, typename V::mapped_type>;

    bool operator()(Target &a, V &&v) const {
        return false;
        // return range_response<T>(o, t, std::make_move_iterator(std::begin(v)), std::make_move_iterator(std::end(v)));
    }

    bool operator()(Target &o, V const &v) const {
        return false;
        // return range_response<T>(o, t, std::begin(v), std::end(v));
    }
};

/******************************************************************************/

template <class M>
struct LoadMap {
    using K = typename M::key_type;
    using V = typename M::mapped_type;

    static void load_span(std::optional<M>& o, Span& span) {
        DUMP("load span into map", span.rank());
        if (span.rank() != 2 || span.length(1) != 2) return;
        o.emplace();
        std::optional<K> key;
        span.map([&](Ref &ref) {
            if (key) {
                if (auto v = ref.load<V>()) {
                    o->emplace(std::move(*key), std::move(*v));
                    key.reset();
                    return true;
                } else return false;
            } else {
                key = ref.load<K>();
                return bool(key);
            }
        });
    }

    std::optional<M> operator()(Ref &r) const {
        std::optional<M> o;
        if (auto p = r.load<Span>()) load_span(o, *p);
        else if (auto p = r.load<Array>()) load_span(o, *p);
        return o;
    }
};

template <class K, class V, class C, class A>
struct Impl<std::map<K, V, C, A>> : Default<std::map<K, V, C, A>>, LoadMap<std::map<K, V, C, A>>, DumpMap<std::map<K, V, C, A>> {};

/******************************************************************************/

template <class F>
struct LoadFunction {
    static auto load(Ref &v) {
        std::optional<F> out;
        // if (auto p = v.load<Callback<typename F::result_type>>()) out.emplace(std::move(*p));
        return out;
    }
};

template <class R, class ...Ts>
struct Impl<std::function<R(Ts...)>> : Default<std::function<R(Ts...)>>, LoadFunction<std::function<R(Ts...)>> {};

/******************************************************************************/

}
