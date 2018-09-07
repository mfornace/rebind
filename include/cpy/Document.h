#pragma once
#include "Function.h"
#include <map>

namespace cpy {

/******************************************************************************/

template <class T, class=void>
struct Renderer;

template <class T, class=void>
struct Opaque : std::false_type {};

template <> struct Opaque<char const *>     : std::true_type {};
template <> struct Opaque<void>             : std::true_type {};
template <> struct Opaque<std::string>      : std::true_type {};
template <> struct Opaque<std::string_view> : std::true_type {};
template <> struct Opaque<std::type_index>  : std::true_type {};
template <> struct Opaque<BinaryView>       : std::true_type {};
template <> struct Opaque<BinaryData>       : std::true_type {};
template <> struct Opaque<Binary>           : std::true_type {};
template <> struct Opaque<Function>         : std::true_type {};
template <> struct Opaque<Value>            : std::true_type {};
template <> struct Opaque<Arg>              : std::true_type {};
template <> struct Opaque<ArgPack>          : std::true_type {};
template <> struct Opaque<Caller>           : std::true_type {};

template <class T>
struct Opaque<T, std::enable_if_t<(std::is_arithmetic_v<T>)>> : std::true_type {};

template <class T>
struct Opaque<Vector<T>> : Opaque<T> {};

/******************************************************************************/

struct Methods {
    Methods(bool=true) noexcept {}
    std::string name;
    Zip<std::string, Function> methods;
};

struct Document {
    Zip<std::string, Value> values;
    std::map<std::type_index, Methods> types;

    void type(std::type_index t, std::string s) {types[t].name = std::move(s);}

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

    template <class ...Ts>
    void render(Pack<Ts...>) {(render(Type<Ts>()), ...);}

    template <class T>
    void object(std::string_view s, T value) {
        render(Type<T>());
        values.emplace_back(s, std::move(value));
    }

    /// Export function and its signature
    template <class F>
    void function(std::string_view s, F functor) {
        render(typename Signature<F>::no_qualifier());
        values.emplace_back(s, make_function(std::move(functor)));
    }

    /// Always a function - no vagueness here
    template <class F, class ...Ts>
    void method(std::type_index t, std::string name, F f) {
        Signature<F>::no_qualifier::apply2([&](auto r, auto c, auto ...ts) {
            std::cout << name << std::endl;
            std::cout << std::type_index(r).name() << std::endl;
            std::cout << std::type_index(c).name() << std::endl;
            render(r); (render(ts), ...);
        });
        types[t].methods.emplace_back(std::move(name), make_function(std::move(f)));
    }
};

Document & document() noexcept;

struct NoRender {void operator()(Document &) const {}};

template <class ...Ts>
struct Renderer<Pack<Ts...>> {
    void operator()(Document &doc) {
        (doc.render(Type<std::conditional_t<Opaque<Ts>::value, void, Ts>>()), ...);
    }
};

void render(int, int); // undefined

// Opaque never handled because of short-circuiting above
template <class T>
struct Renderer<Vector<T>, std::enable_if_t<!Opaque<T>::value>> {
    void operator()(Document &doc) {doc.render(Type<T>());}
};

/// The default implementation is to call render(Document &, Type<T>) via ADL
template <class T, class>
struct Renderer {
    void operator()(Document &doc) const {render(doc, Type<T>());}
};

/******************************************************************************/

}

#include "Standard.h"