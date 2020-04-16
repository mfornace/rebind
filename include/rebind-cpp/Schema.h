#pragma once
#include <rebind/Function.h>
#include "Value.h"

#include <set>
#include <map>

namespace rebind {

/******************************************************************************/

struct RawSchema {
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
};

/******************************************************************************/

struct Schema {
    RawSchema &raw;

    // Emplace a key and value.
    // If "overwrite" is false and the key already exists, std::out_of_range is thrown.
    template <class T>
    T & object(std::string s, T value, bool overwrite=false) {
        return raw.emplace(std::move(s), overwrite).emplace(std::move(value));
    }

    // Emplace a key and function object.
    // The function object which is emplaced is likely not the same type as the input functor.
    // N may be given as the number of mandatory arguments.
    template <int N=-1, class F>
    void function(std::string name, F functor) {
        raw.emplace(std::move(name)).emplace(make_function<N>(std::move(functor)));
    }

    Value const * operator[](std::string_view s) const {
        if (auto it = raw.contents.find(s); it != raw.contents.end()) {
            DUMP("contains value ", s);
            DUMP("contains value ", it->second.name());
            return &it->second;
        }
        return nullptr;
    }
};

/******************************************************************************/

RawSchema & global_schema() noexcept;

/******************************************************************************/

}