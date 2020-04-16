#pragma once
#include "Signature.h"
#include "Ref.h"
#include "Scope.h"
// #include "types/Core.h"
#include <functional>
#include <stdexcept>

namespace rebind {

/******************************************************************************************/

template <std::size_t N>
struct ArgStack : rebind_args {
    Ref refs[N];

    template <class ...Ts>
    ArgStack(Caller &c, Ts &&...ts) noexcept
        : rebind_args{&c, N}, refs{static_cast<Ts &&>(ts)...} {}
};

/******************************************************************************************/

struct ArgView : rebind_args {
    ArgView() = delete;
    ArgView(ArgView const &) = delete;

    Caller &caller() const {return *static_cast<Caller *>(caller_ptr);}

    // std::string_view name() const {
    //     if (name_ptr) return std::string_view(name_ptr, name_len);
    //     else return {};
    // }

    Ref * begin() noexcept {return reinterpret_cast<ArgStack<1> &>(*this).refs;}

    auto size() const noexcept {return args;}

    Ref * end() noexcept {return begin() + size();}

    Ref &operator[](std::size_t i) noexcept {return begin()[i];}
};

/******************************************************************************************/

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
    std::aligned_union_t<0, T> storage;

    Arg(T &&t) {new(&storage) T(std::move(t));}
    Ref ref() {return Ref(TagIndex(Index::of<T>(), Stack), &storage);}
};

/******************************************************************************************/

inline void handle_call_errors(Call::stat, Target const &target) {

}

/******************************************************************************************/

template <class T>
struct CallReturn {
    template <class ...Ts>
    static std::optional<T> call(Index i, Tag qualifier, void *self, Caller &c, Arg<Ts &&> ...ts) {
        std::aligned_union_t<0, T, void*> buffer;
        DUMP("calling something...");
        ArgStack<sizeof...(Ts)> args(c, ts.ref()...); // Ts && now safely scheduled for destruction
        Target target{&buffer, Index::of<T>(), sizeof(buffer), Target::Stack};
        auto const stat = Call::call(i, &target, self, qualifier, reinterpret_cast<ArgView &>(args));

        std::optional<T> out;
        // can throw, fix this later
        if (stat == Call::in_place) {
            auto p = reinterpret_cast<T *>(&buffer);
            out.emplace(std::move(*p));
            p->~T();
        } else if (stat == Call::heap) {
            auto p = reinterpret_cast<T *&>(buffer);
            out.emplace(std::move(*p));
            delete p;
        } else {
            // function is noexcept until here, now it is permitted to throw (I think)
            handle_call_errors(stat, target);
        }
        return out;
    }
};

/******************************************************************************************/

template <class T>
struct CallReturn<T &> {
    template <class ...Ts>
    static T * call(Index i, Tag qualifier, void *self, Caller &c, Arg<Ts &&> ...ts) {
        DUMP("calling something that returns reference ...");
        ArgStack<sizeof...(Ts)> args(c, ts.ref()...); // Ts && now safely scheduled for destruction
        Target target{nullptr, Index::of<std::remove_cv_t<T>>(), 0,
            std::is_const_v<T> ? Target::Const : Target::Mutable};
        auto const stat = Call::call(i, &target, self, qualifier, reinterpret_cast<ArgView &>(args));
        DUMP("got stat ", stat);

        if (stat == Call::in_place) {
            return reinterpret_cast<T *>(target.out);
        } else {
            // function is noexcept until here, now it is permitted to throw (I think)
            handle_call_errors(stat, target);
            return nullptr;
        }
    }
};

/******************************************************************************************/

template <>
struct CallReturn<void> {
    template <class ...Ts>
    static void call(Index i, Tag qualifier, void *self, Caller &c, Arg<Ts &&> ...ts) {
        DUMP("calling something...");
        ArgStack<sizeof...(Ts)> args(c, ts.ref()...); // Ts && now safely scheduled for destruction
        auto const stat = Call::call(i, nullptr, self, qualifier, reinterpret_cast<ArgView &>(args));
        //handle_call_errors(stat, target);
    }
};

/******************************************************************************************/

