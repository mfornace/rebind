#pragma once
#include "Function.h"
#include "Ref.h"
#include <map>

namespace rebind {

/******************************************************************************/

struct Document;

struct NoRender {void operator()(Document &) const {}};

void render_default(Document &, std::type_info const &);

template <class T, class=void>
struct Renderer {
    void operator()(Document &doc) const {render_default(doc, typeid(T));}
};

/******************************************************************************/

struct Document {
    // Global variables, types, and functions
    std::map<std::string, Value> contents;

    // Map from type index to the associated table of C++ type data
    std::map<Index, Table> types;

    /**************************************************************************/

    // Declare a new type
    template <class T>
    Table type(Type<T> t, std::string_view s, Value data={}) {
        auto table = get_table<T>();
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
    void function(std::string name, F functor) {
        render(typename Signature<F>::unqualified());
        find_function(std::move(name)).emplace(Overload::from<N>(std::move(functor)));
    }

    /// Always a function - no vagueness here
    template <int N=-1, class F, class ...Ts>
    void method(Index t, std::string name, F f) {
        Signature<F>::unqualified::for_each([&](auto r) {if (t != +r) render(+r);});
        find_method(t, std::move(name)).emplace(Overload::from<N>(std::move(f)));
    }

    /**************************************************************************/

    // Declare a new type with an already fetched Table
    void type(Index t, std::string_view s, Value &&data, Table);

    // Find or create a function for a given type and method name
    Function & find_method(Index t, std::string_view name);

    // Find or create a function
    Function & find_function(std::string_view s);

    // Render a given type
    template <class T>
    bool render(Type<T> t={}) {
        static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
        if constexpr(is_usable<T>) {
            return types.emplace(typeid(T), get_table<T>()).second ? Renderer<T>()(*this), true : false;
        } else return false;
    }

    // Render a list of types
    template <class ...Ts>
    void render(Pack<Ts...>) {(render(Type<unqualified<Ts>>()), ...);}
};

/******************************************************************************/

Document & document() noexcept;

/// Specialization for a Pack: renders each Type in the Pack
template <class ...Ts>
struct Renderer<Pack<Ts...>> {
    void operator()(Document &doc) {(doc.render(Type<Ts>()), ...);}
};

void render(int, int); // undefined; provided so identifier is accepted by compiler.

/// The default implementation is to call render(Document &, Type<T>) via ADL
template <class T>
struct Renderer<T, std::void_t<decltype(render(std::declval<Document &>(), Type<T>()))>> {
    void operator()(Document &doc) const {render(doc, Type<T>());}
};

/******************************************************************************/

}

// #include "StandardTypes.h"
