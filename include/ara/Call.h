#pragma once
#include "Signature.h"
#include "Ref.h"
#include "Scope.h"
// #include "types/Core.h"
#include <functional>
#include <stdexcept>

namespace ara {

/******************************************************************************/

template <std::size_t Tags, std::size_t Args>
struct ArgStack : ara_args {
    Ref refs[Tags + Args];

    template <class ...Ts>
    ArgStack(Caller &c, Ts &&...ts) noexcept
        : ara_args{&c, Tags, Args}, refs{static_cast<Ts &&>(ts)...} {
        static_assert(Tags + Args == sizeof...(Ts));
    }
};

/******************************************************************************/

struct ArgView : ara_args {
    ArgView() = delete;
    ArgView(ArgView const &) = delete;

    Caller &caller() const {return *static_cast<Caller *>(caller_ptr);}
    Ref &tag(unsigned int i) noexcept {return reinterpret_cast<ArgStack<1, 0> &>(*this).refs[i];}

    Ref *begin() noexcept {return reinterpret_cast<ArgStack<1, 0> &>(*this).refs + tags;}
    auto size() const noexcept {return args;}
    Ref *end() noexcept {return begin() + size();}

    ara_ref* raw_begin() noexcept {return reinterpret_cast<ara_ref *>(reinterpret_cast<ArgStack<1, 0> &>(*this).refs);}
    ara_ref* raw_end() noexcept {return raw_begin() + tags + args;}

    Ref &operator[](std::size_t i) noexcept {return begin()[i];}
};

/******************************************************************************/

/// Cast element i of v to type T
template <class T>
maybe<T> cast_index(ArgView &v, Scope &s, IndexedType<T> i) {
    // s.index = i.index;
    DUMP(i.index);
    DUMP(v[i.index].name());
    return v[i.index].load(s, Type<T>());
}

/******************************************************************************/

/// Invoke a function and arguments, storing output if it doesn't return void
template <class F, class ...Ts>
Call::stat invoke_to(Target& out, F const &f, Ts &&... ts) {
    static_assert(std::is_invocable_v<F, Ts...>, "Function is not invokable with designated arguments");
    using O = simplify_result<std::invoke_result_t<F, Ts...>>;
    using U = unqualified<O>;
    DUMP("invoking function ", type_name<F>(), " with output ", type_name<O>());

    if (out.tag == Target::None || (std::is_void_v<U> && !out.idx)) {
        std::invoke(f, static_cast<Ts &&>(ts)...);
        return Call::None;
    }

    if (out.accepts<U>()) {
        if constexpr(!std::is_void_v<U>) { // void already handled
            if (out.wants_value()) {
                if constexpr(std::is_same_v<O, U> || std::is_convertible_v<O, U>) {
                    if (auto p = out.placement<U>()) {
                        new(p) U(std::invoke(f, static_cast<Ts &&>(ts)...));
                        out.set_index<U>();
                        return Call::Stack;
                    } else {
                        out.set_reference(*new U(std::invoke(f, static_cast<Ts &&>(ts)...)));
                        DUMP("returned heap...? ", int(out.tag));
                        return Call::Heap;
                    }
                }
            } else { // returns const & or &
                if constexpr(std::is_reference_v<O>) {
                    if (std::is_same_v<O, U &> || out.tag != Target::Mutable) {
                        DUMP("set reference");
                        out.set_reference(std::invoke(f, static_cast<Ts &&>(ts)...));
                        return std::is_same_v<O, U &> ? Call::Mutable : Call::Const;
                    }
                }
            }
        }
    }
    return Call::wrong_return(out, Index::of<U>(), qualifier_of<O>);
}

/******************************************************************************/

template <bool UseCaller, class F, class ...Ts>
Call::stat caller_invoke(Target& out, F const &f, Caller &&c, maybe<Ts> &&...ts) {
    DUMP("casting arguments");
    if (!(ts && ...)) {
        DUMP("casting arguments failed");
        Code i = 0;
        (void) ((ts ? (++i, true) : (Call::wrong_type(out, i, Index::of<unqualified<Ts>>(), qualifier_of<Ts>), false)) && ...);
        return Call::WrongType;
    }
    DUMP("caller_invoke");
    c.enter();
    if constexpr(UseCaller) {
        return invoke_to(out, f, std::move(c), static_cast<Ts &&>(*ts)...);
    } else {
        return invoke_to(out, f, static_cast<Ts &&>(*ts)...);
    }
}


/******************************************************************************/

template <int N, class F, class SFINAE=void>
struct Adapter;

template <class F, class SFINAE=void>
struct MethodAdapter;

struct Method {
    Target &target;
    ArgView &args;
    Call::stat &stat;

