#pragma once
#include <sfb/Call.h>
#include <sfb/Structs.h>

namespace sfb {

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

template <class Tag>
bool match_tag(Tag const& tag, Ref& ref) {
    if (auto t = ref.get<Tag>()) return *t == tag;
    return false;
}

inline bool match_tag(char const *tag, Ref& ref) {
    if (auto t = ref.get<Str>()) {
        return std::string_view(*t) == std::string_view(tag);
    }
    return false;
}

template <std::size_t ...Is, class ...Tags>
bool match_tags_impl(std::index_sequence<Is...>, ArgView& args, Tags const& ...tags) {
    return (match_tag(tags, args.tag(Is)) && ...);
}

template <class ...Tags>
bool match_tags(ArgView& args, Tags const& ...tags) {
    if (args.tags() != sizeof...(tags)) return false;
    return match_tags_impl(std::make_index_sequence<sizeof...(Tags)>(), args, tags...);
}

/******************************************************************************/

struct Frame {
    Target& target;
    ArgView& args;
    Call::stat& stat;

    template <class F, class ...Tags>
    [[nodiscard]] bool operator()(F const functor, Tags const&... tags) {
        DUMP("Frame::operator()", args.tags(), args.size());
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
