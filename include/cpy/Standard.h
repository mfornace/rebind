#pragma once
#include "Document.h"
#include "Types.h"
#include <tuple>
#include <utility>
#include <array>
#include <optional>
#include <variant>
#include <map>

namespace cpy {

/******************************************************************************/

template <class T>
struct Response<std::optional<T>> {
    bool operator()(Variable &out, std::optional<T> const &v, std::type_index t) const {
        return v ? Response<T>()(out, v, std::move(t)), true : false;
    }
    bool operator()(Variable &out, std::optional<T> const &p, std::type_index t, Qualifier q) const {
        return p ? Response<std::remove_cv_t<T>>()(out, *p, std::move(t), q), true : false;
    }
};

template <class T>
struct Request<std::optional<T>> {
    std::optional<std::optional<T>> operator()(Variable const &v, Dispatch &msg) const {
        std::optional<std::optional<T>> out;
        if (!v || v.request<std::nullptr_t>()) out.emplace();
        else if (auto p = v.request<std::remove_cv_t<T>>(msg))
            out.emplace(std::move(*p));
        return out;
    }
};

/******************************************************************************/

template <class T>
struct Response<std::shared_ptr<T>> {
    bool operator()(Variable &out, std::shared_ptr<T> const &p, std::type_index t) const {
        DUMP("shared_ptr value", t.name(), bool(p));
        if (!p) return false;
        if (t == typeid(std::remove_cv_t<T>)) return out = *p, true;
        else return Response<std::remove_cv_t<T>>()(out, *p, std::move(t));
    }
    bool operator()(Variable &out, std::shared_ptr<T> const &p, std::type_index t, Qualifier q) const {
        DUMP("shared_ptr reference", t.name(), typeid(std::remove_cv_t<T>).name(), (t == typeid(std::remove_cv_t<T>)), bool(p), q, out.qualifier());
        if (!p) return false;
        if (t == typeid(std::remove_cv_t<T>)) return out = {Type<T &>(), *p}, true;
        else return Response<std::remove_cv_t<T>>()(out, *p, std::move(t), q);
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
    bool operator()(Variable &out, std::variant<Ts...> v, std::type_index t) const {
        DUMP(out.name(), typeid(v).name(), t.name());
        DUMP(v.index());
        return std::visit([&](auto const &x) {
            DUMP(typeid(x).name());
            return Response<no_qualifier<decltype(x)>>()(out, x, t);
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
struct MapResponse {
    using T = std::pair<typename V::key_type, typename V::mapped_type>;

    bool operator()(Variable &out, V const &v, std::type_index t) const {
        Vector<T> o(std::begin(v), std::end(v));
        if (t == typeid(Vector<T>)) return out = std::move(o), true;
        if (t == typeid(Sequence)) return out = Sequence(std::make_move_iterator(std::begin(o)), std::make_move_iterator(std::end(o))), true;
        return false;
    }
};

template <class V>
struct MapRequest {
    using T = std::pair<typename V::key_type, typename V::mapped_type>;

    std::optional<V> operator()(Variable const &v, Dispatch &msg) const {
        std::optional<V> out;
        if (auto p = v.request<Vector<T>>())
            out.emplace(std::make_move_iterator(std::begin(*p)), std::make_move_iterator(std::end(*p)));
        return out;
    }
};

template <class K, class V, class C, class A>
struct Request<std::map<K, V, C, A>> : MapRequest<std::map<K, V, C, A>> {};

template <class K, class V, class C, class A>
struct Response<std::map<K, V, C, A>> : MapResponse<std::map<K, V, C, A>> {};

/******************************************************************************/

template <class F>
struct FunctionRequest {
    std::optional<F> operator()(Variable const &v, Dispatch &msg) const {
        if (auto p = v.request<Callback<typename F::result_type>>(msg)) return F{std::move(*p)};
        return {};
    }
};

template <class R, class ...Ts>
struct Request<std::function<R(Ts...)>> : FunctionRequest<std::function<R(Ts...)>> {};

/******************************************************************************/

}
