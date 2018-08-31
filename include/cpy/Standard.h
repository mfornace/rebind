#pragma once
#include "Document.h"
#include <tuple>
#include <utility>
#include <array>
#include <optional>

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
        if (!t) return std::monostate();
        else return *t;
    }
};

template <class T>
struct FromValue<std::optional<T>> {
    Dispatch &message;
    template <class U>
    std::optional<T> operator()(U &&u) const {
        if (std::holds_alternative<std::monostate>(u)) return {};
        return FromValue<T>(message)(static_cast<U &&>(u));
    }
};

template <class ...Ts>
struct ToValue<std::variant<Ts...>> {
    Value operator()(std::variant<Ts...> t) const {
        return std::visit([](auto &t) -> Value {return std::move(t);}, t);
    }
};


template <class T, class ...Ts>
struct FromValue<std::variant<T, Ts...>> {
    Dispatch &message;

    template <class V1, class U>
    std::variant<T, Ts...> scan(Pack<V1>, U &u, Dispatch &msg) const {
        try {return FromValue<V1>{msg}(std::move(u));}
        catch (DispatchError const &err) {}
        throw message.error("no conversions succeeded", typeid(std::variant<T, Ts...>), typeid(U));
    }

    template <class V1, class V2, class U, class ...Vs>
    std::variant<T, Ts...> scan(Pack<V1, V2, Vs...>, U &u, Dispatch &msg) const {
        try {return FromValue<V1>{msg}(std::move(u));}
        catch (DispatchError const &err) {}
        return scan(Pack<V2, Vs...>(), u, msg);
    }

    template <class U>
    std::variant<T, Ts...> operator()(U &&u) const {
        Dispatch msg = message;
        return scan(Pack<T, Ts...>(), u, msg);
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