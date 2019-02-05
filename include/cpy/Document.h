#pragma once
#include "Function.h"
#include <map>

namespace cpy {

/******************************************************************************/

struct Document;

struct NoRender {void operator()(Document &) const {}};

template <class T, class=void>
struct Renderer {
    void operator()(Document &) const {
        if (Debug) std::cout << "no render " << typeid(T).name() << std::endl;
    }
};

/******************************************************************************/

struct TypeData {
    std::map<std::string, Function> methods;
    std::map<std::type_index, Variable> data;
};

struct Document {
    std::map<std::string, Variable> contents;
    std::map<std::type_index, std::pair<std::string const, Variable> *> types;

    TypeData & type(std::type_index t, std::string s, Variable data={});

    Function & find_method(std::type_index t, std::string name);

    Function & find_function(std::string s);

    template <class T>
    bool render(Type<T> t={}) {
        static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
        return types.emplace(typeid(T), nullptr).second ? Renderer<T>()(*this), true : false;
    }

    template <class ...Ts>
    void render(Pack<Ts...>) {(render(Type<no_qualifier<Ts>>()), ...);}

    template <class T>
    void object(std::string s, T value) {
        render(Type<T>());
        if (auto p = contents.emplace(std::move(s), std::move(value)); !p.second)
            throw std::runtime_error("already rendered object with key " + p.first->first);
    }

    /// Export function and its signature. N may be given as the number of mandatory arguments
    template <int N=-1, class F>
    void function(std::string name, F functor) {
        render(typename Signature<F>::no_qualifier());
        find_function(std::move(name)).emplace<N>(std::move(functor));
    }

    /// Always a function - no vagueness here
    template <int N=-1, class F, class ...Ts>
    void method(std::type_index t, std::string name, F f) {
        Signature<F>::no_qualifier::for_each([&](auto r) {if (t != +r) render(+r);});
        find_method(t, std::move(name)).emplace<N>(std::move(f));
    }
};

Document & document() noexcept;

template <class ...Ts>
struct Renderer<Pack<Ts...>> {
    void operator()(Document &doc) {(doc.render(Type<Ts>()), ...);}
};

void render(int, int); // undefined

/// The default implementation is to call render(Document &, Type<T>) via ADL
template <class T>
struct Renderer<T, std::void_t<decltype(render(std::declval<Document &>(), Type<T>()))>> {
    void operator()(Document &doc) const {render(doc, Type<T>());}
};

/******************************************************************************/

}

#include "Standard.h"
