#pragma once
#include "Context.h"

namespace cpy {

/******************************************************************************/

/// No need to inherit from std::exception since the use case is so limited, I think.
struct Skip {
    std::string_view message;
    Skip() noexcept : message("Test skipped") {}
    explicit Skip(std::string_view const &m) noexcept : message(m) {}
};

/******************************************************************************/

static char const *cast_bug_message = "CastVariant().check() returned false but CastVariant()() was still called";

/// Default behavior for casting a variant to a desired argument type
template <class T, class=void>
struct CastVariant {
    // Return true if type T can be cast from type U
    template <class U>
    constexpr bool check(U const &) const {
        return std::is_convertible_v<U &&, T> ||
            (std::is_same_v<T, std::monostate> && std::is_default_constructible_v<T>);
    }
    // Return casted type T from type U
    template <class U>
    T operator()(U &u) const {
        if constexpr(std::is_convertible_v<U &&, T>) return static_cast<T>(std::move(u));
        else if constexpr(std::is_default_constructible_v<T>) return T(); // only hit if U == std::monostate
        else throw std::logic_error(cast_bug_message); // never get here
    }
};

/// Cast element i of v to type T
template <class T>
T cast_index(ArgPack &v, IndexedType<T> i) {
    return std::visit(CastVariant<T>(), v[i.index - 2u].var);
}

/// Check that element i of v can be cast to type T
template <class T>
bool check_cast_index(ArgPack &v, IndexedType<T> i) {
    return std::visit([](auto const &x) {return CastVariant<T>().check(x);}, v[i.index - 2u].var);
}

/******************************************************************************/

/// TestSignature assumes signature void(Context) if none can be deduced
template <class F, class=void>
struct TestSignature : Pack<void, Context> {
    static_assert(std::is_invocable<F, Context>(),
        "Functor is not callable with implicit signature void(Context). "
        "Specialize cpy::Signature<T> for your function or use a functor with a "
        "deducable (i.e. non-templated) signature");
};

/// Otherwise TestSignature assumes the deduced Signature
template <class F>
struct TestSignature<F, std::void_t<typename Signature<F>::return_type>> : Signature<F> {};

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

/// Basic wrapper to make C++ functor into a type erased std::function
template <class F>
struct TestAdaptor {
    F function;

    /// Run C++ functor; logs non-HandlerException and rethrows all exceptions
    Value operator()(Context &ctx, ArgPack args) {
        try {
            return TestSignature<F>::apply([&](auto return_type, auto context_type, auto ...ts) {
                static_assert(std::is_convertible_v<Context, decltype(*context_type)>,
                              "First argument in signature should be of type Context");
                if (args.size() != sizeof...(ts))
                    throw std::invalid_argument("cpy: wrong number of arguments");
                if ((... && check_cast_index(args, ts)))
                    return value_invoke(function, Context(ctx), cast_index(args, ts)...);
                throw std::invalid_argument("cpy: wrong argument types");
            });
        } catch (Skip const &e) {
            ctx.info("value", e.message);
            ctx.handle(Skipped);
            throw e;
        } catch (HandlerError const &e) {
            throw e;
        } catch (std::exception const &e) {
            ctx.info("value", e.what());
            ctx.handle(Exception);
            throw e;
        } catch (...) {
            ctx.handle(Exception);
            std::rethrow_exception(std::current_exception());
        }
    }
};

/******************************************************************************/

/// Basic wrapper to make a fixed Value into a std::function
struct ValueAdaptor {
    Value value;
    Value operator()(Context &, ArgPack const &) const {return value;}
};

/******************************************************************************/

struct TestCaseComment {
    std::string comment;
    FileLine location;
    TestCaseComment() = default;

    template <class T>
    TestCaseComment(Comment<T> c)
        : comment(std::move(c.comment)), location(std::move(c.location)) {}
};

/// A named, commented, possibly parametrized unit test case
struct TestCase {
    std::string name;
    TestCaseComment comment;
    std::function<Value(Context &, ArgPack)> function;
    std::vector<ArgPack> parameters;
};

void register_test(TestCase t);

template <class F>
void register_test(std::string name, TestCaseComment c, F const &f, std::vector<ArgPack> v={}) {
    if (TestSignature<F>::size::value <= 2 && v.empty()) v.emplace_back();
    register_test(TestCase{std::move(name), std::move(c), TestAdaptor<F>{f}, std::move(v)});
}

/******************************************************************************/

template <class F>
struct UnitTest {
    std::string name;
    F function;
};

template <class F>
UnitTest<F> unit_test(std::string name, F const &f, std::vector<ArgPack> v={}) {
    register_test(name, TestCaseComment(), f, std::move(v));
    return {std::move(name), f};
}

template <class F>
UnitTest<F> unit_test(std::string name, TestCaseComment comment, F const &f, std::vector<ArgPack> v={}) {
    register_test(name, std::move(comment), f, std::move(v));
    return {std::move(name), f};
}

/******************************************************************************/

/// Same as unit_test() but just returns a meaningless bool instead of a functor object
template <class F>
bool anonymous_test(std::string name, TestCaseComment comment, F &&function, std::vector<ArgPack> v={}) {
    register_test(std::move(name), std::move(comment), static_cast<F &&>(function), std::move(v));
    return bool();
}

/// Helper class for UNIT_TEST() macro, overloads the = operator to make it a bit prettier.
struct AnonymousClosure {
    std::string name;
    TestCaseComment comment;
    std::vector<ArgPack> args;

    AnonymousClosure(std::string s, TestCaseComment c, std::vector<ArgPack> v={})
        : name(std::move(s)), comment(std::move(c)), args(std::move(v)) {}

    template <class F>
    constexpr bool operator=(F const &f) && {
        return anonymous_test(std::move(name), std::move(comment), f, std::move(args));
    }
};

/******************************************************************************/

/// Call a registered unit test with type-erased arguments and output
/// Throw std::runtime_error if test not found or test throws exception
Value call(std::string_view s, Context c, ArgPack pack);

/// Call a registered unit test with non-type-erased arguments and output
template <class ...Ts>
Value call(std::string_view s, Context c, Ts &&...ts) {
    return call(s, std::move(c), ArgPack{make_value(static_cast<Ts &&>(ts))...});
}

/// Get a stored value from its unit test name
/// Throw std::runtime_error if test not found or test does not hold a Value
Value get_value(std::string_view s);

/******************************************************************************/

}
