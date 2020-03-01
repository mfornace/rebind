#pragma once
#include "Value.h"

namespace rebind {

/******************************************************************************/

/// Tags for method of Response or Request specialization
struct Default {};
struct Specialized {};
struct ADL {};

/******************************************************************************/

/// The default behavior has no custom conversions
template <class T, class SFINAE=void>
struct Request {
    static_assert(std::is_same_v<unqualified<T>, T>);
    using method = Default;

    std::optional<T> operator()(Pointer const &r, Scope &msg) const {
        return msg.error("mismatched class type", typeid(T));
    }

    constexpr bool operator()(Pointer const &r) const {return false;}
};

/******************************************************************************/

template <class T, std::enable_if_t<!std::is_reference_v<T>, int>>
std::optional<T> Pointer::request(Scope &s, Type<T> t) const {
    std::optional<T> out;
    if (has_value()) {
        if (auto p = request_reference<T &&>(qual)) out.emplace(std::move(*p));
        else if (auto p = request_reference<T const &>(qual)) out.emplace(*p);
        else {
            out = Request<T>()(static_cast<Pointer const &>(*this), s);
        }
    }
    return out;
}

/******************************************************************************/

template <class T, class=void>
struct RequestMethod {using method = Specialized;};

template <class T>
struct RequestMethod<T, std::void_t<typename Request<T>::method>> {using type = typename Request<T>::method;};

template <class T>
using request_method = typename RequestMethod<T>::type;

/******************************************************************************/

/// Unused overload just to register the request identifier
void request(int, int, int);

/// ADL version of Request
template <class T>
struct Request<T, std::void_t<decltype(request(Type<T>(), std::declval<Pointer const &>(), std::declval<Scope &>()))>> {
    using method = ADL;

    std::optional<T> operator()(Pointer const &r, Scope &msg) const {
        return static_cast<std::optional<T>>(request(Type<T>(), r, msg));
    }
};

/******************************************************************************/

/// Default response just tries implicit conversions
template <class T, class SFINAE=void>
struct Response {
    using method = Default;
    static_assert(std::is_same_v<unqualified<T>, T>);

    Value operator()(TypeIndex const &idx, T const &) const {
        DUMP("no conversion found from source", type_index<T>(), "to", idx);
        return {};
    }
};

/******************************************************************************/

template <class T, class=void>
struct ResponseMethod {using type = Specialized;};

template <class T>
struct ResponseMethod<T, std::void_t<typename Response<T>::method>> {using type = typename Response<T>::method;};

template <class T>
using response_method = typename ResponseMethod<T>::type;

/******************************************************************************/

template <class T>
struct Response<T, std::void_t<decltype(response(std::declval<TypeIndex>(), std::declval<T>()))>> {
    static_assert(std::is_same_v<unqualified<T>, T>);
    using method = ADL;

    Pointer operator()(TypeIndex const &idx, T &t) const {return response(idx, t);}
    Pointer operator()(TypeIndex const &idx, T const &t) const {return response(idx, t);}
    Pointer operator()(TypeIndex const &idx, T &&t) const {return response(idx, std::move(t));}
};

/******************************************************************************/

// void lvalue_fails(Variable const &, Scope &, TypeIndex);
// void rvalue_fails(Variable const &, Scope &, TypeIndex);

/******************************************************************************/

}
