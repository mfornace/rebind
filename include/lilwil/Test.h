#pragma once
#include "Context.h"
#include <cpy/Signature.h>

namespace lilwil {
using cpy::Pack;
using cpy::Signature;

/******************************************************************************/

/// No need to inherit from std::exception since the use case is so limited, I think.
struct Skip {
    std::string_view message;
    Skip() noexcept : message("Test skipped") {}
    explicit Skip(std::string_view const &m) noexcept : message(m) {}
};

/******************************************************************************/

/// TestSignature assumes signature void(Context) if none can be deduced
template <class X, class F, class=void>
struct TestSignature : Pack<void, Context<X>> {
    static_assert(std::is_invocable<F, Context<X>>(),
        "Functor is not callable with implicit signature void(Context). "
        "Specialize cpy::Signature<T> for your function or use a functor with a "
        "deducable (i.e. non-template, no auto) signature");
};

/// Otherwise TestSignature assumes the deduced Signature
template <class X, class F>
struct TestSignature<X, F, std::void_t<typename Signature<F>::return_type>> : Signature<F> {};

/******************************************************************************/

template <class F, class X, class ...Ts>
X context_invoke(std::true_type, F const &f, Context<X> &c, Ts &&...ts) {
    return value_invoke(f, c, static_cast<Ts &&>(ts)...);
}

template <class F, class X, class ...Ts>
X context_invoke(std::false_type, F const &f, Context<X> &c, Ts &&...ts) {
    return value_invoke(f, static_cast<Ts &&>(ts)...);
}

/// Basic wrapper to make C++ functor into a type erased std::function
template <class X, class F>
struct TestAdaptor {
     F function;
//     using Ctx = decltype(has_head<Context<X>>(TestSignature<X, F>()));
//     using Sig = decltype(skip_head<1 + int(Ctx::value)>(TestSignature<X, F>()));
//     /// Run C++ functor; logs non-ClientError and rethrows all exceptions
    X operator()(Context<X> &ct, Vector<X> args);// {
//         if (Debug) std::cout << typeid(Sig).name() << std::endl;
//         if (Debug) std::cout << args.size() << std::endl;
//         if (Debug) std::cout << Ctx::value << std::endl;
//         try {
//             if (args.size() != Sig::size)
//                 throw WrongNumber(Sig::size, args.size());
//             return Sig::indexed([&](auto ...ts) {
//                 Dispatch msg("mismatched test argument");
//                 if constexpr(Ctx::value) return value_invoke(function, ct, cast_index(args, msg, ts)...);
//                 else return value_invoke(function, cast_index(args, msg, ts)...);
//             });
//         } catch (Skip const &e) {
//             ct.info("value", e.message);
//             ct.handle(Skipped);
//             throw;
//         } catch (ClientError const &) {
//             throw;
//         } catch (std::exception const &e) {
//             ct.info("value", e.what());
//             ct.handle(Exception);
//             throw;
//         } catch (...) {
//             ct.handle(Exception);
//             throw;
//         }
//     }
};

/******************************************************************************/

/// Basic wrapper to make a fixed Variable into a std::function
template <class X>
struct ValueAdaptor {
    X value;
    X operator()(Context<X> &, Vector<X> const &) const {return value;}
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
template <class X>
struct TestCase {
    std::string name;
    TestCaseComment comment;
    std::function<X(Context<X> &, Vector<X>)> function;
    Vector<Vector<X>> parameters;
};

template <class X>
void add_test(TestCase<X> t);

template <class X, class F>
void add_test(std::string name, TestCaseComment c, F const &f, Vector<Vector<X>> v={}) {
    if (TestSignature<X, F>::size <= 2 && v.empty()) v.emplace_back();
    add_test<X>(TestCase<X>{std::move(name), std::move(c), TestAdaptor<X, F>{f}, std::move(v)});
}

/******************************************************************************/

template <class F>
struct UnitTest {
    std::string name;
    F function;
};

template <class X, class F>
UnitTest<F> unit_test(std::string name, F const &f, Vector<Vector<X>> v={}) {
    add_test(name, TestCaseComment(), f, std::move(v));
    return {std::move(name), f};
}

template <class X, class F>
UnitTest<F> unit_test(std::string name, TestCaseComment comment, F const &f, Vector<Vector<X>> v={}) {
    add_test<X>(name, std::move(comment), f, std::move(v));
    return {std::move(name), f};
}

/******************************************************************************/

/// Same as unit_test() but just returns a meaningless bool instead of a functor object
template <class X, class F>
bool anonymous_test(std::string name, TestCaseComment comment, F &&function, Vector<Vector<X>> v={}) {
    add_test<X>(std::move(name), std::move(comment), static_cast<F &&>(function), std::move(v));
    return bool();
}

/// Helper class for UNIT_TEST() macro, overloads the = operator to make it a bit prettier.
struct AnonymousClosure {
    std::string name;
    TestCaseComment comment;

    AnonymousClosure(std::string s, TestCaseComment c)
        : name(std::move(s)), comment(std::move(c)) {}

    template <class F>
    constexpr bool operator=(F const &f) && {
        using X = cpy::Variable;
        return anonymous_test<X>(std::move(name), std::move(comment), f);
    }
};

/******************************************************************************/

/// Call a registered unit test with type-erased arguments and output
/// Throw std::runtime_error if test not found or test throws exception
template <class X>
X call(std::string_view s, Context<X> c, Vector<X> pack);

/// Call a registered unit test with non-type-erased arguments and output
template <class X, class ...Ts>
X call(std::string_view s, Context<X> c, Ts &&...ts) {
    return call(s, std::move(c), Vector<X>{make_output(static_cast<Ts &&>(ts))...});
}

/// Get a stored value from its unit test name
/// Throw std::runtime_error if test not found or test does not hold a Value
template <class X>
X get_value(std::string_view s);

/******************************************************************************/

}