    template <class S, class F>
    [[nodiscard]] bool operator()(S &&self, F const functor) {
        DUMP(args.tags, args.size());
        Scope s;
        if (args.tags == 0) {
            DUMP("found match");
            stat = MethodAdapter<F>::call(target, functor, std::forward<S>(self), args);
            return true;
        }
        return false;
    }

    template <class S, class F>
    [[nodiscard]] bool operator()(S &&self, std::string_view name, F const functor) {
        DUMP(args.tags, args.size());
        Scope s;
        if (args.tags == 1) if (auto given = args.tag(0).load<std::string_view>(s)) {
            DUMP(*given, " ", name);
            if (*given == name) {
                DUMP("found match");
                stat = MethodAdapter<F>::call(target, functor, std::forward<S>(self), args);
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
        // stat = Callable<unqualified<Base>>()(target, std::forward<S>(self), args);
        return Call::was_invoked(stat);
    }
};

/******************************************************************************/


template <int N, class F>
struct Functor {
    F function;
    Functor(F &&f) : function(std::move(f)) {}

    constexpr operator F const &() const noexcept {
        DUMP("cast into Functor ", &function, " ", reinterpret_cast<void const *>(function), " ", type_name<F>());
        return function;
    }
};

template <int N, class F>
struct Callable<Functor<N, F>> : Adapter<N, F>, std::true_type {};

/******************************************************************************/

template <class F, class SFINAE>
struct Adapter<0, F, SFINAE> {
    using Signature = simplify_signature<F>;
    using Return = decltype(first_type(Signature()));
    using UseCaller = decltype(second_is_convertible<Caller>(Signature()));
    using Args = decltype(without_first_types<1 + int(UseCaller::value)>(Signature()));

    /*
     Interface implementation for a function with no optional arguments.
     - Returns WrongNumber if args is not the right length
     */
    bool operator()(Method m, F const &f) const noexcept {
        DUMP("call_to function adapter ", type_name<F>(), " ", std::addressof(f), " args=", m.args.size());
        if (m.args.tags)
            return Call::wrong_number(m.target, m.args.tags, 0);
        if (m.args.size() != Args::size)
            return Call::wrong_number(m.target, m.args.size(), Args::size);

        auto frame = m.args.caller().new_frame(); // make a new unentered frame, must be noexcept
        Caller handle(frame); // make the Caller with a weak reference to frame
        Scope scope(handle);

        m.stat = m.target.make_noexcept([&] {
            return Args::indexed([&](auto ...ts) {
                DUMP("invoking...");
                return caller_invoke<UseCaller::value, F, decltype(*ts)...>(m.target, f, std::move(handle), cast_index(m.args, scope, ts)...);
            });
        });
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
struct MethodAdapter {
    using Signature = simplify_signature<F>;
    using Return = decltype(first_type(Signature()));
    using UseCaller = decltype(third_is_convertible<Caller>(Signature()));
    using Args = decltype(without_first_types<2 + int(UseCaller::value)>(Signature()));

    /*
     Interface implementation for a function with no optional arguments.
     - Returns WrongNumber if args is not the right length
     */
    template <class S>
    static Call::stat call(Target& out, F const &f, S &&self, ArgView &args) noexcept {
        DUMP("call_to MethodAdapter<", type_name<F>(), ">: ", std::addressof(f), " ", args.size());

        if (args.size() != Args::size)
            return Call::wrong_number(out, args.size(), Args::size);

        auto frame = args.caller().new_frame(); // make a new unentered frame, must be noexcept
        Caller handle(frame); // make the Caller with a weak reference to frame
        Scope scope(handle);

        return out.make_noexcept([&] {
            return Args::indexed([&](auto ...ts) {
                DUMP("invoking...");
                return invoke_method<UseCaller::value, F, S &&, decltype(*ts)...>(
                    out, f, std::move(handle), std::forward<S>(self), cast_index(args, scope, ts)...);
            });
        });
        // It is planned to be allowed that the invoked function's C++ exception may propagad       // in the future, assuming the caller policies allow this.
        // Therefore, resource destruction must be done via the frame going out of scope.
    }
};


/******************************************************************************/

// template <class R, class C>
// struct Adapter<0, R C::*, std::enable_if_t<std::is_member_object_pointer_v<R C::*>>> {
//     using F = R C::*;

//     static stat::call call_to(FnOutput &v, F const &f, Caller &&c, ArgView args) noexcept {
//         if (args.size() != 1) return v.WrongNumber(1, args.size());

//         auto frame = caller.new_frame();
//         DUMP("Adapter<", Index::of<R>(), ", ", Index::of<C>(), ">::()");
//         Caller handle(frame);
//         Scope s(handle);

//         if (auto p = self.request<C &&>()) {
//             return out.set(std::move(*p).*f), stat::call::ok;
//         } else if (auto p = self.request<C &>()) {
//             return out.set((*p).*f), stat::call::ok;
//         } else if (auto p = self.request<C const &>()) {
//             return out.set((*p).*f), stat::call::ok;
//         }
//         // value conversions not allowed currently
//         // else if (auto p = self.request_value<C>()) { }
//         throw std::move(s.set_error("invalid argument"));
//     }
// };

/******************************************************************************/

/*
    For a callable type we need to get a function pointer of:
    bool(void const *self, void *out, Caller &&, ArgView);
*/
template <int N=-1, class F>
auto make_function(F f) {
    // First we apply lossless simplifications.
    // i.e. a lambda with specified arguments and no defaults is converted to a function pointer.
    // This makes the type more readable and might reduce the compile time a bit.
    auto simplified = SimplifyFunction<F, N>()(std::move(f));

    using S = decltype(simplified);

    // Now get the number of optional arguments
    constexpr int n = N == -1 ? 0 : simplify_signature<S>::size - 1 - N;

    // Return the callable object holding the functor
    static_assert(is_usable<Functor<n, S>>);
    return Functor<n, S>{std::move(simplified)};
}

/******************************************************************************/

// // N is the number of trailing optional arguments
// template <std::size_t N, class F, class SFINAE>
// struct Adapter {
//     F function;
//     using Signature = simplify_signature<F>;
//     using Return = decltype(first_type(Signature()));
//     using UsesCaller = decltype(second_is_convertible<Caller>(Signature()));
//     using Args = decltype(without_first_types<1 + int(UsesCaller::value)>(Signature()));

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
// };

// /******************************************************************************/



template <class T>
struct Arg;

template <class T>
struct Arg<T&> {
    T &t;

    Arg(T &t) : t(t) {}
    Ref ref() {return Ref(t);}
};

template <class T>
struct Arg<T const&> {
    T const &t;

    Arg(T const &t) : t(t) {}
    Ref ref() {return Ref(t);}
};

template <class T>
struct Arg<T &&> {
    storage_like<T> storage;

    Arg(T &&t) {new(&storage) T(std::move(t));}
    Ref ref() {return Ref(Index::of<T>(), Tag::Stack, &storage);}
};

/******************************************************************************/

struct CallError : std::exception {
    char const * message;
    Call::stat code;

    CallError(char const *m, Call::stat c) noexcept : message(m), code(c) {}
    char const * what() const noexcept override {return message;}

};

[[noreturn]] void call_throw(Target &&target, Call::stat c);

/******************************************************************************/

template <class T>
struct Destructor {
    T &held;
    ~Destructor() noexcept {Destruct::impl<T>::put(held, Destruct::Stack);}
};

/******************************************************************************/

template <class T>
struct CallReturn {
    static std::optional<T> get(Index i, Tag qualifier, void *self, Caller &c, ArgView &args) {
        std::aligned_union_t<0, T, void*> buffer;
        Target target{Index::of<T>(), &buffer, sizeof(buffer), Target::Stack};
        auto const stat = Call::call(i, target, self, qualifier, args);

        std::optional<T> out;
        switch (stat) {
            case Call::Stack: {
                Destructor<T> raii{storage_cast<T>(buffer)}; // fix to be noexcept
                out.emplace(std::move(raii.held));
                break;
            }
            case Call::Impossible:  {break;}
            case Call::WrongType:   {break;}
            case Call::WrongNumber: {break;}
            case Call::WrongReturn: {break;}
            default: call_throw(std::move(target), stat);
        }
        return out;
    }

    template <class ...Ts>
    static T call(Index i, Tag qualifier, void *self, Caller &c, ArgView &args) {
        std::aligned_union_t<0, T, void*> buffer;
        Target target{Index::of<T>(), &buffer, sizeof(buffer), Target::Stack};
        auto const stat = Call::call(i, target, self, qualifier, args);

        switch (stat) {
            case Call::Stack: {
                Destructor<T> raii{storage_cast<T>(buffer)}; // fix to be noexcept
                return std::move(raii.held);
            }
            default: call_throw(std::move(target), stat);
        }
    }
};

/******************************************************************************/

template <class T>
struct CallReturn<T &> {
    static T * get(Index i, Tag qualifier, void *self, Caller &c, ArgView &args) {
        DUMP("calling something that returns reference ...");
        Target target{Index::of<std::remove_cv_t<T>>(), nullptr, 0,
            std::is_const_v<T> ? Target::Const : Target::Mutable};

        auto const stat = Call::call(i, target, self, qualifier, args);
        DUMP("got stat ", stat);
        switch (stat) {
            case (std::is_const_v<T> ? Call::Const : Call::Mutable): return *reinterpret_cast<T *>(target.out);
            case Call::Impossible:  {return nullptr;}
            case Call::WrongType:   {return nullptr;}
            case Call::WrongNumber: {return nullptr;}
            case Call::WrongReturn: {return nullptr;}
            default: call_throw(std::move(target), stat);
        }
    }

    static T & call(Index i, Tag qualifier, void *self, Caller &c, ArgView &args) {
        DUMP("calling something that returns reference ...");
        Target target{Index::of<std::remove_cv_t<T>>(), nullptr, 0,
            std::is_const_v<T> ? Target::Const : Target::Mutable};

        auto const stat = Call::call(i, target, self, qualifier, args);
        DUMP("got stat ", stat);
        switch (stat) {
            case (std::is_const_v<T> ? Call::Const : Call::Mutable): return *reinterpret_cast<T *>(target.out);
            default: call_throw(std::move(target), stat);
        }
    }
};

/******************************************************************************/

template <>
struct CallReturn<void> {
    static void call(Index i, Tag qualifier, void *self, Caller &c, ArgView &args) {
        DUMP("calling something... ", args.size());
        Target target{Index(), nullptr, 0, Target::None};

        auto const stat = Call::call(i, target, self, qualifier, args);
        DUMP("got stat ", stat);
        switch (stat) {
            case Call::None: {return;}
            default: call_throw(std::move(target), stat);
        }
    }

    static void get(Index i, Tag qualifier, void *self, Caller &c, ArgView &args) {
        DUMP("calling something...");
        Target target{Index(), nullptr, 0, Target::None};

        auto const stat = Call::call(i, target, self, qualifier, args);
        DUMP("got stat ", stat);
        switch (stat) {
            case Call::None: {return;}
            case Call::Impossible:  {return;}
            case Call::WrongType:   {return;}
            case Call::WrongNumber: {return;}
            case Call::WrongReturn: {return;}
            default: call_throw(std::move(target), stat);
        }
    }
};

/******************************************************************************/

template <>
struct CallReturn<Ref> {
    static Ref call(Index i, Tag qualifier, void *self, Caller &c, ArgView &args) {
        DUMP("calling something...");
        Target target{Index(), nullptr, 0, Target::Reference};
        auto stat = Call::call(i, target, self, qualifier, args);

        switch (stat) {
            case Call::Const:   return Ref(target.idx, Tag::Const, target.out);
            case Call::Mutable: return Ref(target.idx, Tag::Mutable, target.out);
            // function is noexcept until here, now it is permitted to throw (I think)
            default: return nullptr;
        }
    }
};

/******************************************************************************/


namespace parts {

template <class T>
struct Reduce {
    static_assert(!std::is_reference_v<T>);
    using type = std::decay_t<T>;
};

template <class T>
struct Reduce<T &> {
    // using type = std::decay_t<T> &;
    using type = T &;
};

template <class T>
using const_decay = std::conditional_t<
    std::is_array_v<T>,
    std::remove_extent_t<T> const *,
    std::conditional_t<std::is_function_v<T>, std::add_pointer_t<T>, T>
>;

template <class T>
using shrink_const = std::conditional_t<false && std::is_trivially_copyable_v<T> && is_always_stackable<T>, T, T const &>;

template <class T>
struct Reduce<T const &> {
    using type = shrink_const<const_decay<T>>;
    static_assert(std::is_convertible_v<T, type>);
};

// static_assert(std::is_same_v<typename Reduce< char const (&)[3] >::type, char const *>);
// static_assert(std::is_same_v<typename Reduce< std::string const & >::type, std::string const &>);
// static_assert(std::is_same_v<typename Reduce< void(double) >::type, void(*)(double)>);


template <class T, int N, class ...Ts>
T call_args(Index i, Tag qualifier, void *self, Caller &c, Arg<Ts &&> ...ts) {
    ArgStack<N, sizeof...(Ts) - N> args(c, ts.ref()...);
    DUMP(type_name<T>(), " tags=", N, " args=", reinterpret_cast<ArgView &>(args).size());
    return CallReturn<T>::call(i, qualifier, self, c, reinterpret_cast<ArgView &>(args));
}

template <class T, int N, class ...Ts>
T call(Index i, Tag qualifier, void *self, Caller &c, Ts &&...ts) {
    return call_args<T, N, typename Reduce<Ts>::type...>(i, qualifier, self, c, static_cast<Ts &&>(ts)...);
}

template <class T, int N, class ...Ts>
maybe<T> get_args(Index i, Tag qualifier, void *self, Caller &c, Arg<Ts &&> ...ts) {
    ArgStack<N, sizeof...(Ts) - N> args(c, ts.ref()...);
    return CallReturn<T>::get(i, qualifier, self, c, reinterpret_cast<ArgView &>(args));
}

template <class T, int N, class ...Ts>
maybe<T> get(Index i, Tag qualifier, void *self, Caller &c, Ts &&...ts) {
    return get_args<T, N, typename Reduce<Ts>::type...>(i, qualifier, self, c, static_cast<Ts &&>(ts)...);
}


}






}