template <>
struct CallReturn<Ref> {
    template <class ...Ts>
    static Ref call(Index i, Tag qualifier, void *self, Caller &c, Arg<Ts &&> ...ts) {
        DUMP("calling something...");
        ArgStack<sizeof...(Ts)> args(c, ts.ref()...); // Ts && now safely scheduled for destruction
        Target target{nullptr, Index(), 0, Target::Reference};
        auto const stat = Call::call(i, &target, self, qualifier, reinterpret_cast<ArgView &>(args));

        if (stat == Call::in_place) {
            return Ref(TagIndex(target.idx, target.tag), target.out);
        } else {
            // function is noexcept until here, now it is permitted to throw (I think)
            handle_call_errors(stat, target);
            return nullptr;
        }
    }
};

/******************************************************************************************/

namespace parts {

template <class T, class ...Ts>
Maybe<T> call(Index i, Tag qualifier, void *self, Caller &c, Ts &&...ts) {
    DUMP(type_name<T>());
    return CallReturn<T>::template call<Ts...>(i, qualifier, self, c, static_cast<Ts &&>(ts)...);
}

}

/******************************************************************************/

/// Cast element i of v to type T
template <class T>
Maybe<T> cast_index(ArgView &v, Scope &s, IndexedType<T> i) {
    // s.index = i.index;
    DUMP(i.index);
    DUMP(v[i.index].name());
    return v[i.index].load(s, Type<T>());
}

/******************************************************************************/

/// Invoke a function and arguments, storing output if it doesn't return void
template <class F, class ...Ts>
Call::stat invoke_to(Target* out, F const &f, Ts &&... ts) {
    using O = simplify_result<std::invoke_result_t<F, Ts...>>;
    using U = unqualified<O>;
    DUMP("invoking function ", type_name<F>(), " with output ", type_name<O>());

    if (!out || (std::is_void_v<U> && !out->idx)) {
        std::invoke(f, static_cast<Ts &&>(ts)...);
        return Call::none;
    }

    if (!out->accepts<U>()) return Call::wrong_type;

    if constexpr(!std::is_void_v<U>) { // void already handled
        if (out->wants_value()) {
            if constexpr(std::is_same_v<O, U> || std::is_convertible_v<O, U>) {
                if (auto p = out->placement<U>()) {
                    new(p) U(std::invoke(f, static_cast<Ts &&>(ts)...));
                    out->set_index<U>();
                    return Call::in_place;
                } else {
                    out->set_reference(*new U(std::invoke(f, static_cast<Ts &&>(ts)...)));
                    return Call::heap;
                }
            }
        } else { // returns const & or &
            if constexpr(std::is_reference_v<O>) {
                if (std::is_same_v<O, U &> || out->tag != Target::Mutable) {
                    DUMP("set reference");
                    out->set_reference(std::invoke(f, static_cast<Ts &&>(ts)...));
                    return Call::in_place;
                }
            }
        }
    }
    return Call::wrong_qualifier;
}

template <bool UseCaller, class F, class ...Ts>
Call::stat caller_invoke(Target* out, F const &f, Caller &&c, Maybe<Ts> &&...ts) {
    DUMP("casting arguments");
    if (!(ts && ...)) {
        DUMP("casting arguments failed");
        return Call::invalid_argument;
    }
    DUMP("caller_invoke");
    c.enter();
    if constexpr(UseCaller) {
        return invoke_to(out, f, std::move(c), static_cast<Ts &&>(*ts)...);
    } else {
        return invoke_to(out, f, static_cast<Ts &&>(*ts)...);
    }
}

/******************************************************************************************/

template <int N, class F, class SFINAE=void>
struct Adapter;

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
    Call::stat operator()(Target* out, F const &f, ArgView &args) const noexcept {
        DUMP("call_to function adapter ", type_name<F>(), " ", std::addressof(f), " ", args.size());
        // DUMP("method name", args.name(), " ", !args.name().empty());

        if (args.size() != Args::size) {
            // return v.wrong_number(Args::size, args.size());
            return Call::wrong_number;
        }

        auto frame = args.caller().new_frame(); // make a new unentered frame, must be noexcept
        Caller handle(frame); // make the Caller with a weak reference to frame
        Scope s(handle);

        return Args::indexed([&](auto ...ts) {
            DUMP("invoking...");
            return caller_invoke<UseCaller::value, F, decltype(*ts)...>(out, f, std::move(handle), cast_index(args, s, ts)...);
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
//         if (args.size() != 1) return v.wrong_number(1, args.size());

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

/******************************************************************************/

}
