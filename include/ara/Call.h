#pragma once
#include "Signature.h"
#include "Ref.h"
#include <functional>
#include <stdexcept>

// Implementations for calling a function from C++

namespace ara {

/******************************************************************************/

template <std::size_t Tags, std::size_t Args>
struct ArgStack : ara_args {
    Ref refs[Args + Tags];

    template <class ...Ts>
    ArgStack(Context c, Ts &&...ts) noexcept
        : ara_args{c, Tags, Args}, refs{static_cast<Ts &&>(ts)...} {
        static_assert(Args + Tags == sizeof...(Ts));
        // Possible to do at compile time...but sort of tedious.
        std::reverse(std::begin(refs), std::end(refs));
    }
};

/******************************************************************************/

template <>
struct ArgStack<0, 0> : ara_args {
    ArgStack(Context c) noexcept : ara_args{c, 0, 0} {}
};

/******************************************************************************/

struct ArgView {
    ara_args c;

    Ref* ptr() noexcept {return reinterpret_cast<ArgStack<1, 0> &>(c).refs;}

    /// Begin/End of arguments
    auto begin() noexcept {return std::make_reverse_iterator(ptr() + c.args);}
    auto end() noexcept {return std::make_reverse_iterator(ptr());}
    /// Number of args
    auto size() const noexcept {return c.args;}

    /// Number of tags
    auto tags() const noexcept {return c.tags;}
    Ref &tag(unsigned int i) noexcept {return ptr()[c.args + c.tags - 1 - i];}

    Ref &operator[](std::size_t i) noexcept {return begin()[i];}
};

static_assert(std::is_aggregate_v<ArgView>);

/******************************************************************************/

template <class T>
struct Arg;

// Just hold the reference. No extra conversions possible
template <class T>
struct Arg<T&> {
    T &t;

    Arg(T &t) noexcept : t(t) {}
    Ref ref() noexcept {return Ref(t);}
};

template <class T>
struct Arg<T const&> {
    T const& t;

    Arg(T const& t) noexcept : t(t) {}
    Ref ref() noexcept {return Ref(t);}
};

// For rvalue, Hold a non-RAII version of the value to allow stealing
template <class T>
struct Arg<T&&> {
    storage_like<T> storage;

