#pragma once
#include "Context.h"
#include <cpy/Function.h>

namespace cpy {

/******************************************************************************/

/// No need to inherit from std::exception since the use case is so limited, I think.
struct Skip {
    std::string_view message;
    Skip() noexcept : message("Test skipped") {}
    explicit Skip(std::string_view const &m) noexcept : message(m) {}
};

/******************************************************************************/

/// TestSignature assumes signature void(Context) if none can be deduced
template <class F, class=void>
struct TestSignature : Pack<void, Context> {
    static_assert(std::is_invocable<F, Context>(),
        "Functor is not callable with implicit signature void(Context). "
        "Specialize cpy::Signature<T> for your function or use a functor with a "
        "deducable (i.e. non-template, no auto) signature");
};

/// Otherwise TestSignature assumes the deduced Signature
template <class F>
struct TestSignature<F, std::void_t<typename Signature<F>::return_type>> : Signature<F> {};

/******************************************************************************/

template <class F, class ...Ts>
Variable context_invoke(std::true_type, F const &f, Context &c, Ts &&...ts) {
    if (Debug) std::cout << "invoking with context" << std::endl;
    return value_invoke(f, c, static_cast<Ts &&>(ts)...);
}

template <class F, class ...Ts>
Variable context_invoke(std::false_type, F const &f, Context &c, Ts &&...ts) {
    if (Debug) std::cout << "invoking context guard" << std::endl;
    return value_invoke(f, static_cast<Ts &&>(ts)...);
}

/// Basic wrapper to make C++ functor into a type erased std::function
template <class F>
struct TestAdaptor {
    F function;
    using Ctx = decltype(has_head<Context>(TestSignature<F>()));
    using Sig = decltype(skip_head<1 + int(Ctx::value)>(TestSignature<F>()));

    /// Run C++ functor; logs non-ClientError and rethrows all exceptions
    Variable operator()(Context &ct, ArgPack args) {
        if (Debug) std::cout << typeid(Sig).name() << std::endl;
        if (Debug) std::cout << args.size() << std::endl;
        if (Debug) std::cout << Ctx::value << std::endl;
        try {
            if (args.size() != Sig::size)
                throw WrongNumber(Sig::size, args.size());
            return Sig::indexed([&](auto ...ts) {
                Dispatch msg("mismatched test argument");
                if constexpr(Ctx::value) return value_invoke(function, ct, cast_index(args, msg, ts)...);
                else return value_invoke(function, cast_index(args, msg, ts)...);
            });
        } catch (Skip const &e) {
            ct.info("value", e.message);
            ct.handle(Skipped);
            throw;
        } catch (ClientError const &) {
            throw;
        } catch (DispatchError const &) {
            throw;
        } catch (std::exception const &e) {
            ct.info("value", e.what());
            ct.handle(Exception);
            throw;
        } catch (...) {
            ct.handle(Exception);
            throw;
        }
    }
};

/******************************************************************************/

/// Basic wrapper to make a fixed Variable into a std::function
struct ValueAdaptor {
    Variable value;
    Variable operator()(Context &, ArgPack const &) const {return value;}
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
    std::function<Variable(Context &, ArgPack)> function;
    Vector<ArgPack> parameters;
};

void add_test(TestCase t);

template <class F>
void add_test(std::string name, TestCaseComment c, F const &f, Vector<ArgPack> v={}) {
    if (TestSignature<F>::size <= 2 && v.empty()) v.emplace_back();
    add_test(TestCase{std::move(name), std::move(c), TestAdaptor<F>{f}, std::move(v)});
}

/******************************************************************************/

template <class F>
struct UnitTest {
    std::string name;
    F function;
};

template <class F>
UnitTest<F> unit_test(std::string name, F const &f, Vector<ArgPack> v={}) {
    add_test(name, TestCaseComment(), f, std::move(v));
    return {std::move(name), f};
}

template <class F>
UnitTest<F> unit_test(std::string name, TestCaseComment comment, F const &f, Vector<ArgPack> v={}) {
    add_test(name, std::move(comment), f, std::move(v));
    return {std::move(name), f};
}

/******************************************************************************/

/// Same as unit_test() but just returns a meaningless bool instead of a functor object
template <class F>
bool anonymous_test(std::string name, TestCaseComment comment, F &&function, Vector<ArgPack> v={}) {
    add_test(std::move(name), std::move(comment), static_cast<F &&>(function), std::move(v));
    return bool();
}

/// Helper class for UNIT_TEST() macro, overloads the = operator to make it a bit prettier.
struct AnonymousClosure {
    std::string name;
    TestCaseComment comment;
    Vector<ArgPack> args;

    AnonymousClosure(std::string s, TestCaseComment c, Vector<ArgPack> v={})
        : name(std::move(s)), comment(std::move(c)), args(std::move(v)) {}

    template <class F>
    constexpr bool operator=(F const &f) && {
        return anonymous_test(std::move(name), std::move(comment), f, std::move(args));
    }
};

/******************************************************************************/

/// Call a registered unit test with type-erased arguments and output
/// Throw std::runtime_error if test not found or test throws exception
Variable call(std::string_view s, Context c, ArgPack pack);

/// Call a registered unit test with non-type-erased arguments and output
template <class ...Ts>
Variable call(std::string_view s, Context c, Ts &&...ts) {
    return call(s, std::move(c), ArgPack{make_output(static_cast<Ts &&>(ts))...});
}

/// Get a stored value from its unit test name
/// Throw std::runtime_error if test not found or test does not hold a Variable
Variable get_value(std::string_view s);

/******************************************************************************/

}
