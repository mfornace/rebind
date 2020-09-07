#pragma once
#include "Frame.h"

// Implementations for defining functions and methods in C++

namespace sfb {

/******************************************************************************/

// This is the actual callable object plus a lifetime annotation
template <class F>
struct Functor {
    F function;
    Lifetime lifetime;
};

// This is some future thing for default arguments
template <int N, class F>
struct DefaultFunctor : Functor<F> {
    static_assert(N > 0, "Functor must have some default arguments");
    using Functor<F>::Functor;
};

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
        if constexpr(std::is_same_v<U, Index>) {
            if (target.can_yield(Target::Index2)) {
                target.c.index = std::invoke(f, std::forward<Ts>(ts)...);
                return Call::Index2;
            }
        }

        if (target.matches<U>()) { // exact type match, excluding qualifiers

            if (std::is_same_v<O, U &> && target.can_yield(Target::Write)) {
                target.set_reference(std::invoke(f, std::forward<Ts>(ts)...));
                return Call::Write;
            }

            if (std::is_reference_v<O> && target.can_yield(Target::Read)) {
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
                if (target.can_yield(Target::Heap)) {
                    target.set_heap(Allocator<U>::invoke_heap(f, std::forward<Ts>(ts)...));
                    if (!std::is_same_v<O, U> && !is_dependent<U>) target.set_lifetime({});
                    return Call::Heap;
                }
            }

        } else {
            if constexpr(std::is_reference_v<O>) {
                switch (Ref(std::invoke(f, std::forward<Ts>(ts)...)).get_to(target)) {
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
                switch (out.get_to(target)) {
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

template <class T>
struct ArgCast {
    T value;

    T&& operator*() noexcept {return std::move(value);}

    static bool put(ArgCast* a, Ref& r) {
        if (auto p = r.get(Type<T>())) {
            new (a) ArgCast{std::move(*p)};
            return true;
        } else return false;
    }
};

/******************************************************************************/

template <class T>
struct ArgCast<T&> {
    T* value;

    T& operator*() noexcept {return *value;}

    static bool put(ArgCast* a, Ref& r) {
        if (auto p = r.get(Type<T&>())) {
            new (a) ArgCast{p};
            return true;
        } else return false;
    }
};

/******************************************************************************/

template <class T>
struct ArgCast<T const &> {
    std::aligned_union_t<0, T, T const*> t;
    bool held;

    T const& operator*() noexcept {return held ? reinterpret_cast<T const&>(t) : *reinterpret_cast<T const*&>(t);}

    static bool put(ArgCast* a, Ref& r) {
        DUMP("ArgCast: try reference conversion");
        if (auto p = r.get(Type<T const&>())) {new (&a->t) T const*(p); a->held = false; return true;}
        DUMP("ArgCast: try copy conversion");
        if (auto p = r.get(Type<T>()))        {new (&a->t) T(std::move(*p)); a->held = true; return true;}
        a->held = false;
        return false;
    }

    ~ArgCast() noexcept {if (held) reinterpret_cast<T&>(t).~T();}
};

/******************************************************************************/

template <class T>
struct ArgCast<T&&> {
    std::aligned_union_t<0, T, T*> t;
    bool held;

    T&& operator*() noexcept {return std::move(held ? reinterpret_cast<T &>(t) : *reinterpret_cast<T*&>(t));}

    static bool put(ArgCast* a, Ref& r) {
        if (auto p = r.get(Type<T&&>())) {new (&a->t) T*(p); a->held = false; return true;}
        if (auto p = r.get(Type<T>()))   {new (&a->t) T(std::move(*p)); a->held = true; return true;}
        return false;
    }

    ~ArgCast() noexcept {if (held) reinterpret_cast<T&>(t).~T();}
};

/******************************************************************************/

template <class ...Ts>
struct CastedArgs {
    std::tuple<storage_like<ArgCast<Ts>>...> storage;
    std::uint32_t done = 0;

    template <std::size_t ...Is>
    Call::stat error(Target& t, std::index_sequence<Is...>) const {
        Call::stat err;
        (void)((Is == done ? (err = Call::wrong_type(t, Is, Index::of<unqualified<Ts>>(), qualifier_of<Ts>), true) : false) || ...);
        return err;
    }

    template <class F, std::size_t ...Is>
    Call::stat operator()(Target& t, ArgView& args, F const& callback, std::index_sequence<Is...>) {
        // Check number of arguments
        if (args.size() != sizeof...(Ts))
            return Call::wrong_number(t, args.size(), sizeof...(Ts));
        // Cast each argument
        bool ok = ((ArgCast<Ts>::put(reinterpret_cast<ArgCast<Ts>*>(&std::get<Is>(storage)),
            args[Is]) ? (++done, true) : false) && ...);
        if (ok) return callback(*reinterpret_cast<ArgCast<Ts>&>(std::get<Is>(storage))...);
        else return error(t, std::index_sequence<Is...>());
    }

    template <std::size_t ...Is>
    void destruct(std::index_sequence<Is...>) noexcept {
        if (done == sizeof...(Ts)) {
            (reinterpret_cast<ArgCast<Ts>&>(std::get<Is>(storage)).~ArgCast<Ts>(), ...);
        } else {
            (void)((Is < done ? (reinterpret_cast<ArgCast<Ts>&>(std::get<Is>(storage)).~ArgCast<Ts>(), true) : false) && ...);
        }
    }

    ~CastedArgs() noexcept {destruct(std::make_index_sequence<sizeof...(Ts)>());}
};

template <class F, class ...Ts>
Call::stat apply_casts(Pack<Ts...>, Target& t, ArgView& args, F const& callback) {
    return CastedArgs<Ts...>()(t, args, callback, std::make_index_sequence<sizeof...(Ts)>());
}

/******************************************************************************/

// This thing does the job of converting arguments, handling the call context
template <class F, class SFINAE>
struct ApplyCall {
    using Signature = simplify_signature<F>;
    using Return = decltype(first_type(Signature()));
    using Args = decltype(pop_first_type(Signature()));

    /*
     Interface implementation for a function with no optional arguments.
     - Returns WrongNumber if args is not the right length
     */
    static Call::stat invoke(Target& out, Lifetime life, F const &f, ArgView &args) noexcept {
        DUMP("call_to ApplyCall<", type_name<F>(), ">", type_name<Signature>(),
            "address=", std::addressof(f), "args=", args.size(), "tags=", args.tags(), "lifetime=", life.value);

        out.set_lifetime(life);
        return out.make_noexcept([&] {
            return apply_casts(Args(), out, args, [&](auto &&...ts) {
                return invoke_to(out, f, std::forward<decltype(ts)>(ts)...);
            });
        });
    }
};

/******************************************************************************/

// Impl for a functor
// method() calls the functor with argview, returning result in target
// prepare() takes the argview, casts each argument to the exact type requested
// some arguments are fine as is or easily convertible, these can be just left in the argview
// some arguments are non-trivial, have to allocate space for them ...
// better not to do this on heap, so should just allocate the maybe tuple for this stuff
// currently on the stack but it won't be able to be kept there if has to be returned
// other option is to take a pointer which actually does invocation...?
// could put the returned args into the target, would sometimes fit there
// dont really need the bool annotation though...
template <class F>
struct Impl<Functor<F>> : Default<Functor<F>> {
    using Signature = simplify_signature<F>;
    using Return = decltype(first_type(Signature()));
    using Args = decltype(prepend_type(pop_first_type(Signature()), Type<Functor<F> const&>()));

    /*
     Interface implementation for a function with no optional arguments.
     - Returns WrongNumber if args is not the right length
     */
    static bool call(Frame m) noexcept {
        DUMP("call_to function adapter", type_name<F>(), "args=", m.args.size());
        if (m.args.tags())
            return Call::wrong_number(m.target, m.args.tags(), 0);

        m.stat = m.target.make_noexcept([&] {
            return apply_casts(Args(), m.target, m.args, [&](Functor<F> const& f, auto &&...ts) {
                m.target.set_lifetime(f.lifetime);
                return invoke_to(m.target, f.function, std::forward<decltype(ts)>(ts)...);
            });
        });
        DUMP("invoked with stat", m.stat);
        return Call::was_invoked(m.stat);
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

/******************************************************************************/

// N is the number of trailing optional arguments
template <int N, class F>
struct Impl<DefaultFunctor<N, F>> : Default<DefaultFunctor<N, F>> {
    using Signature = simplify_signature<F>;
    using Return = decltype(first_type(Signature()));
    using Args = decltype(pop_first_type(Signature()));

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
