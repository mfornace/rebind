#pragma once
#include <ara/Call.h>
#include <ara/Structs.h>

// Implementations for defining functions and methods in C++

namespace ara {

/******************************************************************************/

template <class F>
struct Functor {
    F function;
    Lifetime lifetime;
    // tricky... may have a difference depending if return result is coerced to value...

    // constexpr operator F const &() const noexcept {
    //     DUMP("cast into Functor", &function, reinterpret_cast<void const *>(function), type_name<F>());
    //     return function;
    // }
};

template <int N, class F>
struct DefaultFunctor : Functor<F> {
    static_assert(N > 0, "Functor must have some default arguments");
    using Functor<F>::Functor;
};

/******************************************************************************/

template <class F, class SFINAE=void>
struct FunctorCall;

template <int N, class F, class SFINAE=void>
struct DefaultFunctorCall;

template <class F, class SFINAE=void>
struct MethodCall;

/******************************************************************************/

struct Method {
    Target &target;
    ArgView &args;
    Call::stat &stat;

    template <class S, class F>
    [[nodiscard]] bool operator()(S &&self, F const functor, Lifetime life={}) {
        DUMP("Method::operator()", args.tags(), args.size());
        if (args.tags() == 0) {
            DUMP("found match");
            stat = MethodCall<F>::call(target, life, functor, std::forward<S>(self), args);
            return true;
        }
        return false;
    }

    template <class S, class F>
    [[nodiscard]] bool operator()(S &&self, std::string_view name, F const functor, Lifetime life={}) {
        DUMP("Method::operator()", args.tags(), args.size());
        if (args.tags() == 1) if (auto given = args.tag(0).load<Str>()) {
            std::string_view s(*given);
            DUMP("Checking for method", name, "from", s);
            if (s == name) {
                DUMP("found match");
                stat = MethodCall<F>::call(target, life, functor, std::forward<S>(self), args);
                return true;
            }
        }
        return false;
    }

    template <class Base, class S>
    [[nodiscard]] bool derive(S &&self) {
        static_assert(std::is_reference_v<Base>, "T should be a reference type for Method::derive<T>()");
        static_assert(std::is_convertible_v<S &&, Base>);
        static_assert(!std::is_same_v<unqualified<S>, unqualified<Base>>);
        return Impl<unqualified<Base>>::call(*this, std::forward<S>(self));
    }
};

static_assert(std::is_move_constructible_v<Method>);
static_assert(std::is_copy_constructible_v<Method>);

/******************************************************************************/

#warning "have to allow lifetime extension here"
template <class T>
struct MaybeArg {
    std::optional<T> value;
    // MaybeArg(Ref &ref) :

};

/// Cast element i of v to type T
template <class T>
maybe<T> cast_index(ArgView &v, IndexedType<T> i) {
    DUMP(i.index);
    DUMP(v[i.index].name());
    return v[i.index].load(Type<T>());
}

/******************************************************************************/

/// Invoke a function and arguments, storing output if it doesn't return void
template <class F, class ...Ts>
Call::stat invoke_to(Target& target, F const &f, Ts &&... ts) {
    static_assert(std::is_invocable_v<F, Ts...>, "Function is not invokable with designated arguments");
    using O = simplify_result<std::invoke_result_t<F, Ts...>>;
    using U = unqualified<O>;
    DUMP("invoking function", type_name<F>(), "with output", type_name<O>(), "to constraint", int(target.c.mode));

    if (target.c.mode == Target::None || (std::is_void_v<U> && !target.index())) {
        (void) std::invoke(f, std::forward<Ts>(ts)...);
        return Call::None;
    }

    if constexpr(!std::is_void_v<U>) { // void already handled
        if (target.accepts<U>()) {

            if (std::is_same_v<O, U &> && target.accepts(Target::Write)) {
                target.set_reference(std::invoke(f, std::forward<Ts>(ts)...));
                return Call::Write;
            }

            if (std::is_reference_v<O> && target.accepts(Target::Read)) {
                target.set_reference(static_cast<U const &>(std::invoke(f, std::forward<Ts>(ts)...)));
                return Call::Read;
            }

            if (std::is_same_v<O, U> || std::is_convertible_v<O, U>) {
                if (auto p = target.placement<U>()) {
                    Allocator<U>::invoke_stack(p, f, std::forward<Ts>(ts)...);
                    target.set_index<U>();
                    if (!std::is_same_v<O, U> && !is_dependent<U>) target.set_lifetime({});
                    return Call::Stack;
                }
                if (target.accepts(Target::Heap)) {
                    target.set_heap(Allocator<U>::invoke_heap(f, std::forward<Ts>(ts)...));
                    if (!std::is_same_v<O, U> && !is_dependent<U>) target.set_lifetime({});
                    return Call::Heap;
                }
            }

        } else {
            if constexpr(std::is_reference_v<O>) {
                switch (Ref(std::invoke(f, std::forward<Ts>(ts)...)).load_to(target)) {
                    case Load::Exception: return Call::Exception;
                    case Load::OutOfMemory: return Call::OutOfMemory;
                    case std::is_same_v<O, U &> ? Load::Write : Load::Read:
                        return std::is_same_v<O, U &> ? Call::Write : Call::Read;
                    default: {}
                }
            } else {
                storage_like<U> storage;
                new (&storage) U(std::invoke(f, std::forward<Ts>(ts)...));
                Ref out(Index::of<U>(), Mode::Stack, Pointer::from(&storage));
                switch (out.load_to(target)) {
                    case Load::Exception: return Call::Exception;
                    case Load::OutOfMemory: return Call::OutOfMemory;
                    case Load::Stack: return Call::Stack;
                    case Load::Heap: return Call::Heap;
                    default: {}
                }
            }
        }
    }

#   warning "needs work... for reference returned as value... etc"

    return Call::wrong_return(target, Index::of<U>(), qualifier_of<O>);
}

/******************************************************************************/

template <bool UseCaller, class F, class ...Ts>
Call::stat caller_invoke(Target& out, F const &f, Caller &&c, maybe<Ts> &&...ts) {
    DUMP("casting arguments");
    if (!(ts && ...)) {
        DUMP("casting arguments failed");
        Code i = 0;
        (void) ((ts ? (++i, true) : ((void) Call::wrong_type(out, i, Index::of<unqualified<Ts>>(), qualifier_of<Ts>), false)) && ...);
        return Call::WrongType;
    }
    DUMP("caller_invoke");
    c.enter();
    if constexpr(UseCaller) {
        return invoke_to(out, f, std::move(c), std::forward<Ts>(*ts)...);
    } else {
        return invoke_to(out, f, std::forward<Ts>(*ts)...);
    }
}

/******************************************************************************/

template <class F>
struct Impl<Functor<F>> {
    using Signature = simplify_signature<F>;
    using Return = decltype(first_type(Signature()));
    using UseCaller = decltype(second_is_convertible<Caller>(Signature()));
    using Args = decltype(without_first_types<1 + int(UseCaller::value)>(Signature()));

