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
                throw WrongNumber(sizeof...(ts), args.size());
            if ((... && check_cast_index(args, ts, 1)))
                return value_invoke(function, cast_index(args, ts, 1)...);
            throw WrongTypes(args);
        });
    }
};

template <class F>
struct FunctionAdaptor2 {
    F function;

    /// Run C++ functor; logs non-ClientError and rethrows all exceptions
    Value operator()(BaseContext &ct, ArgPack &args) const {
        Signature<F>::apply([](auto return_type, auto context_type, auto ...ts) {
            (NoMutable<decltype(*ts)>(), ...);
        });
        return Signature<F>::apply([&](auto return_type, auto context_type, auto ...ts) {
            if (args.size() != sizeof...(ts))
                throw WrongNumber(sizeof...(ts), args.size());
            if ((... && check_cast_index(args, ts, 2)))
                return value_invoke(function, ct, cast_index(args, ts, 2)...);
            throw WrongTypes(args);
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

    template <class O>
    void define(char const *s, O &&o) {
        values.emplace_back(s, make_function(static_cast<O &&>(o)));
    }


    template <class O>
    void define2(char const *s, O &&o) {
        values.emplace_back(s, make_function2(static_cast<O &&>(o)));
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

    void method(char const *s, char const *n, Value v) {
        methods.emplace_back(s, n, std::move(v));
    }
};

Document & document() noexcept;










/// std::ostream synchronizer for redirection from multiple threads
struct StreamSync {
    std::ostream &stream;
    std::streambuf *original; // never changed (unless by user)
    std::mutex mutex;
    Vector<std::streambuf *> queue;
};

extern StreamSync cout_sync;
extern StreamSync cerr_sync;

/// RAII acquisition of cout or cerr
struct RedirectStream {
    StreamSync &sync;
    std::streambuf * const buf;

    RedirectStream(StreamSync &s, std::streambuf *b) : sync(s), buf(b) {
        if (!buf) return;
        std::lock_guard<std::mutex> lk(sync.mutex);
        if (sync.queue.empty()) sync.stream.rdbuf(buf); // take over the stream
        else sync.queue.push_back(buf); // or add to queue
    }

    ~RedirectStream() {
        if (!buf) return;
        std::lock_guard<std::mutex> lk(sync.mutex);
        auto it = std::find(sync.queue.begin(), sync.queue.end(), buf);
        if (it != sync.queue.end()) sync.queue.erase(it); // remove from queue
        else if (sync.queue.empty()) sync.stream.rdbuf(sync.original); // set to original
        else { // let next waiting stream take over
            sync.stream.rdbuf(sync.queue[0]);
            sync.queue.erase(sync.queue.begin());
        }
    }
};












}
