#pragma once
#include "Array.h"
#include <tuple>
#include <utility>
#include <array>
#include <optional>
#include <variant>
#include <map>

namespace sfb {

template <>
struct Impl<std::string_view> : Default<std::string_view> {
    static bool dump(Target &a, std::string_view t) {
        if (a.accepts<Str>()) return a.emplace<Str>(t);
        if (a.accepts<String>()) return a.emplace<String>(t);
        return false;
    }

    static auto load(Ref &r) {
        std::optional<std::string_view> out;
        if (auto p = r.get<Str>()) out.emplace(*p);
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
        if (auto p = r.get<Str>()) out.emplace(*p);
        if (auto p = r.get<String>()) out.emplace(std::move(*p));
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
        if (!r || r.get<std::nullptr_t>()) {
            out.emplace();
        } else if (auto p = r.get<std::remove_cv_t<T>>()) {
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
        if (!r || r.get<std::nullptr_t>()) out.emplace();
        else if (auto p = r.get<std::remove_cv_t<T>>())
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
        if (auto p = r.get<T>()) return out.emplace(std::move(*p)), true;
        return false;
    }

    static auto load(Ref &r) {
        std::optional<std::variant<Ts...>> out;
        (void) (put<Ts>(out, r) || ...);
        return out;
    }
};

/******************************************************************************/

template <class T, class B, class E>
bool dump_array(Target& a, std::size_t size, B&& begin, E&& end) {
    auto raw = std::make_unique<storage_like<T>[]>(size);
    auto const address = reinterpret_cast<T*>(raw.get());
    std::uninitialized_copy(std::forward<B>(begin), std::forward<E>(end), address);
    std::unique_ptr<T[]> ptr(address);
    raw.release();
    DUMP("made array", size);
    return a.emplace<Array>(Span(address, size), std::move(ptr));
}

template <class B, class E>
bool dump_view(Target& a, std::size_t size, B begin, E const& end) {
    return a.emplace<View>(size, [&](auto &p, Ignore) {
        for (; begin != end; ++begin) {
            new(p) Ref(*begin);
            ++p;
        }
    });
}

template <class M>
struct DumpMap {
    using T = std::pair<typename M::key_type, typename M::mapped_type>;

    static bool dump(Target& a, M&& v) {
        using std::begin; using std::end; using std::size;
        if (a.accepts<View>()) return dump_view(a, size(v), begin(v), end(v));
        if (a.accepts<Array>()) return dump_array<T>(a, size(v),
            std::make_move_iterator(begin(v)), std::make_move_iterator(end(v)));
        // Span impossible (not contiguous).
        // Tuple? Probably not useful.
        return false;
    }

    static bool dump(Target& a, M const& v) {
        using std::begin; using std::end; using std::size;
        if (a.accepts<View>()) return dump_view(a, size(v), begin(v), end(v));
        if (a.accepts<Array>()) return dump_array<T>(a, size(v), begin(v), end(v));
        return false;
        // return range_response<T>(o, t, std::begin(v), std::end(v));
    }

    static bool dump(Target& a, M& v) {
        using std::begin; using std::end; using std::size;
        if (a.accepts<View>()) return dump_view(a, size(v), begin(v), end(v));
        if (a.accepts<Array>()) return dump_array<T>(a, size(v), begin(v), end(v));
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
        DUMP(span.rank(), span.length(0), span.length(1));
        o.emplace();
        std::optional<K> key;
        if (!span.map([&](Ref& ref) {
            if (key) {
                DUMP("loading value");
                if (auto v = ref.get<V>()) {
                    DUMP("emplace key value pair");
                    o->emplace(std::move(*key), std::move(*v));
                    key.reset();
                    return true;
                } else return false;
            } else {
                key = ref.get<K>();
                DUMP("loaded key", bool(key));
                return bool(key);
            }
        })) o.reset();
    }

    static std::optional<M> load(Ref &r) {
        DUMP("try to load map", type_name<M>());
        std::optional<M> o;
        if (auto p = r.get<Span>()) load_span(o, *p);
        else if (auto p = r.get<Array>()) load_span(o, p->span());
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
        // if (auto p = v.get<Callback<typename F::result_type>>()) out.emplace(std::move(*p));
        return out;
    }
};

template <class R, class ...Ts>
struct Impl<std::function<R(Ts...)>> : Default<std::function<R(Ts...)>>, LoadFunction<std::function<R(Ts...)>> {};

/******************************************************************************/

}