    /*
     Interface implementation for a function with no optional arguments.
     - Returns WrongNumber if args is not the right length
     */
    static bool call(Method m, Functor<F> const &f) noexcept {
        DUMP("call_to function adapter", type_name<F>(), std::addressof(f), "args=", m.args.size());
        if (m.args.tags())
            return Call::wrong_number(m.target, m.args.tags(), 0);
        if (m.args.size() != Args::size)
            return Call::wrong_number(m.target, m.args.size(), Args::size);

        auto frame = m.args.caller().new_frame(); // make a new unentered frame, must be noexcept
        Caller handle(frame); // make the Caller with a weak reference to frame

        m.target.set_lifetime(f.lifetime);
        m.stat = m.target.make_noexcept([&] {
            return Args::indexed([&](auto ...ts) {
                DUMP("invoking...");
                return caller_invoke<UseCaller::value, F, decltype(*ts)...>(
                    m.target, f.function, std::move(handle), cast_index(m.args, ts)...);
            });
        });
        DUMP("invoked with stat and lifetime", m.stat, f.lifetime.value);
        return Call::was_invoked(m.stat);
        // It is possible that maybe the invoked function's C++ exception may propagated in the future, assuming the caller policies allow this.
        // Therefore, resource destruction must be done via the frame going out of scope.
    }
};

/******************************************************************************/

template <bool UseCaller, class F, class S, class ...Ts>
Call::stat invoke_method(Target& out, F const &f, Caller &&c, S &&self, maybe<Ts> &&...ts) {
    DUMP("casting arguments");
    if (!(ts && ...)) {
        DUMP("casting arguments failed");
        Code i = 0;
        (void) ((ts ? (++i, true) : (Call::wrong_type(out, i, Index::of<unqualified<Ts>>(), qualifier_of<Ts>), false)) && ...);
        return Call::WrongType;
    }
    DUMP("invoke_method");
    c.enter();
    if constexpr(UseCaller) {
        return invoke_to(out, f, std::forward<S>(self), std::move(c), std::forward<Ts>(*ts)...);
    } else {
        return invoke_to(out, f, std::forward<S>(self), std::forward<Ts>(*ts)...);
    }
}

template <class F, class SFINAE>
struct MethodCall {
    using Signature = simplify_signature<F>;
    using Return = decltype(first_type(Signature()));
    using UseCaller = decltype(third_is_convertible<Caller>(Signature()));
    using Args = decltype(without_first_types<2 + int(UseCaller::value)>(Signature()));

