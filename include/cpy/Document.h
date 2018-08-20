#pragma once
#include "Function.h"

namespace cpy {

/******************************************************************************/

struct Document {
    std::vector<std::pair<std::string, Output>> values;
    std::vector<std::pair<std::type_index, std::string>> types;
    std::vector<std::tuple<std::string, std::string, Function>> methods;

    template <class O>
    void define(char const *s, O &&o) {
        values.emplace_back(s, make_function(static_cast<O &&>(o)));
    }

    template <class O>
    void recurse(char const *s, O &&o) {
        values.emplace_back(s, make_function(static_cast<O &&>(o)));
    }

    void type(char const *s, std::type_index t) {
        types.emplace_back(t, s);
    }

    template <class T>
    void type(char const *s) {type(s, std::type_index(typeid(T)));}

    void method(char const *s, char const *n, Function v) {
        methods.emplace_back(s, n, std::move(v));
    }
};

Document & document() noexcept;

/******************************************************************************/

}