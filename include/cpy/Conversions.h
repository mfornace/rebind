#pragma once
#include "Variable.h"

namespace cpy {

/******************************************************************************/

template <class T, class=void>
struct ValueResponse {
    void operator()(Variable &out, T const &t, std::type_index) const {
        if (Debug) std::cout << "    - default simplifyvalue " << typeid(T).name() << std::endl;
        out = t;
        if (Debug) std::cout << "    - default simplifyvalue " << typeid(T).name() << " 2" << std::endl;
    }
};

template <class T>
struct ValueResponse<T, std::void_t<decltype(response(std::declval<T const &>(), std::declval<std::type_index &&>()))>> {
    void operator()(Variable &out, T const &t, std::type_index idx) const {
        if (Debug) std::cout << "    - adl simplifyvalue" << typeid(T).name() << std::endl;
        out = response(t, std::move(idx));
        if (Debug) std::cout << "    - adl simplifyvalue" << typeid(T).name() << " 2" << std::endl;
    }
};

template <class T, class=void>
struct ReferenceResponse {
    using custom = std::false_type;
    void operator()(Variable &out, T const &t, std::type_index i, Qualifier q) const {
        if (Debug) std::cout << "    - no conversion for reference " << typeid(T).name() << " "
            << int(static_cast<unsigned char>(q)) << " " << i.name() << std::endl;
    }
};

template <class T>
struct ReferenceResponse<T, std::void_t<decltype(response(std::declval<T const &>(), std::declval<std::type_index &&>(), cvalue()))>> {
    using custom = std::true_type;
    template <class Q>
    void operator()(Variable &out, qualified<T, Q> t, std::type_index idx, Q q) const {
        if (Debug) std::cout << "    - convert reference via ADL " << typeid(T).name() << std::endl;
        out = response(static_cast<decltype(t) &&>(t), std::move(idx), q);
    }
};


template <class T, class>
struct Response : ValueResponse<T>, ReferenceResponse<T> {
    static_assert(std::is_same_v<no_qualifier<T>, T>);
};

/******************************************************************************/

/// The 4 default behaviors for casting a reference to an expected type
template <class T, class>
struct Request {
    static_assert(std::is_same_v<no_qualifier<T>, T>);

    std::optional<T> operator()(Variable const &r, Dispatch &msg) const {
        return msg.error("mismatched class type", r.type(), typeid(T));
    }
};

template <class T, class C>
struct Request<T &, C> {
    T * operator()(Variable const &r, Dispatch &msg) const {
        return msg.error("could not bind to lvalue reference", r.type(), typeid(T)), nullptr;
    }
};

template <class T, class C>
struct Request<T const &, C> {
    T const * operator()(Variable const &v, Dispatch &msg) const {
        if (Debug) std::cout << "    - trying & -> const & " << typeid(T).name() << std::endl;
        if (auto p = v.request<T &>(msg)) return p;
        if (Debug) std::cout << "    - trying temporary const & storage " << typeid(T).name() << std::endl;
        if (auto p = v.request<T>(msg)) return msg.store(std::move(*p));
        return msg.error("could not bind to const lvalue reference", v.type(), typeid(T)), nullptr;
    }
};

template <class T, class C>
struct Request<T &&, C> {
    T * operator()(Variable const &v, Dispatch &msg) const {
        if (Debug) std::cout << "    - trying temporary && storage " << typeid(T).name() << std::endl;
        if (auto p = v.request<T>(msg)) return msg.store(std::move(*p));
        return msg.error("could not bind to rvalue reference", v.type(), typeid(T)), nullptr;
    }
};


void request(int, int, int);

/// ADL version
template <class T>
struct Request<T, std::void_t<decltype(request(Type<T>(), std::declval<Variable const &>(), std::declval<Dispatch &>()))>> {

    std::optional<T> operator()(Variable const &r, Dispatch &msg) const {
        return static_cast<std::optional<T>>(request(Type<T>(), r, msg));
    }
};

/******************************************************************************/

}