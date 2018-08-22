#pragma once
#include "Function.h"
#include <map>

namespace cpy {

/******************************************************************************/

template <class T, class=void>
struct Renderer;

template <class T, class=void>
struct Opaque : std::false_type {};

template <class T>
struct Opaque<T, std::enable_if_t<(std::is_arithmetic_v<T>)>> : std::true_type {};

template <>
struct Opaque<char const *> : std::true_type {};

template <>
struct Opaque<void> : std::true_type {};

template <>
struct Opaque<std::string> : std::true_type {};

template <>
struct Opaque<std::string_view> : std::true_type {};

template <>
struct Opaque<Function> : std::true_type {};

template <>
struct Opaque<Value> : std::true_type {};

template <class T>
struct Opaque<Vector<T>> : Opaque<T> {};

template <>
struct Opaque<CallingContext> : std::true_type {};

/******************************************************************************/

struct Methods {
    Methods(bool=true) noexcept {}
    std::string name;
    Zip<std::string, Function> methods;
};

struct Document {
    Zip<std::string, Value> values;
    std::map<std::type_index, Methods> types;

    template <class T, std::enable_if_t<Opaque<T>::value, int> = 0>
    bool render(Type<T> t={}) {
        static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
        return types.emplace(typeid(T), true).second;
    }

    template <class T, std::enable_if_t<!Opaque<T>::value, int> = 0>
    bool render(Type<T> t={}) {
        static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
        return types.emplace(typeid(T), true).second ? Renderer<T>()(*this), true : false;
    }

    // template <class O, class ...Ts>
    // void define(char const *s, O &&o, Ts &&...ts) {
    //     values.emplace_back(s, Function(static_cast<O &&>(o), static_cast<Ts &&>(ts)...));
    // }

    template <class O>
    void recurse(char const *s, O &&o, Vector<std::string> v={}) {
        Signature<no_qualifier<O>>::for_each([&](auto t) {render(+t);});
        values.emplace_back(s, Function(static_cast<O &&>(o), v));
    }

    void type(std::type_index t, std::string s) {types[t].name = std::move(s);}

    template <class F, class ...Ts>
    void method(std::type_index t, std::string name, F &&f, Ts &&...ts) {
        types[t].methods.emplace_back(std::move(name), static_cast<F &&>(f), static_cast<Ts &&>(ts)...);
    }
};

Document & document() noexcept;

template <>
struct Renderer<void> {
    void operator()(Document &) const {}
};

// template <class T>
// struct Renderer<Vector<T>, std::enable_if_t> {
//     void operator()(Document &doc) const {Renderer<T>()(doc);}
// };

template <class T, std::enable_if_t<Opaque<T>::value>>
void render(Document &doc, Type<T>) {}

template <class T, std::enable_if_t<!Opaque<T>::value>>
void render(Document &doc, Type<Vector<T>>) {doc.render(Type<T>());}


template <class T, class>
struct Renderer {
    void operator()(Document &doc) const {render(doc, Type<T>());}
};


// template <class T, class>
// struct Renderer {
// };

/******************************************************************************/

}