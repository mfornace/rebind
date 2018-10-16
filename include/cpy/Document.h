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
template <> struct Opaque<Reference>        : std::true_type {};
template <> struct Opaque<ArgPack>          : std::true_type {};
template <> struct Opaque<Caller>           : std::true_type {};

template <class T>
struct Opaque<T, std::enable_if_t<(std::is_arithmetic_v<T>)>> : std::true_type {};

template <class T>
struct Opaque<Vector<T>> : Opaque<T> {};

/******************************************************************************/

struct TypeData {
    std::map<std::string, Function> methods;
    std::map<std::type_index, Value> data;
};

struct Document {
    std::map<std::string, Value> contents;
    std::map<std::type_index, std::pair<std::string const, Value> *> types;

    TypeData & type(std::type_index t, std::string s, Value data={}) {
        auto it = contents.emplace(std::move(s), TypeData()).first;
        if (auto p = it->second.target<TypeData>()) {
            p->data.emplace(t, std::move(data));
            types[t] = &(*it);
            return *p;
        }
        throw std::runtime_error("bada");
    }

    template <class T, std::enable_if_t<Opaque<T>::value, int> = 0>
    bool render(Type<T> t={}) {
        static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
        return types.emplace(typeid(T), nullptr).second;
    }

    template <class T, std::enable_if_t<!Opaque<T>::value, int> = 0>
    bool render(Type<T> t={}) {
        static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
        return types.emplace(typeid(T), nullptr).second ? Renderer<T>()(*this), true : false;
    }

    template <class ...Ts>
    void render(Pack<Ts...>) {(render(Type<no_qualifier<Ts>>()), ...);}

    template <class T>
    void object(std::string_view s, T value) {
        render(Type<T>());
        contents.emplace(std::move(s), std::move(value));
    }

    /// Export function and its signature
    template <class F>
    void function(std::string s, F functor) {
        render(typename Signature<F>::no_qualifier());
        auto it = contents.find(s);
        if (it == contents.end()) {
            contents.emplace(std::move(s), Function().emplace(std::move(functor)));
        } else {
            if (auto f = it->second.target<Function>())
                f->emplace(std::move(functor));
            else throw std::runtime_error("function with nonfunction");
        }
    }

    /// Always a function - no vagueness here
    template <class F, class ...Ts>
    void method(std::type_index t, std::string name, F f) {
        Signature<F>::no_qualifier::for_each([&](auto r) {if (t != +r) render(+r);});
        if (auto p = types.at(t)->second.target<TypeData>())
            p->methods[std::move(name)].emplace(std::move(f));
        else throw std::runtime_error("bad");
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