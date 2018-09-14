#pragma once
#include "Document.h"
#include "Types.h"
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

// template <class T, std::size_t N, class A>
// struct FromArg<boost::container::small_vector<T, N, A>> : VectorFromArg<boost::container::small_vector<T, N, A>> {};

// template <class T, std::size_t N, class A>
// struct ToValue<boost::container::small_vector<T, N, A>> {
//     Value value(boost::container::small_vector<T, N, A> t) const {return {ValuePack(std::move(t)), bool()};}
// };

// /******************************************************************************/

// template <class T>
// struct ToValue<std::optional<T>> {
//     Value value(std::optional<T> t) const {
//         if (!t) return {};
//         else return {*t, bool()};
//     }
// };

// template <class T>
// struct FromValue<std::optional<T>> {
//     std::optional<T> operator()(Value u, Dispatch &msg) const {
//         if (!u.has_value()) return {};
//         return downcast<T>(std::move(u), msg);
//     }
// };

// template <class ...Ts>
// struct ToValue<std::variant<Ts...>> {
//     Value value(std::variant<Ts...> t) const {
//         return std::visit([](auto &&t) -> Value {
//             return ToValue<no_qualifier<decltype(t)>>()(static_cast<decltype(t) &&>(t));
//         }, t);
//     }
// };


// template <class T, class ...Ts>
// struct FromValue<std::variant<T, Ts...>> {
//     template <class V1, class U>
//     std::variant<T, Ts...> scan(Pack<V1>, U &u, Dispatch &tmp, Dispatch &msg) const {
//         try {return downcast<V1>(std::move(u), tmp);}
//         catch (DispatchError const &err) {}
//         throw msg.error("no conversions succeeded", typeid(std::variant<T, Ts...>), typeid(U));
//     }

//     template <class V1, class V2, class U, class ...Vs>
//     std::variant<T, Ts...> scan(Pack<V1, V2, Vs...>, U &u, Dispatch &tmp, Dispatch &msg) const {
//         try {return downcast<V1>(std::move(u), tmp);}
//         catch (DispatchError const &err) {}
//         return scan(Pack<V2, Vs...>(), u, tmp, msg);
//     }

//     template <class U>
//     std::variant<T, Ts...> operator()(U u, Dispatch &msg) const {
//         Dispatch tmp = msg;
//         return scan(Pack<T, Ts...>(), u, tmp, msg);
//     }
// };

// /******************************************************************************/

// template <class T, bool A>
// struct FromCompiledSequence {
//     template <std::size_t ...Is>
//     auto get(T &&t, std::index_sequence<Is...>) const {
//         return std::conditional_t<A, ArgPack, ValuePack>::from_values(std::get<Is>(std::move(t))...);
//     }

//     Value operator()(T t) const {
//         return get(std::move(t), std::make_index_sequence<std::tuple_size_v<T>>());
//     }
// };

// // template <class T>
// // struct ToValue<T, std::enable_if_t<std::tuple_size<T>::value >= 0>> : FromCompiledSequence<T, false> {

// // };

// // template <class T>
// // struct ToValue<T, std::enable_if_t<std::tuple_size<T>::value >= 0>> : FromCompiledSequence<T, true> {};

// /******************************************************************************/

template <class V, class S, std::size_t ...Is>
V to_compiled_sequence(S &s, Dispatch &msg, std::index_sequence<Is...>) {
    msg.indices.emplace_back(0);
    return {(msg.indices.back() = Is,
        downcast<std::tuple_element_t<Is, V>>(s[Is], msg)
        )...};
}

template <class V, class S>
V to_compiled_sequence(S &&s, Dispatch &msg) {
    if (s.size() != std::tuple_size_v<V>) {
        throw msg.error("wrong sequence length", typeid(S), typeid(V), std::tuple_size_v<V>, s.size());
    }
    V &&v = to_compiled_sequence<V>(s, msg, std::make_index_sequence<std::tuple_size_v<V>>());
    msg.indices.pop_back();
    return v;
}

// /******************************************************************************/

template <class V>
struct CompiledSequenceFromValue {
    V operator()(Reference r, Dispatch &msg) const {
        Value v = r.value({typeid(Vector<Reference>)});
        if (auto p = std::any_cast<Vector<Reference>>(&v))
            return to_compiled_sequence<V>(std::move(*p), msg);
        throw msg.error("mismatched class", r.type(), typeid(V));
    }
};

/// The default implementation is to accept convertible arguments or Value of the exact typeid match
template <class T>
struct FromReference<T, std::enable_if_t<std::tuple_size<T>::value >= 0>> : CompiledSequenceFromValue<T> {};

// template <class V>
// struct CompiledSequenceFromArg {
//     V operator()(Arg &out, Arg &&in, Dispatch &msg) const {
//         if (auto p = std::any_cast<ArgPack>(&in)) {
//             if (Debug) std::cout << "from argpack "  << p->size() << " " << typeid(V).name() << std::endl;
//             return to_compiled_sequence<V>(std::move(*p), msg);
//         }
//         if (auto p = std::any_cast<ValuePack>(&in)) {
//             if (Debug) std::cout << "from valuepack " << typeid(V).name() << std::endl;
//             return to_compiled_sequence<V>(std::move(*p), msg);
//         }
//         throw msg.error(in.has_value() ? "mismatched class" : "object was already moved", in.type(), typeid(V));
//     }
// };

// /// The default implementation is to accept convertible arguments or Value of the exact typeid match
// template <class T>
// struct FromArg<T, std::enable_if_t<std::tuple_size<T>::value >= 0>> : CompiledSequenceFromArg<T> {};

/******************************************************************************/

template <class R, class ...Ts>
struct Renderer<std::function<R(Ts...)>> : Renderer<Pack<no_qualifier<R>, no_qualifier<Ts>...>> {};

// template <class R, class ...Ts>
// struct FromValue<std::function<R(Ts...)>> {
//     std::function<R(Ts...)> operator()(Value v, Dispatch &msg) const {
//         return {};
//     }
// };

/******************************************************************************/

// template <class T>
// void render(cpy::Document &doc, Type<std::future<T>> t) {
//     using F = std::future<T>;
//     doc.type(t, "Future");
//     doc.method(t, "valid", &F::valid);
//     doc.method(t, "wait", &F::wait);
//     doc.method(t, "get", &F::get);
//     doc.method(t, "wait_for", [](F const &f, double t) {
//         return f.wait_for(std::chrono::duration<double>(t)) == std::future_status::ready;
//     });
// }

/******************************************************************************/

}