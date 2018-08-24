#pragma once
#include "Document.h"
#include <tuple>
#include <utility>
#include <array>
#include <optional>

namespace cpy {

template <class T, class U>
struct Renderer<std::pair<T, U>> : Renderer<Pack<no_qualifier<T>, no_qualifier<U>>> {};

template <class ...Ts>
struct Renderer<std::tuple<Ts...>> : Renderer<Pack<no_qualifier<Ts>...>> {};

template <class T, std::size_t N>
struct Renderer<std::array<T, N>> : Renderer<Pack<no_qualifier<T>>> {};

template <class T>
struct Renderer<std::optional<T>> : Renderer<Pack<no_qualifier<T>>> {};

/******************************************************************************/

template <class T>
struct ToValue<std::optional<T>> {
    Value operator()(std::optional<T> t) const {
        if (!t) return std::monostate();
        else return *t;
    }
};

template <class T>
struct FromValue<std::optional<T>> {
    DispatchMessage &message;
    template <class U>
    std::optional<T> operator()(U &&u) const {
        if (std::holds_alternative<std::monostate>(u)) return {};
        return FromValue<T>(message)(static_cast<U &&>(u));
    }
};

template <class ...Ts>
struct ToValue<std::variant<Ts...>> {
    Value operator()(std::variant<T> t) const {
        return std::visit([](auto &t) -> Value {return std::move(t);}, t);
    }
};

template <class T>
struct FromValue<std::optional<T>> {
    DispatchMessage &message;

    template <class U>
    std::optional<T> operator()(U &&u) const {
        if (std::holds_alternative<std::monostate>(u)) return {};
        return FromValue<T>(message)(static_cast<U &&>(u));
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
    static_assert(!any_of_c<std::is_reference_v<Ts>...>);

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
    Dispatch &message;

    template <class U>
    V operator()(U &&u) const {
        throw message.error("expected sequence", typeid(U), typeid(V));
    }

    template <std::size_t ...Is>
    V get(Sequence &u, std::index_sequence<Is...>) const {
        message.indices.emplace_back(0);
        return {(message.indices.back() = Is,
            std::visit(FromValue<std::tuple_element_t<Is, V>>{message}, std::move(u.contents[Is].var))
            )...};
    }

    V operator()(Sequence &&u) const {
        if (u.contents.size() != std::tuple_size_v<V>) {
            throw message.error("wrong sequence length", typeid(Sequence), typeid(V), std::tuple_size_v<V>, u.contents.size());
        }
        V &&v = get(u, std::make_index_sequence<std::tuple_size_v<V>>());
        message.indices.pop_back();
        return v;
    }

    V operator()(Any &&u) const {
        auto ptr = &u;
        if (auto p = std::any_cast<AnyReference>(&u)) ptr = p->get();
        if (auto p = std::any_cast<no_qualifier<V>>(ptr)) return static_cast<V>(*p);
        throw message.error(u.has_value() ? "mismatched class" : "object was already moved", u.type(), typeid(V));
    }
};

/// The default implementation is to accept convertible arguments or Any of the exact typeid match
template <class T1, class T2>
struct FromValue<std::pair<T1, T2>> : CompiledSequenceFromValue<std::pair<T1, T2>> {};

template <class ...Ts>
struct FromValue<std::tuple<Ts...>> : CompiledSequenceFromValue<std::tuple<Ts...>> {};

template <class T, std::size_t N>
struct FromValue<std::array<T, N>> : CompiledSequenceFromValue<std::array<T, N>> {};

}