    Arg(T&& t) noexcept {Allocator<T>::stack(&storage, std::move(t));}
    Ref ref() noexcept {return Ref(Index::of<T>(), Mode::Stack, Pointer::from(&storage));}
};

[[noreturn]] void call_throw(Target &&target, Call::stat c);

/******************************************************************************/
// auto const stat = Call::invoke(i, target, self, mode, args);



template <class T>
struct Output<T, false> {
    template <class F>
    std::optional<T> operator()(F &&f) const {
        std::aligned_union_t<0, T, void*> buffer;
        Target target(Index::of<T>(), &buffer, sizeof(buffer), Target::constraint<T>);
        auto const stat = f(target);

        std::optional<T> out;
        switch (stat) {
            case Call::Stack: {
                Destruct::RAII<T> raii{storage_cast<T>(buffer)};
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
};

template <class T>
struct Output<T, true> {
    template <class F>
    T operator()(F &&f) const {
        std::aligned_union_t<0, T, void*> buffer;
        Target target(Index::of<T>(), &buffer, sizeof(buffer), Target::constraint<T>);
        auto const stat = f(target);

        switch (stat) {
            case Call::Stack: {
                Destruct::RAII<T> raii{storage_cast<T>(buffer)};
                return std::move(raii.held);
            }
            default: call_throw(std::move(target), stat);
        }
    }
};

/******************************************************************************/

template <class T>
struct Output<T &, false> {
    template <class F>
    T * operator()(F &&f) const {
        DUMP("calling something that returns reference ...");
        Target target(Index::of<std::remove_cv_t<T>>(), nullptr, 0,
            std::is_const_v<T> ? Target::Read : Target::Write);

        auto const stat = f(target);
        DUMP("got stat", stat);
        switch (stat) {
            case (std::is_const_v<T> ? Call::Read : Call::Write): return *reinterpret_cast<T *>(target.output());
            case Call::Impossible:  {return nullptr;}
            case Call::WrongType:   {return nullptr;}
            case Call::WrongNumber: {return nullptr;}
            case Call::WrongReturn: {return nullptr;}
            default: call_throw(std::move(target), stat);
        }
    }
};

template <class T>
struct Output<T &, true> {
    template <class F>
    T & operator()(F &&f) const {
        DUMP("calling something that returns reference ...");
        Target target(Index::of<std::remove_cv_t<T>>(), nullptr, 0,
            std::is_const_v<T> ? Target::Read : Target::Write);

        auto const stat = f(target);
        DUMP("got stat", stat);
        switch (stat) {
            case (std::is_const_v<T> ? Call::Read : Call::Write): return *reinterpret_cast<T *>(target.output());
            default: call_throw(std::move(target), stat);
        }
    }
};

/******************************************************************************/

template <>
struct Output<void, false> {
    template <class F>
    void operator()(F &&f) const {
        Target target(Index(), nullptr, 0, Target::None);

        auto const stat = f(target);
        DUMP("got stat", stat);
        switch (stat) {
            case Call::None: {return;}
            default: call_throw(std::move(target), stat);
        }
    }
};

template <>
struct Output<void, true> {
    template <class F>
    void operator()(F &&f) const {
        DUMP("calling something...");
        Target target(Index(), nullptr, 0, Target::None);

        auto const stat = f(target);
        DUMP("got stat", stat);
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
struct Output<Ref, true> {
    template <class F>
    Ref operator()(F &&f) const {
        DUMP("calling something...");
        Target target(Index(), nullptr, 0, Target::Read | Target::Write);
        auto const stat = f(target);

        switch (stat) {
            case Call::Read:   return Ref(target.index(), Mode::Read, Pointer::from(target.output()));
            case Call::Write: return Ref(target.index(), Mode::Write, Pointer::from(target.output()));
            // function is noexcept until here, now it is permitted to throw (I think)
            default: return nullptr;
        }
    }
};

/******************************************************************************/

namespace parts {

/******************************************************************************/

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
using shrink_const = std::conditional_t<std::is_trivially_copyable_v<T> && is_always_stackable<T>, T, T const &>;

template <class T>
struct Reduce<T const &> {
    using type = shrink_const<const_decay<T>>;
    static_assert(std::is_convertible_v<T, type>);
};

static_assert(std::is_same_v<typename Reduce< char const (&)[3] >::type, char const *>);
static_assert(std::is_same_v<typename Reduce< std::string const & >::type, std::string const &>);
static_assert(std::is_same_v<typename Reduce< void(double) >::type, void(*)(double)>);

/******************************************************************************/

template <int N, class F, class ...Ts>
decltype(auto) with_exact_args(F &&f, Context &c, Arg<Ts &&> ...ts) {
    static_assert(N <= sizeof...(Ts));
    ArgStack<N, sizeof...(Ts) - N> args(c, ts.ref()...);
    DUMP(type_name<F>(), " tags=", N, " args=", reinterpret_cast<ArgView &>(args).size());
    ((std::cout << type_name<Ts>() << std::endl), ...);
    return f(reinterpret_cast<ArgView &>(args));
}

template <int N, class F, class ...Ts>
decltype(auto) with_args(F &&f, Context &c, Ts &&...ts) {
    return with_exact_args<N, F, typename Reduce<Ts>::type...>(
        std::forward<F>(f), c, static_cast<Ts &&>(ts)...);
}

// template <class T, int N, class ...Ts>
// maybe<T> attempt_args(Index i, Pointer self, Mode mode, Context &c, Arg<Ts &&> ...ts) {
//     ArgStack<N, sizeof...(Ts) - N> args(c, ts.ref()...);
//     return Output<T>::attempt(i, self, mode, reinterpret_cast<ArgView &>(args));
// }

// template <class T, int N, class ...Ts>
// maybe<T> attempt(Index i, Pointer self, Mode mode, Context &c, Ts &&...ts) {
//     return attempt_args<T, N, typename Reduce<Ts>::type...>(i, self, mode, c, static_cast<Ts &&>(ts)...);
// }

/******************************************************************************/

}

/******************************************************************************/

}
