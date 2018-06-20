#pragma once
#include "Value.h"

namespace cpy {

using Logs = std::vector<KeyPair>;

/******************************************************************************/

template <class T>
struct Ungluer {
    T const & operator()(T const &t) const {return t;}
};

template <class T>
decltype(auto) unglue(T const &t) {return Ungluer<T>()(t);}

/******************************************************************************/

template <class T, class=void>
struct AddKeyPairs {
    void operator()(Logs &v, T const &t) const {
        v.emplace_back(KeyPair{{}, make_value(t)});
    }
};

/******************************************************************************/

template <class K, class V>
struct Glue {
    K key;
    V value;
};

template <class V>
struct Gluer {
    template <class K>
    Glue<K, std::conditional_t<std::is_class_v<V>, V, V const &>>
        operator()(K key, V const &value) {return {std::move(key), value};}
};

template <class K, class V>
struct Gluer<Glue<K, V>> {
    template <class K2>
    Glue<K, V> const & operator()(K2 &&, Glue<K, V> const &v) {return v;}
};

/******************************************************************************/

template <class K, class V>
decltype(auto) glue(K key, V const &value) {return Gluer<V>()(key, value);}

template <class K, class V>
Glue<std::decay_t<K>, std::decay_t<V>> glue_value(K &&key, V &&value) {
    return {static_cast<K &&>(key), static_cast<V &&>(value)};
}

template <class K, class V>
struct Ungluer<Glue<K, V>> {
    decltype(auto) operator()(Glue<K, V> const &t) const {return Ungluer<V>()(t.value);}
};

template <class K, class V>
struct AddKeyPairs<Glue<K, V>> {
    void operator()(Logs &v, Glue<K, V> const &g) const {
        v.emplace_back(KeyPair{g.key, make_value(g.value)});
    }
};

/******************************************************************************/

struct FileLine {
    int line = 0;
    char const *file = nullptr;
};

inline auto file_and_line(char const *s, int i) {return FileLine{i, s};}

template <>
struct AddKeyPairs<FileLine> {
    void operator()(Logs &v, FileLine const &g) const {
        v.emplace_back(KeyPair{"file", static_cast<std::string_view>(g.file)});
        v.emplace_back(KeyPair{"line", static_cast<std::size_t>(g.line)});
    }
};

template <class T>
struct Comment {
    T comment;
    FileLine location;
};

template <class T>
Comment<T> comment(T t, char const *s, int i) {return {t, {i, s}};}

template <class T>
struct AddKeyPairs<Comment<T>> {
    void operator()(Logs &v, Comment<T> const &c) const {
        AddKeyPairs<FileLine>()(v, c.location);
        v.emplace_back(KeyPair{"comment", make_value(c.comment)});
    }
};

/******************************************************************************/

template <class L, class R>
struct ComparisonGlue {
    L lhs;
    R rhs;
    char const *op;
};

template <class X, class Y>
ComparisonGlue<X const &, Y const &> comparison_glue(X const &x, Y const &y, char const *op) {
    return {x, y, op};
}

template <class L, class R>
struct AddKeyPairs<ComparisonGlue<L, R>> {
    void operator()(Logs &v, ComparisonGlue<L, R> const &t) const {
        v.emplace_back(KeyPair{"lhs", make_value(t.lhs)});
        v.emplace_back(KeyPair{"rhs", make_value(t.rhs)});
        v.emplace_back(KeyPair{"op", Value(std::string_view(t.op))});
    }
};

/******************************************************************************/

}