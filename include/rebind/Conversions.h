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

/// The default behavior has no custom conversions
template <class T, class SFINAE>
struct Request<T &, SFINAE> {
    using method = Default;

    T * operator()(Pointer const &r, Scope &msg) const {
        // lvalue_fails(v, msg, typeid(T));
        return nullptr;
    }

    constexpr bool operator()(Pointer const &r) const {return false;}
};

/// The default behavior tries the T -> T const & and T & -> T const & routes
template <class T, class SFINAE>
struct Request<T const &, SFINAE> {
    using method = Default;

    T const * operator()(Pointer const &r, Scope &msg) const {
        DUMP("trying & -> const & ", typeid(T).name());
        if (auto p = r.request<T &>(msg)) return p;
        // DUMP("trying temporary const & storage ", typeid(T).name());
        // if (auto p = v.request<T>(msg)) return msg.store(std::move(*p));
        return msg.error("could not bind to const lvalue reference", typeid(T)), nullptr;
    }

    constexpr bool operator()(Pointer const &r) const {return false;}
};

/// The default behavior tries the T -> T && route
template <class T, class SFINAE>
struct Request<T &&, SFINAE> {
    using method = Default;

    T * operator()(Pointer const &r, Scope &msg) const {
        // DUMP("trying temporary && storage ", typeid(T).name());
        // if (auto p = v.request<T>(msg)) return msg.store(std::move(*p));
        // rvalue_fails(v, msg, typeid(T));
        return nullptr;
    }

    constexpr bool operator()(Pointer const &r) const {return false;}
};

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
        return {};
        // DUMP("no conversion found from source", TypeIndex(typeid(T), Q), "to", idx);
        // return implicit_response(out, idx, Q, static_cast<T2 &&>(t));
    }
};

/// Default response just tries implicit conversions
template <class T, class SFINAE>
struct RefResponse {
    using method = Default;
    static_assert(std::is_same_v<unqualified<T>, T>);

    Pointer operator()(TypeIndex const &idx, Qualifier, T const &) const {
        return {};
        // DUMP("no conversion found from source", TypeIndex(typeid(T), Q), "to", idx);
        // return implicit_response(out, idx, Q, static_cast<T2 &&>(t));
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

    // template <class T2>
    Pointer operator()(TypeIndex const &idx, T const &t) const {
        return {};
        // DUMP("ADL Response", typeid(T), idx);
        // out = response(idx, static_cast<T2 &&>(t));
        // return out || implicit_response(out, idx, Q, static_cast<T2 &&>(t));
    }
};

/******************************************************************************/

// void lvalue_fails(Variable const &, Scope &, TypeIndex);
// void rvalue_fails(Variable const &, Scope &, TypeIndex);

/******************************************************************************/

}
