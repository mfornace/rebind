#pragma once
#include "Document.h"
#include <tuple>
#include <utility>
#include <array>
#include <optional>
#include <variant>

namespace boost {
    namespace container {
        template <class T, std::size_t N, class Alloc>
        class small_vector;
    }
}

namespace cpy {

template <class T, class U>
struct Renderer<std::pair<T, U>> : Renderer<Pack<no_qualifier<T>, no_qualifier<U>>> {};

template <class ...Ts>
struct Renderer<std::tuple<Ts...>> : Renderer<Pack<no_qualifier<Ts>...>> {};

template <class T, std::size_t N>
struct Renderer<std::array<T, N>> : Renderer<Pack<no_qualifier<T>>> {};

template <class T>
struct Renderer<std::optional<T>> : Renderer<Pack<no_qualifier<T>>> {};

template <class ...Ts>
struct Renderer<std::variant<Ts...>> : Renderer<Pack<no_qualifier<Ts>...>> {};

/******************************************************************************/

template <class T, std::size_t N, class A>
struct Opaque<boost::container::small_vector<T, N, A>> : Opaque<T> {};

template <class T, std::size_t N, class A>
struct Renderer<boost::container::small_vector<T, N, A>, std::enable_if_t<!Opaque<T>::value>> {
    void operator()(Document &doc) {doc.render(Type<T>());}
};

/******************************************************************************/

template <class T>
struct ToValue<std::optional<T>> {
    Value operator()(std::optional<T> t) const {
        if (!t) return {};
        else return *t;
    }
};

template <class T>
struct FromValue<std::optional<T>> {
    std::optional<T> operator()(Value &&u, Dispatch &message) const {
        if (!u.has_value()) return {};
        return FromValue<T>(message)(std::move(u));
    }
};

template <class ...Ts>
struct ToValue<std::variant<Ts...>> {
    Value operator()(std::variant<Ts...> t) const {
        return std::visit([](auto &t) -> Value {
            return ToValue<no_qualifier<decltype(t)>>(std::move(t));
        }, t);
    }
};


template <class T, class ...Ts>
struct FromValue<std::variant<T, Ts...>> {
    template <class V1, class U>
    std::variant<T, Ts...> scan(Pack<V1>, U &u, Dispatch &tmp, Dispatch &msg) const {
        try {return FromValue<V1>{tmp}(std::move(u));}
        catch (DispatchError const &err) {}
        throw msg.error("no conversions succeeded", typeid(std::variant<T, Ts...>), typeid(U));
    }

    template <class V1, class V2, class U, class ...Vs>
    std::variant<T, Ts...> scan(Pack<V1, V2, Vs...>, U &u, Dispatch &tmp, Dispatch &msg) const {
        try {return FromValue<V1>{tmp}(std::move(u));}
        catch (DispatchError const &err) {}
        return scan(Pack<V2, Vs...>(), u, tmp, msg);
    }

    template <class U>
    std::variant<T, Ts...> operator()(U &&u, Dispatch &msg) const {
        Dispatch tmp = msg;
        return scan(Pack<T, Ts...>(), u, tmp, msg);
    }
};

/******************************************************************************/

template <class T, class U>
struct ToValue<std::pair<T, U>> {
    static_assert(!std::is_reference_v<T>);
    static_assert(!std::is_reference_v<U>);

    Sequence operator()(std::pair<T, U> p) const {
        return Sequence::from_values(std::move(p.first), std::move(p.second));
    }
};


template <class ...Ts>
struct ToValue<std::tuple<Ts...>> {
    static_assert(!((std::is_reference_v<Ts>) || ...));

    template <std::size_t ...Is>
    Sequence get(std::tuple<Ts...> &&t, std::index_sequence<Is...>) const {
        return Sequence::from_values(std::move(std::get<Is>(t))...);
    }

    Sequence operator()(std::tuple<Ts...> t) const {
        return get(std::move(t), std::make_index_sequence<sizeof...(Ts)>());
    }
};

/******************************************************************************/

template <class V>
struct CompiledSequenceFromValue {
    template <std::size_t ...Is>
    V extract2(Sequence &u, Dispatch &message, std::index_sequence<Is...>) const {
        message.indices.emplace_back(0);
        return {(message.indices.back() = Is,
            FromValue<std::tuple_element_t<Is, V>>()(std::move(u.contents[Is]), message)
            )...};
    }

    V extract(Sequence &&u, Dispatch &message) const {
        if (u.contents.size() != std::tuple_size_v<V>) {
            throw message.error("wrong sequence length", typeid(Sequence), typeid(V), std::tuple_size_v<V>, u.contents.size());
        }
        V &&v = extract2(u, message, std::make_index_sequence<std::tuple_size_v<V>>());
        message.indices.pop_back();
        return v;
    }

    template <class U>
    V operator()(U &&u, Dispatch &message) const {
        static_assert(U::aaa, "");
        throw message.error("expected sequence", typeid(U), typeid(V));
    }

    V operator()(Value &&u, Dispatch &message) const {
        auto ptr = &u;
        // if (auto p = std::any_cast<Reference>(&u)) ptr = p->get();
        if (auto p = cast<no_qualifier<V>>(ptr)) return *p;
        if (auto p = cast<Sequence>(ptr)) return extract(std::move(*p), message);
        throw message.error(u.has_value() ? "mismatched class" : "object was already moved", u.type(), typeid(V));
    }
};

/// The default implementation is to accept convertible arguments or Value of the exact typeid match
template <class T1, class T2>
struct FromValue<std::pair<T1, T2>> : CompiledSequenceFromValue<std::pair<T1, T2>> {};

template <class ...Ts>
struct FromValue<std::tuple<Ts...>> : CompiledSequenceFromValue<std::tuple<Ts...>> {};

template <class T, std::size_t N>
struct FromValue<std::array<T, N>> : CompiledSequenceFromValue<std::array<T, N>> {};

}