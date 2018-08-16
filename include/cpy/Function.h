#pragma once
#include "Signature.h"
#include "Value.h"

#include <typeindex>
#include <iostream>

namespace cpy {

/******************************************************************************/

static char const *cast_bug_message = "FromValue().check() returned false but FromValue()() was still called";

/// Default behavior for casting a variant to a desired argument type
template <class T, class=void>
struct FromValue {
    // Return true if type T can be cast from type U
    template <class U>
    constexpr bool check(U const &) const {
        return std::is_convertible_v<U &&, T> ||
            (std::is_same_v<T, std::monostate> && std::is_default_constructible_v<T>);
    }
    // Return casted type T from type U
    template <class U>
    T operator()(U &&u) const {
        if constexpr(std::is_convertible_v<U &&, T>) return static_cast<T>(static_cast<U &&>(u));
        else if constexpr(std::is_default_constructible_v<T>) return T(); // only hit if U == std::monostate
        else throw std::logic_error(cast_bug_message); // never get here
    }

    bool check(Any const &u) const {
        std::cout << "check" << bool(std::any_cast<no_qualifier<T>>(&u)) << std::endl;
        return std::any_cast<no_qualifier<T>>(&u);}

    T operator()(Any &&u) const {
        return static_cast<T>(std::any_cast<T>(u));
    }
    T operator()(Any const &u) const {
        throw std::logic_error("shouldn't be used");
    }
};

template <class T>
struct FromValue<Vector<T>> {
    template <class U>
    bool check(U const &) const {return false;}

    bool check(Vector<Value> const &u) const {
        return true;
        // std::cout << "check" << bool(std::any_cast<no_qualifier<T>>(&u)) << std::endl;
        // return std::any_cast<no_qualifier<T>>(&u);
    }

    Vector<T> operator()(Vector<Value> &&u) const {
        Vector<T> out;
        for (auto &x : u) {
            std::visit([&](auto &x) {out.emplace_back(FromValue<T>()(std::move(x)));}, x.var);
        }
        return out;
    }

    template <class U>
    Vector<T> operator()(U const &) const {
        throw std::logic_error("shouldn't be used");
    }
};

/******************************************************************************/

/// Invoke a function and arguments, storing output in a Value if it doesn't return void
template <class F, class ...Ts>
Value value_invoke(F &&f, Ts &&... ts) {
    if constexpr(std::is_same_v<void, std::invoke_result_t<F, Ts...>>) {
        std::invoke(static_cast<F &&>(f), static_cast<Ts &&>(ts)...);
        return {};
    } else {
        return make_value(std::invoke(static_cast<F &&>(f), static_cast<Ts &&>(ts)...));
    }
}

/******************************************************************************/

/// Cast element i of v to type T
template <class T>
T cast_index(ArgPack &v, IndexedType<T> i, unsigned int offset) {
    return std::visit(FromValue<T>(), std::move(v[i.index - offset].var));
}

/// Check that element i of v can be cast to type T
template <class T>
bool check_cast_index(ArgPack &v, IndexedType<T> i, unsigned int offset) {
    return std::visit([](auto const &x) {return FromValue<T>().check(x);}, v[i.index - offset].var);
}

/******************************************************************************/

template <class T>
struct NoMutable {
    static_assert(
        !std::is_lvalue_reference_v<T> ||
        std::is_const_v<std::remove_reference_t<T>>,
        "Mutable lvalue references not allowed in function signature"
    );
};

// Basic wrapper to make C++ functor into a type erased std::function
// the C++ functor must be callable with (T), (const &) or (&&) parameters
// we need to take the any class by reference...
// if args contains an Any
// the function may move the Any if it takes Value
// or the function may leave the Any alone if it takes const &
template <class F>
struct FunctionAdaptor {
    F function;

    /// Run C++ functor; logs non-ClientError and rethrows all exceptions
    Value operator()(BaseContext const &, ArgPack &args) const {
        Signature<F>::apply([](auto return_type, auto ...ts) {
            (NoMutable<decltype(*ts)>(), ...);
        });
        return Signature<F>::apply([&](auto return_type, auto ...ts) {
            if (args.size() != sizeof...(ts))
                throw std::invalid_argument("cpy: wrong number of arguments");
            if ((... && check_cast_index(args, ts, 1)))
                return value_invoke(function, cast_index(args, ts, 1)...);
            throw std::invalid_argument("cpy: wrong argument types");
        });
    }
};

// struct OverloadAdaptor {
//     Vector<Function> overloads;

//     Value operator()(BaseContext &ct, ArgPack &args) const {
//         if (overloads.empty())
//             throw std::invalid_argument("cpy: no overloads registered");
//         for (auto it = overloads.begin(); it != std::prev(overloads.end()); ++it) {
//             try {return (*it)(ct, args);}
//             catch (std::invalid_argument const &) {}
//         }
//         return overloads.back()(ct, args);
//     }
// };

template <class F>
struct FunctionAdaptor2 {
    F function;

    /// Run C++ functor; logs non-ClientError and rethrows all exceptions
    Value operator()(BaseContext &ct, ArgPack &args) const {
        return Signature<F>::apply([&](auto return_type, auto context_type, auto ...ts) {
            if (args.size() != sizeof...(ts))
                throw std::invalid_argument("cpy: wrong number of arguments");
            if ((... && check_cast_index(args, ts, 1)))
                return value_invoke(function, ct, cast_index(args, ts, 1)...);
            throw std::invalid_argument("cpy: wrong argument types");
        });
    }
};

template <class F>
Function make_function(F f) {return FunctionAdaptor<F>{std::move(f)};}

template <class F>
Function make_function2(F f) {return FunctionAdaptor2<F>{std::move(f)};}

/******************************************************************************/

struct Document {
    std::vector<std::pair<std::string, Value>> values;
    std::vector<std::pair<std::type_index, std::string>> types;
    std::vector<std::tuple<std::string, std::string, Value>> methods;

    template <class F>
    void def(char const *s, F &&f) {
        values.emplace_back(s, make_function(static_cast<F &&>(f)));
    }

    void type(char const *s, std::type_index t) {
        types.emplace_back(t, s);
    }

    template <class T>
    void type(char const *s) {type(s, std::type_index(typeid(T)));}

    void method(char const *s, char const *n, Value v) {
        methods.emplace_back(s, n, std::move(v));
    }
};

Document & document() noexcept;

}