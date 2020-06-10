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

template <class Module>
struct GlobalSchema {
    static Schema global_schema;
};

template <class Module>
Schema GlobalSchema<Module>::global_schema{};

template <class T>
struct Impl<T, std::void_t<decltype(T::global_schema)>> : Default<T> {
    static_assert(std::is_aggregate_v<T>);

    static bool call(Method m, T) {
        if (!m.args.tags()) {
            DUMP("writing the schema");
            T::write(T::global_schema);
            m.stat = Call::None;
            return true;
        }
        DUMP("got to the module! args=", m.args.size(), " tags=", m.args.tags());
        for (auto const &a : m.args) {
            DUMP(a.name());
        }

        if (auto name = m.args.tag(0).get<Str>()) {
            DUMP(name->data());
            DUMP(std::string_view(*name), name->size());
            auto const &value = T::global_schema[*name];
            m.args.c.tags -= 1;
            DUMP("invoking module member! args=", m.args.size(), " tags=", m.args.tags());
            m.stat = Call::call(value.index(), m.target, value.address(), Mode::Read, m.args);
            return true;
        }
        DUMP("not a method!");
        return false;
    }
};

/******************************************************************************/

}
