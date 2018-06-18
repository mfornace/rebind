#pragma once
#include "Context.h"

namespace cpy {

/******************************************************************************/

template <class T, class=void>
struct CastVariant {
    template <class U>
    bool check(U const &u) const {
        return std::is_convertible<U &&, T>::value || std::is_same<T, std::monostate>::value;
    }

    template <class U, std::enable_if_t<(!std::is_convertible<U &&, T>::value), int> = 0>
    T operator()(U &u) const {return T();}

    template <class U, std::enable_if_t<(std::is_convertible<U &&, T>::value), int> = 0>
    T operator()(U &u) const {return static_cast<T>(std::move(u));}
};

template <std::size_t I, class T>
T cast_index(ArgPack &v, IndexedType<I, T>) {
    return std::visit(CastVariant<T>(), v[I - 2].var);
}

template <std::size_t I, class T>
bool check_cast_index(ArgPack &v, IndexedType<I, T>) {
    return std::visit([](auto const &x) {return CastVariant<T>().check(x);}, v[I - 2].var);
}

/******************************************************************************/

template <class F, class=void>
struct TestSignature : Pack<void, Context> {
    static_assert(std::is_invocable<F, Context>(),
        "Functor is not callable with implicit signature void(Context). "
        "Specialize Signature<T> for your function or use a functor with a "
        "deducable (i.e. non-templated) signature");
};

template <class F>
struct TestSignature<F, std::void_t<typename Signature<F>::return_type>> : Signature<F> {};

template <class F>
struct TestAdaptor {
    F function;

    bool operator()(Context ctx, ArgPack args) noexcept {
        try {
            return TestSignature<F>::apply([&](auto return_type, auto context_type, auto ...ts) {
                static_assert(std::is_convertible<Context, decltype(*context_type)>(),
                              "First argument in signature should be of type Context");
                bool ok = args.size() == sizeof...(ts);
                (void) std::initializer_list<bool>{(ok = ok && check_cast_index(args, ts))...};
                if (ok) std::invoke(function, Context(ctx), cast_index(args, ts)...);
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

struct TestCase {
    std::string name;
    TestCaseComment comment;
    std::function<bool(Context, ArgPack)> function;
};

struct Suite {
    std::vector<TestCase> cases;

    template <class F>
    void operator()(std::string name, TestCaseComment c, F const &f) {
        cases.emplace_back(TestCase{std::move(name), std::move(c), TestAdaptor<F>{f}});
    }
};

Suite & default_suite();

/******************************************************************************/

template <class String, class F>
struct UnitTest {
    String name;
    F function;
};

template <class N, class F>
UnitTest<N, F> unit_test(N name, F const &f) {
    default_suite()(name, TestCaseComment(), f);
    return {std::move(name), f};
}

template <class N, class F>
UnitTest<N, F> unit_test(N name, TestCaseComment comment, F const &f) {
    default_suite()(name, std::move(comment), f);
    return {std::move(name), f};
}

/******************************************************************************/

template <class N, class F>
bool anonymous_test(N &&name, F &&function) {
    default_suite()(static_cast<N &&>(name), TestCaseComment(), static_cast<F &&>(function));
    return true;
}

template <class N, class F>
bool anonymous_test(N &&name, TestCaseComment comment, F &&function) {
    default_suite()(static_cast<N &&>(name), std::move(comment), static_cast<F &&>(function));
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
