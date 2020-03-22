#pragma once
#include "Function.h"
#include "Ref.h"
#include <set>

namespace rebind {

/******************************************************************************/

struct Schema;

struct NoRender {void operator()(Schema &) const {}};

void render_default(Schema &, std::type_info const &);

template <class T, class=void>
struct Renderer {
    void operator()(Schema &s) const {render_default(s, typeid(T));}
};

/******************************************************************************/

struct Schema {
    // Global variables, types, and functions
    std::map<std::string, Value, std::less<>> contents;

    // Set of all type definitions
    std::set<Index> types;

    /**************************************************************************/

    // Declare a new type
    template <class T>
    Index type(Type<T> t, std::string_view s, Value data={}) {
        auto table = fetch<T>();
        type(t, s, std::move(data), table);
        return table;
    }

    // Render a list of types
    template <class T>
    void object(std::string s, T value) {
        render(Type<T>());
        if (auto p = contents.emplace(std::move(s), std::move(value)); !p.second)
            throw std::runtime_error("already rendered object with key " + p.first->first);
    }

    /// Export function and its signature. N may be given as the number of mandatory arguments
    template <int N=-1, class F>
    void function(std::string_view name, F functor) {
        render(typename Signature<F>::unqualified());
        find_global(name) = declare_function<N>(std::move(functor));
    }

    /// Always a function - no vagueness here
    template <int N=-1, class F, class ...Ts>
    void method(Index t, std::string_view name, F functor) {
        Signature<F>::unqualified::for_each([&](auto r) {if (t != +r) render(+r);});
        find_property(t, name) = declare_function<N>(std::move(functor));
    }

    /**************************************************************************/

    // Declare a new type with an already fetched Index
    void type(Index t, std::string_view s, Value &&data, Index);

    // Find or create a function for a given type and method name
    Value & find_property(Index t, std::string_view name);

    // Find or create a function
    Value & find_global(std::string_view s);

    // Render a given type
    template <class T>
    bool render(Type<T> t={}) {
        static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
        if constexpr(is_usable<T>) {
            return types.emplace(fetch<T>()).second ? Renderer<T>()(*this), true : false;
        } else return false;
    }

    // Render a list of types
    template <class ...Ts>
    void render(Pack<Ts...>) {(render(Type<unqualified<Ts>>()), ...);}
};

/******************************************************************************/

Schema & schema() noexcept;

/******************************************************************************/

/// Specialization for a Pack: renders each Type in the Pack
template <class ...Ts>
struct Renderer<Pack<Ts...>> {
    void operator()(Schema &s) {(s.render(Type<Ts>()), ...);}
};

void render(int, int); // undefined; provided so identifier is accepted by compiler.

/// The default implementation is to call render(Schema &, Type<T>) via ADL
template <class T>
struct Renderer<T, std::void_t<decltype(render(std::declval<Schema &>(), Type<T>()))>> {
    void operator()(Schema &s) const {render(s, Type<T>());}
};

/******************************************************************************/

}

// #include "StandardTypes.h"
