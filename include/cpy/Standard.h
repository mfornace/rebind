#pragma once
#include "Document.h"
#include "Types.h"
#include <tuple>
#include <utility>
#include <array>
#include <optional>
#include <variant>
#include <map>

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

template <class V>
struct VectorRenderer {
    void operator()(Document &doc) const {
        doc.render<typename V::value_type>();
        doc.type(typeid(V), "std.Vector");
        doc.method(typeid(V), "append", [](V &v, typename V::value_type o) {v.emplace_back(std::move(o));});
        doc.method(typeid(V), "[]", [](V &v, std::size_t i) -> decltype(v.at(i)) {return v.at(i);});
        doc.method(typeid(V), "__len__", [](V const &v) {return v.size();});
    }
};

template <class T, class A>
struct Renderer<std::vector<T, A>> : VectorRenderer<std::vector<T, A>> {};

template <class T, std::size_t N, class A>
struct Renderer<boost::container::small_vector<T, N, A>> : VectorRenderer<boost::container::small_vector<T, N, A>> {};

template <class V>
struct MapRenderer {
    void operator()(Document &doc) const {
        doc.render<typename V::value_type>();
        doc.type(typeid(V), "std.Map");
        doc.method(typeid(V), "emplace", [](V &v, typename V::key_type k, typename V::mapped_type p) {v.emplace(std::move(k), std::move(p));});
        doc.method(typeid(V), "[]", [](V &v, typename V::mapped_type const &t) -> decltype(v.at(t)) {return v.at(t);});
        doc.method(typeid(V), "__len__", [](V const &v) {return v.size();});
    }
};

template <class T, class C, class A>
struct Renderer<std::map<T, C, A>> : MapRenderer<std::map<T, C, A>> {};

// template <class T, std::size_t N, class A>
// struct Opaque<boost::container::small_vector<T, N, A>> : Opaque<T> {};

// template <class T, std::size_t N, class A>
// struct Renderer<boost::container::small_vector<T, N, A>, std::enable_if_t<!Opaque<T>::value>> {
//     void operator()(Document &doc) {doc.render(Type<T>());}
// };

template <class T, std::size_t N, class A>
struct Request<boost::container::small_vector<T, N, A>> : VectorRequest<boost::container::small_vector<T, N, A>> {};

template <class T, std::size_t N, class A>
struct Response<boost::container::small_vector<T, N, A>> : VectorResponse<boost::container::small_vector<T, N, A>> {};

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

template <class R, class ...Ts>
struct Renderer<std::function<R(Ts...)>> : Renderer<Pack<no_qualifier<R>, no_qualifier<Ts>...>> {};

/******************************************************************************/

template <class V>
struct MapResponse {
    using T = std::pair<typename V::key_type, typename V::mapped_type>;

    void operator()(Variable &out, V const &v, std::type_index t) const {
        Vector<T> o(std::begin(v), std::end(v));
        if (t == typeid(Vector<T>)) out = std::move(o);
        if (t == typeid(Sequence)) out = Sequence(std::make_move_iterator(std::begin(o)), std::make_move_iterator(std::end(o)));
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

}
