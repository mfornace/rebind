#pragma once
#include "Array.h"
#include <tuple>
#include <utility>
#include <array>
#include <optional>
#include <variant>
#include <map>

namespace ara {

/******************************************************************************/

template <class T>
struct Dumpable<std::optional<T>> {
    bool operator()(Target &a, std::optional<T> &t) const {
        return t ? Dumpable<T>()(a, *t) : false;
    }
    bool operator()(Target &a, std::optional<T> &&t) const {
        return t ? Dumpable<T>()(a, std::move(*t)) : false;
    }
    bool operator()(Target &a, std::optional<T> const &t) const {
        return t ? Dumpable<T>()(a, *t) : false;
    }
};

template <class T>
struct Loadable<std::optional<T>> {
    std::optional<std::optional<T>> operator()(Ref &r) const {
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
struct Dumpable<std::shared_ptr<T>> {
    bool operator()(Target &a, std::shared_ptr<T> const &p) const {
        // DUMP("shared_ptr", t, Index::of<T>(), bool(p), Q);
        // return p && get_response<Q>(out, std::move(t), *p);
        return false;
    }
};

template <class T>
struct Loadable<std::shared_ptr<T>> {
    std::optional<std::shared_ptr<T>> operator()(Ref &r) const {
        std::optional<std::shared_ptr<T>> out;
        if (!r || r.load<std::nullptr_t>()) out.emplace();
        else if (auto p = r.load<std::remove_cv_t<T>>())
            out.emplace(std::make_shared<T>(std::move(*p)));
        return out;
    }
};

/******************************************************************************/

template <class ...Ts>
struct Dumpable<std::variant<Ts...>> {
    bool operator()(Target &a, std::variant<Ts...> const &) const {
        return false;
    }
        // return std::visit([&](auto &&x) {return get_response<Q>(out, t, static_cast<decltype(x) &&>(x));}, static_cast<V &&>(v));
};

template <class ...Ts>
struct Loadable<std::variant<Ts...>> {
    template <class T>
    static bool put(std::optional<std::variant<Ts...>> &out, Ref &r) {
        if (auto p = r.load<T>()) return out.emplace(std::move(*p)), true;
        return false;
    }

    std::optional<std::variant<Ts...>> operator()(Ref &r) const {
        std::optional<std::variant<Ts...>> out;
        (void) (put<Ts>(out, r) || ...);
        return out;
    }
};

/******************************************************************************/

template <class V>
struct DumpMap {
    using T = std::pair<typename V::key_type, typename V::mapped_type>;

    bool operator()(Ref &o, Index t, V &&v) const {
        return false;
        // return range_response<T>(o, t, std::make_move_iterator(std::begin(v)), std::make_move_iterator(std::end(v)));
    }

    bool operator()(Ref &o, Index t, V const &v) const {
        return false;
        // return range_response<T>(o, t, std::begin(v), std::end(v));
    }
};

template <class V>
struct LoadMap {
    using T = std::pair<typename V::key_type, typename V::mapped_type>;

    std::optional<V> operator()(Ref &v) const {
        std::optional<V> out;
        // if (auto p = v.load<Vector<T>>())
        //     out.emplace(std::make_move_iterator(std::begin(*p)), std::make_move_iterator(std::end(*p)));
        return out;
    }
};

template <class K, class V, class C, class A>
struct Loadable<std::map<K, V, C, A>> : LoadMap<std::map<K, V, C, A>> {};

template <class K, class V, class C, class A>
struct Dumpable<std::map<K, V, C, A>> : DumpMap<std::map<K, V, C, A>> {};

/******************************************************************************/

template <class F>
struct LoadFunction {
    std::optional<F> operator()(Ref &v) const {
        std::optional<F> out;
        // if (auto p = v.load<Callback<typename F::result_type>>()) out.emplace(std::move(*p));
        return out;
    }
};

template <class R, class ...Ts>
struct Loadable<std::function<R(Ts...)>> : LoadFunction<std::function<R(Ts...)>> {};

/******************************************************************************/

}
