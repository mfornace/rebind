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

template <class T, std::size_t N, class A>
struct Request<boost::container::small_vector<T, N, A>> : VectorRequest<boost::container::small_vector<T, N, A>> {};

template <class T, std::size_t N, class A>
struct Response<boost::container::small_vector<T, N, A>> : SimplifyVector<boost::container::small_vector<T, N, A>> {};

template <class T, std::size_t N>
struct Response<std::array<T, N>> : SimplifyVector<std::array<T, N>> {};

/******************************************************************************/

template <class T>
struct Response<std::optional<T>> {
    void operator()(Variable &out, std::optional<T> const &v, std::type_index t) const {
        if (v) Response<T>()(out, v, std::move(t));
    }
    void operator()(Variable &out, std::optional<T> const &p, std::type_index t, Qualifier q) const {
        if (p) Response<std::remove_cv_t<T>>()(out, *p, std::move(t), q);
    }
};

template <class T>
struct Request<std::optional<T>> {
    std::optional<std::optional<T>> operator()(Variable const &v, Dispatch &msg) const {
        std::optional<std::optional<T>> out;
        if (!v) out.emplace();
        else if (auto p = v.request<std::remove_cv_t<T>>(msg))
            out.emplace(std::move(*p));
        return out;
    }
};

/******************************************************************************/

template <class T>
struct Response<std::shared_ptr<T>> {
    void operator()(Variable &out, std::shared_ptr<T> const &p, std::type_index t) const {
        DUMP("value", t.name(), bool(p));
        if (!p) return;
        if (t == typeid(std::remove_cv_t<T>)) out = *p;
        else Response<std::remove_cv_t<T>>()(out, *p, std::move(t));
    }
    void operator()(Variable &out, std::shared_ptr<T> const &p, std::type_index t, Qualifier q) const {
        DUMP("reference", t.name(), typeid(std::remove_cv_t<T>).name(), bool(p), q);
        if (!p) return;
        if (t == typeid(std::remove_cv_t<T>)) out = {Type<T &>(), *p};
        else Response<std::remove_cv_t<T>>()(out, *p, std::move(t), q);
    }
};

template <class T>
struct Request<std::shared_ptr<T>> {
    std::optional<std::shared_ptr<T>> operator()(Variable const &v, Dispatch &msg) const {
        std::optional<std::shared_ptr<T>> out;
        if (!v) out.emplace();
        else if (auto p = v.request<std::remove_cv_t<T>>(msg))
            out.emplace(std::make_shared<T>(std::move(*p)));
        return out;
    }
};

/******************************************************************************/

template <class ...Ts>
struct Response<std::variant<Ts...>> {
    void operator()(Variable &out, std::variant<Ts...> v, std::type_index t) const {
        DUMP(out.name(), typeid(v).name(), t.name());
        DUMP(v.index());
        std::visit([&](auto const &x) {
            DUMP(typeid(x).name());
            Response<no_qualifier<decltype(x)>>()(out, x, t);
        }, v);
    }
};

template <class ...Ts>
struct Request<std::variant<Ts...>> {
    template <class T>
    static bool put(std::optional<std::variant<Ts...>> &out, Variable const &v, Dispatch &msg) {
        if (auto p = v.request<T>(msg)) return out.emplace(std::move(*p)), true;
        return false;
    }

    std::optional<std::variant<Ts...>> operator()(Variable const &v, Dispatch &msg) const {
        std::optional<std::variant<Ts...>> out;
        (void) (put<Ts>(out, v, msg) || ...);
        return out;
    }
};

/******************************************************************************/

template <class V>
struct CompiledSequenceResponse {
    template <class O, std::size_t ...Is>
    O put(V const &v, std::index_sequence<Is...>) const {
        O o;
        o.reserve(sizeof...(Is));
        (o.emplace_back(std::get<Is>(v)), ...);
        return o;
    }

    void operator()(Variable &out, V const &v, std::type_index t) const {
        if (t == typeid(Sequence))
            out = put<Sequence>(v, std::make_index_sequence<std::tuple_size_v<V>>());
    }
};

template <class ...Ts>
struct Response<std::tuple<Ts...>> : CompiledSequenceResponse<std::tuple<Ts...>> {};

template <class T, class U>
struct Response<std::pair<T, U>> : CompiledSequenceResponse<std::pair<T, U>> {};

/******************************************************************************/

template <class V>
struct CompiledSequenceRequest {
    template <class ...Ts>
    static void combine(std::optional<V> &out, Ts &&...ts) {
        DUMP("trying CompiledSequenceRequest combine", bool(ts)...);
        if ((bool(ts) && ...)) out = V{*static_cast<Ts &&>(ts)...};
    }

    template <class S, std::size_t ...Is>
    static void request_each(std::optional<V> &out, S &s, Dispatch &msg, std::index_sequence<Is...>) {
        DUMP("trying CompiledSequenceRequest request");
        combine(out, std::move(s[Is]).request(msg, Type<std::tuple_element_t<Is, V>>())...);
    }

    template <class S>
    static void check(std::optional<V> &out, S &s, Dispatch &msg) {
        DUMP("trying CompiledSequenceRequest check");
        if (std::size(s) != std::tuple_size_v<V>) {
            msg.error("wrong sequence length", typeid(S), typeid(V), std::tuple_size_v<V>, s.size());
        } else {
            msg.indices.emplace_back(0);
            request_each(out, s, msg, std::make_index_sequence<std::tuple_size_v<V>>());
            msg.indices.pop_back();
        }
    }

    std::optional<V> operator()(Variable r, Dispatch &msg) const {
        std::optional<V> out;
        DUMP("trying CompiledSequenceRequest", r.type().name());
        if constexpr(!std::is_same_v<V, Sequence>) {
            if (auto p = r.request<Sequence>()) {
                DUMP("trying CompiledSequenceRequest2", r.type().name());
                check(out, *p, msg);
            } else {
                DUMP("trying CompiledSequenceRequest3", r.type().name());
                msg.error("expected sequence to make compiled sequence", r.type(), typeid(V));
            }
        }
        return out;
    }
};

/// The default implementation is to accept convertible arguments or Variable of the exact typeid match
template <class T>
struct Request<T, std::enable_if_t<std::tuple_size<T>::value >= 0>> : CompiledSequenceRequest<T> {};

/******************************************************************************/

template <class R, class ...Ts>
struct Renderer<std::function<R(Ts...)>> : Renderer<Pack<no_qualifier<R>, no_qualifier<Ts>...>> {};

/******************************************************************************/

}