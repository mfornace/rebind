#pragma once
#include <ara/Core.h>
#include <tuple>

namespace ara {

/******************************************************************************/

/// Dumpable for CompileSequence concept -- a sequence of compile time length
// Does not currently handle C arrays.
template <class T>
struct DumpTuple {
    // using Array = std::array<Value, std::tuple_size_v<T>>;

    // template <std::size_t ...Is>
    // static Sequence sequence(T const &t, std::index_sequence<Is...>) {
    //     Sequence o;
    //     o.reserve(sizeof...(Is));
    //     (o.emplace_back(std::get<Is>(t)), ...);
    //     return o;
    // }

    // template <std::size_t ...Is>
    // static Sequence sequence(T &&t, std::index_sequence<Is...>) {
    //     Sequence o;
    //     o.reserve(sizeof...(Is));
    //     (o.emplace_back(std::get<Is>(std::move(t))), ...);
    //     return o;
    // }

    // template <std::size_t ...Is>
    // static Array array(T const &t, std::index_sequence<Is...>) {return {std::get<Is>(t)...};}

    // template <std::size_t ...Is>
    // static Array array(T &&t, std::index_sequence<Is...>) {return {std::get<Is>(std::move(t))...};}

    template <std::size_t ...Is>
    static bool view(Target &v, T const &t, std::index_sequence<Is...>) {
        if (auto o = v.emplace_if<View>()) {
            // o->size = sizeof...(Is);
            // o->shape = nullptr;
            // o->data = reinterpret_cast<ara_ref *>(new Ref[sizeof...(Is)]{Ref::from_existing(std::get<Is>(t))...});
            return true;
        } else return false;
    }

    template <std::size_t ...Is>
    static bool tuple(std::unique_ptr<T> &&p, std::index_sequence<Is...>) {
        return false;
    }

    static bool dump(Target &v, T const &t) {
        auto idx = std::make_index_sequence<std::tuple_size_v<T>>();
        if (v.accepts<View>()) return view(v, t, idx);
        if constexpr(std::is_copy_constructible_v<T>) {
            if (v.accepts<Tuple>()) return tuple(std::make_unique<T>(t), idx);
        }
        return false;
    }

    static bool dump(Target &v, T &&t) {
        auto idx = std::make_index_sequence<std::tuple_size_v<T>>();
        // Tuple, if copy constructible
        if constexpr(std::is_move_constructible_v<T>) {
            if (v.accepts<Tuple>()) return tuple(std::make_unique<T>(std::move(t)), idx);
        }
        return false;
    }
};


template <class V>
struct LoadTuple {
    // using Array = std::array<Value, std::tuple_size_v<V>>;

    // template <class ...Ts>
    // static void combine(std::optional<V> &out, Ts &&...ts) {
    //     DUMP("trying LoadTuple combine", bool(ts)...);
    //     if ((bool(ts) && ...)) out = V{*static_cast<Ts &&>(ts)...};
    // }

    // template <class T, std::size_t ...Is>
    // static void request_each(std::optional<V> &out, T &&t, std::index_sequence<Is...>) {
    //     DUMP("trying LoadTuple load");
    //     combine(out, std::move(t[Is]).load(Type<std::tuple_element_t<Is, V>>())...);
    // }

    // template <class T>
    // static void load(std::optional<V> &out, T &&t) {
    //     DUMP("trying LoadTuple load");
    //     if (std::size(t) != std::tuple_size_v<V>) {
    //         // s.error("wrong sequence length", Index::of<V>(), std::tuple_size_v<V>, std::size(t));
    //     } else {
    //         // s.indices.emplace_back(0);
    //         request_each(out, std::move(t), std::make_index_sequence<std::tuple_size_v<V>>());
    //         // s.indices.pop_back();
    //     }
    // }

    static std::optional<V> load(Ref &r) {
        std::optional<V> out;
        // Span
        // View
        // Array
        // Tuple
        // DUMP("trying LoadTuple", r.name());
        // if constexpr(!std::is_same_v<V, Array>) {
        //     if (auto p = r.load<std::array<Value, std::tuple_size_v<V>>>()) {
        //         DUMP("trying array CompiledSequenceRequest2", r.name());
        //         load(out, std::move(*p));
        //     }
        //     return out;
        // }
        // if (auto p = r.load<Sequence>()) {
        //     DUMP("trying CompiledSequenceRequest2", r.name());
        //     load(out, std::move(*p));
        // } else {
        //     DUMP("trying CompiledSequenceRequest3", r.name());
        //     // s.error("expected sequence to make compiled sequence", Index::of<V>());
        // }
        return out;
    }
};

/******************************************************************************/

// Coverage of std::pair, std::array, and std::tuple. *Not* C arrays.
template <class T>
struct Impl<T, std::enable_if_t<std::tuple_size<T>::value >= 0>> : LoadTuple<T>, DumpTuple<T> {};

/******************************************************************************/

}