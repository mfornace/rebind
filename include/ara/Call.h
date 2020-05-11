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
    ArgStack(Caller &c, Ts &&...ts) noexcept
        : ara_args{&c, Tags, Args}, refs{static_cast<Ts &&>(ts)...} {
        static_assert(Args + Tags == sizeof...(Ts));
        // Tags go at the back and they're reversed.
        // Possible to do at compile time...but sort of tedious.
        if constexpr(Tags > 0) {
            std::rotate(std::begin(refs), std::begin(refs) + Tags, std::end(refs));
            std::reverse(std::begin(refs) + Args, std::end(refs));
        }
    }
};

/******************************************************************************/

struct ArgView {
    ara_args c;

    Ref *begin() noexcept {return reinterpret_cast<ArgStack<1, 0> &>(c).refs;}
    auto size() const noexcept {return c.args;}
    Ref *end() noexcept {return begin() + size();}

    Caller &caller() const {return *static_cast<Caller *>(c.caller_ptr);}

    auto tags() const noexcept {return c.tags;}
    Ref &tag(unsigned int i) noexcept {return *(end() + tags() - i - 1);}

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

    Arg(T&& t) noexcept {allocate_in_place<T>(&storage, std::move(t));}
    Ref ref() noexcept {return Ref(Index::of<T>(), Tag::Stack, Pointer::from(&storage));}
};

[[noreturn]] void call_throw(Target &&target, Call::stat c);

/******************************************************************************/

template <class T>
struct CallReturn {
    static std::optional<T> get(Index i, Tag qualifier, Pointer self, ArgView &args) {
        std::aligned_union_t<0, T, void*> buffer;
        Target target(Index::of<T>(), &buffer, sizeof(buffer), Target::constraint<T>);
        auto const stat = Call::call(i, target, self, qualifier, args);

        std::optional<T> out;
        switch (stat) {
            case Call::Stack: {
                DestructGuard<T, false> raii{storage_cast<T>(buffer)};
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
    static T call(Index i, Tag qualifier, Pointer self, ArgView &args) {
        std::aligned_union_t<0, T, void*> buffer;
        Target target(Index::of<T>(), &buffer, sizeof(buffer), Target::constraint<T>);
        auto const stat = Call::call(i, target, self, qualifier, args);

        switch (stat) {
            case Call::Stack: {
                DestructGuard<T, false> raii{storage_cast<T>(buffer)};
                return std::move(raii.held);
            }
            default: call_throw(std::move(target), stat);
        }
    }
};

/******************************************************************************/

template <class T>
struct CallReturn<T &> {
    static T * get(Index i, Tag qualifier, Pointer self, ArgView &args) {
        DUMP("calling something that returns reference ...");
        Target target(Index::of<std::remove_cv_t<T>>(), nullptr, 0,
            std::is_const_v<T> ? Target::Const : Target::Mutable);

        auto const stat = Call::call(i, target, self, qualifier, args);
        DUMP("got stat", stat);
        switch (stat) {
            case (std::is_const_v<T> ? Call::Const : Call::Mutable): return *reinterpret_cast<T *>(target.output());
            case Call::Impossible:  {return nullptr;}
            case Call::WrongType:   {return nullptr;}
            case Call::WrongNumber: {return nullptr;}
            case Call::WrongReturn: {return nullptr;}
            default: call_throw(std::move(target), stat);
        }
    }

    static T & call(Index i, Tag qualifier, Pointer self, ArgView &args) {
        DUMP("calling something that returns reference ...");
        Target target(Index::of<std::remove_cv_t<T>>(), nullptr, 0,
            std::is_const_v<T> ? Target::Const : Target::Mutable);

        auto const stat = Call::call(i, target, self, qualifier, args);
        DUMP("got stat", stat);
        switch (stat) {
            case (std::is_const_v<T> ? Call::Const : Call::Mutable): return *reinterpret_cast<T *>(target.output());
            default: call_throw(std::move(target), stat);
        }
    }
};

/******************************************************************************/

template <>
struct CallReturn<void> {
    static void call(Index i, Tag qualifier, Pointer self, ArgView &args) {
        DUMP("calling something...", args.size());
        Target target(Index(), nullptr, 0, Target::None);

        auto const stat = Call::call(i, target, self, qualifier, args);
        DUMP("got stat", stat);
        switch (stat) {
            case Call::None: {return;}
            default: call_throw(std::move(target), stat);
        }
    }

    static void get(Index i, Tag qualifier, Pointer self, ArgView &args) {
        DUMP("calling something...");
        Target target(Index(), nullptr, 0, Target::None);

        auto const stat = Call::call(i, target, self, qualifier, args);
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
struct CallReturn<Ref> {
    static Ref call(Index i, Tag qualifier, Pointer self, ArgView &args) {
        DUMP("calling something...");
        Target target(Index(), nullptr, 0, Target::Const | Target::Mutable);
        auto stat = Call::call(i, target, self, qualifier, args);

        switch (stat) {
            case Call::Const:   return Ref(target.index(), Tag::Const, Pointer::from(target.output()));
            case Call::Mutable: return Ref(target.index(), Tag::Mutable, Pointer::from(target.output()));
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

template <class T, int N, class ...Ts>
T call_args(Index i, Tag qualifier, Pointer self, Caller &c, Arg<Ts &&> ...ts) {
    static_assert(N <= sizeof...(Ts));
    ArgStack<N, sizeof...(Ts) - N> args(c, ts.ref()...);
    DUMP(type_name<T>(), " tags=", N, " args=", reinterpret_cast<ArgView &>(args).size());
    ((std::cout << type_name<Ts>() << std::endl), ...);
    return CallReturn<T>::call(i, qualifier, self, reinterpret_cast<ArgView &>(args));
}

template <class T, int N, class ...Ts>
T call(Index i, Tag qualifier, Pointer self, Caller &c, Ts &&...ts) {
    return call_args<T, N, typename Reduce<Ts>::type...>(i, qualifier, self, c, static_cast<Ts &&>(ts)...);
}

template <class T, int N, class ...Ts>
maybe<T> get_args(Index i, Tag qualifier, Pointer self, Caller &c, Arg<Ts &&> ...ts) {
    ArgStack<N, sizeof...(Ts) - N> args(c, ts.ref()...);
    return CallReturn<T>::get(i, qualifier, self, reinterpret_cast<ArgView &>(args));
}

template <class T, int N, class ...Ts>
maybe<T> get(Index i, Tag qualifier, Pointer self, Caller &c, Ts &&...ts) {
    return get_args<T, N, typename Reduce<Ts>::type...>(i, qualifier, self, c, static_cast<Ts &&>(ts)...);
}

/******************************************************************************/

}

/******************************************************************************/

}
