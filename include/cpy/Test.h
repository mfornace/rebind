#pragma once
#include "Context.h"

namespace cpy {

/******************************************************************************/

template <class T, class=void>
struct CastVariant {
    template <class U>
    bool check(U const &u) const {
        return std::is_convertible_v<U &&, T> || std::is_same_v<T, std::monostate>;
    }

    template <class U, std::enable_if_t<(!std::is_convertible_v<U &&, T>), int> = 0>
    T operator()(U &u) const {return T();}

    template <class U, std::enable_if_t<(std::is_convertible_v<U &&, T>), int> = 0>
    T operator()(U &u) const {return static_cast<T>(std::move(u));}
};

/// Cast element i of v to type T
template <class T>
T cast_index(ArgPack &v, IndexedType<T> i) {
    return std::visit(CastVariant<T>(), v[i.index - 2].var);
}

/// Check that element i of v can be cast to type T
template <class T>
bool check_cast_index(ArgPack &v, IndexedType<T> i) {
    return std::visit([](auto const &x) {return CastVariant<T>().check(x);}, v[i.index - 2].var);
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

template <class F>
struct TestSignature<F, std::void_t<typename Signature<F>::return_type>> : Signature<F> {};

template <class F, class ...Ts, std::enable_if_t<(!std::is_same_v<void, std::invoke_result_t<F, Ts...>>), int> = 0>
void invoke(Value &output, F &&f, Ts &&...ts) {output = make_value(std::invoke(static_cast<F &&>(f), static_cast<Ts &&>(ts)...));}

template <class F, class ...Ts, std::enable_if_t<(std::is_same_v<void, std::invoke_result_t<F, Ts...>>), int> = 0>
void invoke(Value &, F &&f, Ts &&...ts) {std::invoke(static_cast<F &&>(f), static_cast<Ts &&>(ts)...);}

template <class F>
struct TestAdaptor {
    F function;

    /// Catches any exceptions; returns whether the test could be begun.
    bool operator()(Value &output, Context ctx, ArgPack args) noexcept {
        try {
            return TestSignature<F>::apply([&](auto return_type, auto context_type, auto ...ts) {
                static_assert(std::is_convertible<Context, decltype(*context_type)>(),
                              "First argument in signature should be of type Context");
                bool ok = args.size() == sizeof...(ts);
                (void) std::initializer_list<bool>{(ok = ok && check_cast_index(args, ts))...};
                if (ok) invoke(output, function, Context(ctx), cast_index(args, ts)...);
                return ok;
            });
        } catch (std::exception const &e) {
            ctx.info("value", e.what());
        } catch (...) {}
        ctx.handle(Exception);
        return true;
    }
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
    std::function<bool(Value &, Context, ArgPack)> function;
    std::vector<ArgPack> parameters;
};

/// A vector of TestCase
struct Suite {
    std::vector<TestCase> cases;

    template <class F>
    void operator()(std::string name, TestCaseComment c, F const &f, std::vector<ArgPack> v={}) {
        cases.emplace_back(TestCase{std::move(name), std::move(c), TestAdaptor<F>{f}, std::move(v)});
    }
};

Suite & suite();

/******************************************************************************/

template <class F>
struct UnitTest {
    std::string name;
    F function;
};

template <class F>
UnitTest<F> unit_test(std::string name, F const &f, std::vector<ArgPack> v={}) {
    suite()(name, TestCaseComment(), f);
    return {std::move(name), f};
}

template <class F>
UnitTest<F> unit_test(std::string name, TestCaseComment comment, F const &f, std::vector<ArgPack> v={}) {
    suite()(name, std::move(comment), f);
    return {std::move(name), f};
}

/******************************************************************************/

template <class F>
bool anonymous_test(std::string name, F &&function) {
    suite()(std::move(name), TestCaseComment(), static_cast<F &&>(function));
    return true;
}

template <class F>
bool anonymous_test(std::string name, TestCaseComment comment, F &&function) {
    suite()(std::move(name), std::move(comment), static_cast<F &&>(function));
    return true;
}

struct AnonymousClosure {
    std::string const &name;
    TestCaseComment comment;

    AnonymousClosure(std::string const &s, TestCaseComment c) : name(s), comment(std::move(c)) {}

    template <class F>
    constexpr bool operator=(F const &f) {return anonymous_test(name, comment, f);}
};

/******************************************************************************/

}
