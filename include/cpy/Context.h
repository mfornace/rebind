#pragma once
#include "Approx.h"
#include "Value.h"
#include "Signature.h"
#include "Glue.h"

#include <functional>
#include <atomic>

namespace cpy {

/******************************************************************************/

using Event = std::uint_fast32_t;

static constexpr Event Failure = 0;
static constexpr Event Success = 1;
static constexpr Event Exception = 2;
static constexpr Event Timing = 3;

using Scopes = std::vector<std::string>;

/******************************************************************************/

double current_time() noexcept;

/******************************************************************************/

using Callback = std::function<bool(Event, Scopes const &, Logs &&)>;

using Counter = std::atomic<std::size_t>;

/******************************************************************************/

struct Context {
    std::vector<Callback> callbacks; // or could be vector of callbacks for each type.
    Scopes scopes;
    Logs logs;
    std::vector<Counter> *counters = nullptr;
    void *metadata; // could be transitioned to std::any but right now pointer is OK

    Context();

    Context(Scopes s, std::vector<Callback> h, std::vector<Counter> *c=nullptr, void *m=nullptr);

    /// Subsection
    template <class F>
    auto operator()(std::string name, F &&functor) {
        Context ctx(scopes, callbacks, counters);
        ctx.scopes.push_back(std::move(name));
        return static_cast<F &&>(functor)(ctx);
    }

    /**************************************************************************/

    void info(std::string s) {logs.emplace_back(KeyPair{{}, std::move(s)});}

    void info(char const *s) {logs.emplace_back(KeyPair{{}, std::string_view(s)});}

    template <class T>
    void info(T &&t) {AddKeyPairs<std::decay_t<T>>()(logs, static_cast<T &&>(t));}

    template <class K, class V>
    void info(K &&k, V &&v) {
        logs.emplace_back(KeyPair{static_cast<K &&>(k), make_value(static_cast<V &&>(v))});
    }

    /**************************************************************************/

    template <class ...Ts>
    void handle(Event e, Ts &&...ts) {
        if (e < callbacks.size() && callbacks[e]) {
            (void) std::initializer_list<bool>{(info(static_cast<Ts &&>(ts)), false)...};
            callbacks[e](e, scopes, std::move(logs));
        }
        if (counters && e < counters->size())
            (*counters)[e].fetch_add(1u, std::memory_order_relaxed);
        logs.clear();
    }

    template <class F, class ...Args>
    auto time(std::size_t n, F &&f, Args &&...args) {
        auto start = current_time();
        std::invoke(static_cast<F &&>(f), static_cast<Args &&>(args)...);
        auto elapsed = current_time() - start;
        handle(Timing, glue("value", elapsed));
        return elapsed;
    }

    template <class Bool=bool, class ...Ts>
    bool require(Bool const &ok, Ts &&...ts) {
        bool b = static_cast<bool>(unglue(ok));
        handle(b ? Success : Failure, static_cast<Ts &&>(ts)..., glue("value", ok));
        return b;
    }

    /******************************************************************************/

    template <class X, class Y, class ...Ts>
    bool equal(X const &x, Y const &y, Ts &&...ts) {
        return require(unglue(x) == unglue(y), comparison_glue(x, y, "=="), static_cast<Ts &&>(ts)...);
    }

    template <class X, class Y, class ...Ts>
    bool not_equal(X const &x, Y const &y, Ts &&...ts) {
        return require(unglue(x) != unglue(y), comparison_glue(x, y, "!="), static_cast<Ts &&>(ts)...);
    }

    template <class X, class Y, class ...Ts>
    bool less(X const &x, Y const &y, Ts &&...ts) {
        return require(unglue(x) < unglue(y), comparison_glue(x, y, "<"), static_cast<Ts &&>(ts)...);
    }

    template <class X, class Y, class ...Ts>
    bool greater(X const &x, Y const &y, Ts &&...ts) {
        return require(unglue(x) > unglue(y), comparison_glue(x, y, ">"), static_cast<Ts &&>(ts)...);
    }

    template <class X, class Y, class ...Ts>
    bool less_eq(X const &x, Y const &y, Ts &&...ts) {
        return require(unglue(x) <= unglue(y), comparison_glue(x, y, "<="), static_cast<Ts &&>(ts)...);
    }

    template <class X, class Y, class ...Ts>
    bool greater_eq(X const &x, Y const &y, Ts &&...ts) {
        return require(unglue(x) >= unglue(y), comparison_glue(x, y, ">="), static_cast<Ts &&>(ts)...);
    }

    template <class X, class Y, class T, class ...Ts>
    bool within(X const &x, Y const &y, T const &tol, Ts &&...ts) {
        auto const a = x - y;
        auto const b = y - x;
        bool ok = (a < b) ? static_cast<bool>(b < tol) : static_cast<bool>(a < tol);
        return require(ok, ComparisonGlue<X const &, Y const &>{x, y, "~~"}, glue("tolerance", tol), static_cast<Ts &&>(ts)...);
    }

    template <class X, class Y, class ...Args>
    bool near(X const &x, Y const &y, Args &&...args) {
        bool ok = ApproxEquals<typename ApproxType<X, Y>::type>()(unglue(x), unglue(y));
        return require(ok, ComparisonGlue<X const &, Y const &>{x, y, "~~"}, static_cast<Args &&>(args)...);
    }

    template <class Exception, class F, class ...Args>
    bool throws_as(F &&f, Args &&...args) {
        try {
            std::invoke(static_cast<F &&>(f), static_cast<Args &&>(args)...);
            return require(false);
        } catch (Exception const &) {return require(true);}
    }

    template <class F, class ...Args>
    bool no_throw(F &&f, Args &&...args) {
        try {
            std::invoke(static_cast<F &&>(f), static_cast<Args &&>(args)...);
            return require(true);
        } catch (...) {return require(false);}
    }
};

/******************************************************************************/

}
