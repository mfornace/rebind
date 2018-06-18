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

using Handler = std::function<bool(Event, Scopes const &, Logs &&)>;

using Counter = std::atomic<std::size_t>;

/******************************************************************************/

struct Context {
    std::vector<Handler> handlers; // or could be vector of handlers for each type.
    Scopes scopes;
    Logs logs;
    std::vector<Counter> *counters = nullptr;

    Context() = default;

    Context(Scopes const &s, std::vector<Handler> const &h, std::vector<Counter> *c=nullptr)
        : scopes(s), handlers(h), counters(c) {}

    Context(Scopes &&s, std::vector<Handler> &&h, std::vector<Counter> *c=nullptr)
        : scopes(std::move(s)), handlers(std::move(h)), counters(c) {}

    /// Subsection
    template <class F>
    auto operator()(std::string name, F &&functor) {
        Context ctx(scopes, handlers, counters);
        ctx.scopes.push_back(std::move(name));
        return functor(ctx);
    }

    /**************************************************************************/

    void info(std::string s) {logs.emplace_back(KeyPair{{}, std::move(s)});}

    void info(char const *s) {logs.emplace_back(KeyPair{{}, std::string_view(s)});}

    template <class T>
    void info(T &&t) {AddKeyPairs<std::decay_t<T>>()(logs, static_cast<T &&>(t));}

    template <class K, class V>
    void info(K &&k, V &&v) {
        logs.emplace_back(KeyPair{
            static_cast<K &&>(k),
            Valuable<std::decay_t<V>>()(static_cast<V &&>(v))
        });
    }

    /**************************************************************************/

    template <class ...Ts>
    void handle(Event e, Ts &&...ts) {
        if (e < handlers.size() && handlers[e]) {
            (void) std::initializer_list<bool>{(info(static_cast<Ts &&>(ts)), false)...};
            handlers[e](e, scopes, std::move(logs));
        }
        if (counters && e < counters->size())
            (*counters)[e].fetch_add(1u, std::memory_order_relaxed);
        logs.clear();
    }

    template <class Bool=bool, class ...Ts>
    bool require(Bool const &ok, Ts &&...ts) {
        bool b{unglue(ok)};
        handle(b ? Success : Failure, static_cast<Ts &&>(ts)..., glue("value", ok));
        return b;
    }

#define CPY_TMP(NAME, OP) template <class X, class Y, class ...Args> \
    bool NAME(X const &x, Y const &y, Args &&...args) { \
        return require(unglue(x) OP unglue(y), comparison_glue(x, y, #OP), static_cast<Args &&>(args)...); \
    }

    CPY_TMP(require_eq,  ==);
    CPY_TMP(require_ne,  !=);
    CPY_TMP(require_lt,  < );
    CPY_TMP(require_gt,  > );
    CPY_TMP(require_ge,  >=);
    CPY_TMP(require_le,  <=);
    CPY_TMP(require_or,  ||);
    CPY_TMP(require_and, &&);
    CPY_TMP(require_xor, ^ );

#undef CPY_TMP

    template <class X, class Y, std::enable_if_t<!(std::is_integral<X>::value && std::is_integral<Y>::value), int> = 0>
    bool require_near(X const &x, Y const &y) {
        bool ok = ApproxEquals<typename ApproxType<X, Y>::type>()(unglue(x), unglue(y));
        return require(ok, ComparisonGlue<X const &, Y const &>{x, y, "=="});
    }

    template <class Exception, class F, class ...Args>
    bool require_throws(F &&f, Args &&...args) {
        bool ok = false;
        try {f(static_cast<Args &&>(args)...);}
        catch (Exception const &) {ok = true;}
        return require(ok);
    }

    template <class F, class ...Args>
    auto time(std::size_t n, F &&f, Args &&...args) {
        auto start = current_time();
        f(static_cast<Args &&>(args)...);
        auto elapsed = current_time() - start;
        handle(Timing, glue("value", elapsed));
        return elapsed;
    }
};

/******************************************************************************/

}