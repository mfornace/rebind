#pragma once
#include <ara/Call.h>
#include <ara/Structs.h>

// Implementations for defining functions and methods in C++

namespace ara {

/******************************************************************************/

// This is the actual callable object plus a lifetime annotation
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

// This is some future thing for default arguments
template <int N, class F>
struct DefaultFunctor : Functor<F> {
    static_assert(N > 0, "Functor must have some default arguments");
    using Functor<F>::Functor;
};

/******************************************************************************/

// template <class F, class SFINAE=void>
// struct FunctorCall;

// template <int N, class F, class SFINAE=void>
// struct DefaultFunctorCall;

template <class F, class SFINAE=void>
struct ApplyMethod;

/******************************************************************************/

// This is the function context with arguments, output, output stat
struct Body {
    Target &target;
    ArgView &args;
    Method::stat &stat;

    template <class S, class F>
    [[nodiscard]] bool operator()(S &&self, F const functor, Lifetime life={}) {
        DUMP("Body::operator()", args.tags(), args.size());
        if (args.tags() == 0) {
            DUMP("found match");
            stat = ApplyMethod<F>::invoke(target, life, functor, std::forward<S>(self), args);
            return true;
        }
        return false;
    }

    template <class S, class F>
    [[nodiscard]] bool operator()(S &&self, std::string_view name, F const functor, Lifetime life={}) {
        DUMP("Body::operator()", args.tags(), args.size());
        if (args.tags() == 1) if (auto given = args.tag(0).get<Str>()) {
            std::string_view s(*given);
            DUMP("Checking for method", name, "from", s);
            if (s == name) {
                DUMP("found match, forwarding args");
                stat = ApplyMethod<F>::invoke(target, life, functor, std::forward<S>(self), args);
                return true;
            }
        }
        return false;
    }

    template <class Base, class S>
    [[nodiscard]] bool derive(S &&self) {
        static_assert(std::is_reference_v<Base>, "T should be a reference type for Body::derive<T>()");
        static_assert(std::is_convertible_v<S &&, Base>);
        static_assert(!std::is_same_v<unqualified<S>, unqualified<Base>>);
        return Impl<unqualified<Base>>::method(*this, std::forward<S>(self));
    }
};

static_assert(std::is_move_constructible_v<Body>);
static_assert(std::is_copy_constructible_v<Body>);

/******************************************************************************/

#warning "have to allow lifetime extension here"

template <class T>
struct MaybeArg {
    std::optional<T> value;
    explicit operator bool() const {return bool(value);}
    T&& operator*() noexcept {return std::move(*value);}

    MaybeArg(Ref &r) : value(r.get(Type<T>())) {}
};

template <class T>
struct MaybeArg<T&> {
    T* value;
    explicit operator bool() const {return bool(value);}
    T& operator*() noexcept {return *value;}

    MaybeArg(Ref &r) : value(r.get(Type<T&>())) {}
};

template <class T>
struct MaybeArg<T const &> {
    T const* value;
    std::optional<T> value2;

    explicit operator bool() const {return value || value2;}
    T const& operator*() noexcept {return value ? *value : *value2;}

    MaybeArg(Ref &r) : value(r.get(Type<T const&>())) {
        if (!value) value2 = r.get(Type<T>());
    }
};


template <class T>
struct MaybeArg<T&&> {
    T* value;
    std::optional<T> value2;

    explicit operator bool() const {return value || value2;}
    T&& operator*() noexcept {return std::move(value ? *value : *value2);}

