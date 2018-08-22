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
struct Opaque<Value> : std::true_type {};

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

    template <class O>
    void define(char const *s, O &&o) {
        values.emplace_back(s, Function(static_cast<O &&>(o)));
    }

    template <class O>
    void recurse(char const *s, O &&o) {
        Signature<no_qualifier<O>>::for_each([&](auto t) {render(+t);});
        values.emplace_back(s, Function(static_cast<O &&>(o)));
    }

    void type(std::type_index t, std::string s) {types[t].name = std::move(s);}

    template <class F>
    void method(std::type_index t, std::string name, F &&f) {
        types[t].methods.emplace_back(std::move(name), Function(static_cast<F &&>(f)));
    }
};

Document & document() noexcept;

template <>
struct Renderer<void> {
    void operator()(Document &) const {}
};

inline void render(Document &, Type<Value>) {}

template <class T, class>
struct Renderer {
    void operator()(Document &doc) const {render(doc, Type<T>());}
};


// template <class T, class>
// struct Renderer {
// };

/******************************************************************************/

}