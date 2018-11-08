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
    void * operator()(Qualifier q, T const &t, std::type_index i) const {
        if (Debug) std::cout << "    - no conversion for reference " << typeid(T).name() << " "
            << int(static_cast<unsigned char>(q)) << " " << i.name() << std::endl;
        return nullptr;
    }
};

template <class T>
struct ReferenceResponse<T, std::void_t<decltype(response(std::declval<T const &>(), std::declval<std::type_index &&>(), cvalue()))>> {
    using custom = std::true_type;
    template <class Q>
    void * operator()(Q q, qualified<T, Q> t, std::type_index idx) const {
        if (Debug) std::cout << "    - convert reference via ADL " << typeid(T).name() << std::endl;
        return const_cast<void *>(static_cast<void const *>(response(static_cast<decltype(t) &&>(t), std::move(idx), q)));
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

    T operator()(Variable const &r, Dispatch &msg) const {
        if (auto v = r.request<T>()) return static_cast<T &&>(*v);
        throw msg.error("mismatched class type", r.type(), typeid(T));
    }
};

template <class T, class C>
struct Request<T &, C> {
    T & operator()(Variable const &r, Dispatch &msg) const {
        throw msg.error("could not bind to lvalue reference", r.type(), typeid(T));
    }
};

template <class T, class C>
struct Request<T const &, C> {
    T const & operator()(Variable const &r, Dispatch &msg) const {
        try {
            if (Debug) std::cout << "    - trying & -> const & " << typeid(T).name() << std::endl;
            return Request<T &>()(r, msg);
        } catch (DispatchError const &) {
            if (Debug) std::cout << "    - trying temporary const & storage " << typeid(T).name() << std::endl;
            return msg.storage.emplace_back().emplace<T>(Request<T>()(r, msg));
        }
    }
};

template <class T, class C>
struct Request<T &&, C> {
    T && operator()(Variable const &r, Dispatch &msg) const {
        if (Debug) std::cout << "    - trying temporary && storage " << typeid(T).name() << std::endl;
        return static_cast<T &&>(msg.storage.emplace_back().emplace<T>(Request<T>()(r, msg)));
    }
};


void request(int, int, int);

/// ADL version
template <class T>
struct Request<T, std::void_t<decltype(
    request(Type<T>(), std::declval<Variable const &>(), std::declval<Dispatch &>()))>> {

    T operator()(Variable const &r, Dispatch &msg) const {
        return static_cast<T>(request(Type<T>(), r, msg));
    }
};

/******************************************************************************/

}