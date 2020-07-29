#pragma once
#include "Adapter.h"
#include "Value.h"
#include "Functor.h"

#include <map>
#include <vector>

namespace ara {

/******************************************************************************/

struct Schema {
    // Global variables and functions
    std::map<std::string, Value, std::less<>> contents;

    Value & emplace(std::string s, bool exist_ok=false) {
        auto p = contents.emplace(std::move(s), nullptr);
        if (p.second || exist_ok) return p.first->second;
        throw std::out_of_range("already rendered object with key " + p.first->first);
    }

    bool exists(std::string_view s) const {
        return contents.count(s);
    }

    auto dump() const {
        std::vector<std::pair<std::string_view, Value const *>> out;
        out.reserve(contents.size());
        for (auto const &p : contents) out.emplace_back(p.first, &p.second);
        return out;
    }

    // Emplace a key and value.
    // If "overwrite" is false and the key already exists, std::out_of_range is thrown.
    template <class T>
    T & object(std::string s, T value, bool overwrite=false) {
        return emplace(std::move(s), overwrite).emplace(std::move(value));
    }

    // Emplace a key and function object.
    // The function object which is emplaced is likely not the same type as the input functor.
    // N may be given as the number of mandatory arguments.
    template <int N=-1, class F>
    void function(std::string name, F functor) {
        emplace(std::move(name)).emplace(make_functor<N>(std::move(functor)));
    }

    Value const & operator[](std::string_view s) const {
        if (auto it = contents.find(s); it != contents.end()) {
            DUMP("contains value", s);
            DUMP("contains value", it->second.name());
            return it->second;
        }
        throw std::out_of_range("Schema key not found: " + std::string(s));
    }
};

/******************************************************************************/

// SFINAE not enabled on map
template <>
struct is_copy_constructible<Schema> : std::false_type {};

/******************************************************************************/

template <>
struct Impl<Schema> : Default<Schema> {
    static bool attribute(Target &t, Schema const &s, std::string_view key) {
        if (auto it = s.contents.find(key); it != s.contents.end()) {
            t.set_reference(it->second);
            return true;
        } else return false;
    }

    static bool method(Body m, Schema const &s) {
        DUMP("trying Schema method!");
        // if (!m.args.tags()) {
        //     DUMP("writing the schema");
        //     T::write(T::global_schema);
        //     m.stat = Method::None;
        //     return true;
        // }
        // DUMP("got to the module: # args =", m.args.size(), ", tags =", m.args.tags());
        // for (auto const &a : m.args) {
        //     DUMP("module call: argument =", a.name());
        // }

        // if (auto name = m.args.tag(0).get<Str>()) {
        //     DUMP(name->data());
        //     DUMP(std::string_view(*name), name->size());
        //     auto const &value = T::global_schema[*name];
        //     m.args.c.tags -= 1;
        //     DUMP("invoking module member! args=", m.args.size(), " tags=", m.args.tags());
        //     m.stat = Method::invoke(value.index(), m.target, value.address(), Mode::Read, m.args);
        //     return true;
        // }
        // DUMP("not a method!");
        return false;
    }
};

/******************************************************************************/

// template <class Mod>
// struct Module {
//     static void init(Caller caller={}) {
//         return Output<void, true>()([&](Target &t) {
//             return parts::with_args<0>([&](auto &args) {
//                 return Call::invoke(fetch(Type<Mod>()), t, args);
//             }, caller);
//         });
//     }

//     template <class T, bool Check=true, int N=1, class ...Ts>
//     static decltype(auto) call(Str name, Caller caller, Ts&& ...ts) {
//         return Output<T, Check>()([&](Target &t) {
//             return parts::with_args<N>([&](auto &args) {
//                 return Call::invoke(fetch(Type<Mod>()), t, args);
//             }, caller, name, std::forward<Ts>(ts)...);
//         });
//     }
// };

/******************************************************************************/


}
