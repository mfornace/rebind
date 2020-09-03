#pragma once
#include <ara/Call.h>
#include <ara/Structs.h>

namespace ara {

/******************************************************************************/

template <class F, class SFINAE=void>
struct ApplyCall;

/******************************************************************************/

template <class S>
struct AttributeFrame {
    Target& target;
    std::string_view name;
    Call::stat& stat;
    Pointer self;

    template <class Base>
    [[nodiscard]] bool derive() {return Impl<Base>::attribute(*this);}
};

/******************************************************************************/

template <class S>
struct ElementFrame {
    Target& target;
    Integer index;
    Call::stat& stat;
    Pointer self;

    template <class Base>
    [[nodiscard]] bool derive() {return Impl<Base>::element(*this);}
};

/******************************************************************************/

template <class ...Tags>
bool match_tags(ArgView& args, Tags const &...tags) {
    if (args.tags() != sizeof...(tags)) return false;
    return true;
}

/******************************************************************************/

struct Frame {
    Target& target;
    ArgView& args;
    Call::stat& stat;

    template <class F, class ...Tags>
    [[nodiscard]] bool operator()(F const functor, Tags const&... tags) {
        DUMP("MethodFrame::operator()", args.tags(), args.size());
        if (match_tags(args, tags...)) {
            stat = ApplyCall<F>::invoke(target, Lifetime(), functor, args);
            return true;
        } else return false;
    }

    template <class Base>
    [[nodiscard]] bool derive() {return Impl<Base>::call(*this);}
};

/******************************************************************************/

// This is the function context with arguments, output, output stat
// template <class S>
// struct MethodFrame {
//     Target& target;
//     ArgView& args;
//     Call::stat& stat;
//     Pointer self;

//     template <class F, class ...Tags>
//     [[nodiscard]] bool operator()(F const functor, Tags const&... tags) {
//         DUMP("MethodFrame::operator()", args.tags(), args.size());
//         if (match_tags(args, tags...)) {
//             stat = ApplyCall<F>::invoke(target, life, functor, *self, args);
//             return true;
//         } else return false;
//     }

//     template <class Base>
//     [[nodiscard]] bool derive() {
//         static_assert(!std::is_reference_v<Base>, "T should not be a reference type for MethodFrame::derive<T>()");
//         static_assert(std::is_convertible_v<S &&, Base>);
//         static_assert(!std::is_same_v<unqualified<S>, unqualified<Base>>);
//         return Impl<unqualified<Base>>::method(MethodFrame<Base>{target, args, stat, self});
//     }
// };

/******************************************************************************/

}