    MaybeArg(Ref &r) : value(r.get(Type<T&&>())) {
        if (!value) value2 = r.get(Type<T>());
    }
};

/******************************************************************************/

/// Cast element i of v to type T
template <class T>
MaybeArg<T> cast_index(ArgView &v, IndexedType<T> i) {
    DUMP("try to cast argument", i.index, "from", v[i.index].name(), "to", type_name<T>());
    return v[i.index];
}

/******************************************************************************/

/// Invoke a function and arguments, storing output if it doesn't return void
template <class F, class ...Ts>
Method::stat invoke_to(Target& target, F const &f, Ts &&... ts) {
    static_assert(std::is_invocable_v<F, Ts...>, "Function is not invokable with designated arguments");
    using O = simplify_result<std::invoke_result_t<F, Ts...>>;
    using U = unqualified<O>;
    DUMP("invoking function", type_name<F>(), "with output", type_name<O>(), "to constraint", int(target.c.mode));

    if (target.c.mode == Target::None || (std::is_void_v<U> && !target.index())) {
        (void) std::invoke(f, std::forward<Ts>(ts)...);
        return Method::None;
    }

    if constexpr(!std::is_void_v<U>) { // void already handled
        if (target.matches<U>()) { // exact type match, excluding qualifiers

            if (std::is_same_v<O, U &> && target.can_yield(Target::Write)) {
                target.set_reference(std::invoke(f, std::forward<Ts>(ts)...));
                return Method::Write;
            }

            if (std::is_reference_v<O> && target.can_yield(Target::Read)) {
                target.set_reference(static_cast<U const &>(std::invoke(f, std::forward<Ts>(ts)...)));
                return Method::Read;
            }

            if (std::is_same_v<O, U> || std::is_convertible_v<O, U>) {
                if (auto p = target.placement<U>()) {
                    Allocator<U>::invoke_stack(p, f, std::forward<Ts>(ts)...);
                    target.set_index<U>();
                    if (!std::is_same_v<O, U> && !is_dependent<U>) target.set_lifetime({});
                    return Method::Stack;
                }
                if (target.can_yield(Target::Heap)) {
                    target.set_heap(Allocator<U>::invoke_heap(f, std::forward<Ts>(ts)...));
                    if (!std::is_same_v<O, U> && !is_dependent<U>) target.set_lifetime({});
                    return Method::Heap;
                }
            }

        } else {
            if constexpr(std::is_reference_v<O>) {
                switch (Ref(std::invoke(f, std::forward<Ts>(ts)...)).get_to(target)) {
                    case Load::Exception: return Method::Exception;
                    case Load::OutOfMemory: return Method::OutOfMemory;
                    case std::is_same_v<O, U &> ? Load::Write : Load::Read:
                        return std::is_same_v<O, U &> ? Method::Write : Method::Read;
                    default: {}
                }
            } else {
                storage_like<U> storage;
                new (&storage) U(std::invoke(f, std::forward<Ts>(ts)...));
                Ref out(Index::of<U>(), Mode::Stack, Pointer::from(&storage));
                switch (out.get_to(target)) {
                    case Load::Exception: return Method::Exception;
                    case Load::OutOfMemory: return Method::OutOfMemory;
                    case Load::Stack: return Method::Stack;
                    case Load::Heap: return Method::Heap;
                    default: {}
                }
            }
        }
    }

#   warning "needs work... for reference returned as value... etc"

    return Method::wrong_return(target, Index::of<U>(), qualifier_of<O>);
}

/******************************************************************************/

template <bool UseCaller, class F, class ...Ts>
Method::stat caller_invoke(Target& out, F const &f, Caller &&c, MaybeArg<Ts> &&...ts) {
    DUMP("casting arguments");
    if (!(ts && ...)) {
        DUMP("casting arguments failed");
        Code i = 0;
        (void) ((ts ? (++i, true) : ((void) Method::wrong_type(out, i, Index::of<unqualified<Ts>>(), qualifier_of<Ts>), false)) && ...);
        return Method::WrongType;
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
struct Impl<Functor<F>> : Default<Functor<F>> {
    using Signature = simplify_signature<F>;
    using Return = decltype(first_type(Signature()));
    using UseCaller = decltype(second_is_convertible<Caller>(Signature()));
    using Args = decltype(without_first_types<1 + int(UseCaller::value)>(Signature()));

    /*
     Interface implementation for a function with no optional arguments.
     - Returns WrongNumber if args is not the right length
     */
    static bool method(Body m, Functor<F> const &f) noexcept {
        DUMP("call_to function adapter", type_name<F>(), std::addressof(f), "args=", m.args.size());
        if (m.args.tags())
            return Method::wrong_number(m.target, m.args.tags(), 0);
        if (m.args.size() != Args::size)
            return Method::wrong_number(m.target, m.args.size(), Args::size);

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
        return Method::was_invoked(m.stat);
        // It is possible that maybe the invoked function's C++ exception may propagated in the future, assuming the caller policies allow this.
        // Therefore, resource destruction must be done via the frame going out of scope.
    }
};

/******************************************************************************/

template <bool UseCaller, class F, class S, class ...Ts>
Method::stat invoke_method(Target& out, F const &f, Caller &&c, S &&self, MaybeArg<Ts> &&...ts) {
    DUMP("casting arguments");
    if (!(ts && ...)) {
        DUMP("casting arguments failed");
        Code i = 0;
        (void) ((ts ? (++i, true) : (Method::wrong_type(out, i, Index::of<unqualified<Ts>>(), qualifier_of<Ts>), false)) && ...);
        return Method::WrongType;
    }
    DUMP("invoke_method");
    c.enter();
    if constexpr(UseCaller) {
        return invoke_to(out, f, std::forward<S>(self), std::move(c), std::forward<Ts>(*ts)...);
    } else {
        return invoke_to(out, f, std::forward<S>(self), std::forward<Ts>(*ts)...);
    }
}

// This thing does the job of converting arguments, handling the call context
template <class F, class SFINAE>
struct ApplyMethod {
    using Signature = simplify_signature<F>;
    using Return = decltype(first_type(Signature()));
    using UseCaller = decltype(third_is_convertible<Caller>(Signature()));
    using Args = decltype(without_first_types<2 + int(UseCaller::value)>(Signature()));

    /*
     Interface implementation for a function with no optional arguments.
     - Returns WrongNumber if args is not the right length
     */
    template <class S>
    static Method::stat invoke(Target& out, Lifetime life, F const &f, S &&self, ArgView &args) noexcept {
        DUMP("call_to ApplyMethod<", type_name<F>(), ">",
            "address=", std::addressof(f), "args=", args.size(), "tags=", args.tags(), "lifetime=", life.value);

        if (args.size() != Args::size)
            return Method::wrong_number(out, args.size(), Args::size);

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
struct Impl<DefaultFunctor<N, F>> : Default<DefaultFunctor<N, F>> {
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