    /*
     Interface implementation for a function with no optional arguments.
     - Returns WrongNumber if args is not the right length
     */
    template <class S>
    static Call::stat call(Target& out, Lifetime life, F const &f, S &&self, ArgView &args) noexcept {
        DUMP("call_to MethodCall<", type_name<F>(), ">:", std::addressof(f), args.size(), life.value);

        if (args.size() != Args::size)
            return Call::wrong_number(out, args.size(), Args::size);

        auto frame = args.caller().new_frame(); // make a new unentered frame, must be noexcept
        Caller handle(frame); // make the Caller with a weak reference to frame

        out.set_lifetime(life);
        return out.make_noexcept([&] {
            return Args::indexed([&](auto ...ts) {
                DUMP("invoking...");
                return invoke_method<UseCaller::value, F, S &&, decltype(*ts)...>(
                    out, f, std::move(handle), std::forward<S>(self), cast_index(args, ts)...);
            });
        });
        // It is planned to be allowed that the invoked function's C++ exception may propagated in the future, assuming the caller policies allow this.
        // Therefore, resource destruction must be done via the frame going out of scope.
    }
};

/******************************************************************************/

template <int N=-1, class F>
auto make_functor(F f, Lifetime const lifetime={}) {
    // First we apply lossless simplifications.
    // i.e. a lambda with specified arguments and no defaults is converted to a function pointer.
    // This makes the type more readable and might reduce the compile time a bit.
    auto simplified = SimplifyFunction<F, N>()(std::move(f));
    using S = decltype(simplified);

    // Now get the number of optional arguments
    constexpr int n = N == -1 ? 0 : simplify_signature<S>::size - 1 - N;
    using Out = std::conditional_t<n == 0, Functor<S>, DefaultFunctor<n, S>>;

    // Return the callable object holding the functor
    static_assert(is_implementable<Out>);
    return Out{std::move(simplified), lifetime};
}


// N is the number of trailing optional arguments
template <int N, class F>
struct Impl<DefaultFunctor<N, F>> {
    using Signature = simplify_signature<F>;
    using Return = decltype(first_type(Signature()));
    using UsesCaller = decltype(second_is_convertible<Caller>(Signature()));
    using Args = decltype(without_first_types<1 + int(UsesCaller::value)>(Signature()));

//     template <class P, class Out>
//     static bool call_one(P, F const &f, Out &out, Caller &c, Scope &s, ArgView const &args) {
//         return P::indexed([&](auto ...ts) {
//             caller_invoke(out, UsesCaller(), f, std::move(c), cast_index(args, s, simplify_argument(ts))...);
//             return true;
//         }d/     }

//     template <class Out, std::size_t ...Is>
//     static bool call_indexed(F const &f, Out &out, Caller &c, ArgView const &args, Scope &s, std::index_sequence<Is...>) {
//         constexpr std::size_t const M = Args::size - 1; // number of total arguments minus 1
//         // check the number of arguments given and call with the under-specified arguments
//         return ((args.size() == M - Is ? call_one(Args::template slice<0, M - Is>(), f, out, c, s, args) : false) || ...);
//     }

//     template <class Out>
//     static bool impl(F const &f, Out &out, Caller &c, ArgView const &args) {
//         auto frame = c();
//         Caller handle(frame);
//         Scope s(handle);
//         if (args.size() == Args::size) { // handle fully specified arguments
//             return Args::indexed([&](auto ...ts) {
//                 caller_invoke(out, UsesCaller(), f,
//                     std::move(handle), cast_index(args, s, simplify_argument(ts))...);
//                 return true;
//             }d/         } else if (args.size() < Args::size - N) {
//             throw WrongNumber(Args::size - N, args.size());
//         } else if (args.size() > Args::size) {
//             throw WrongNumber(Args::size, args.size()); // try under-specified arguments
//         } else {
//             return call_indexed(f, out, handle, args, s, std::make_index_sequence<N>());
//         }
//     }

//     static bool call(void const *self, void *out, Caller &&c, ArgView args, Flag flag) {
//         auto const &f = *static_cast<F const *>(self);

//         if (flag == Flag::ref) {
//             if constexpr (std::is_reference_v<Return>) return impl(f, *static_cast<Ref *>(out), c, args);
//             throw std::runtime_error("Requested reference from a function returning a value");
//         }

//         return impl(f, *static_cast<Value *>(out), c, args);
//     }
};

/******************************************************************************/

}