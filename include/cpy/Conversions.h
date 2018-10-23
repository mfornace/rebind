#pragma once
#include "Value.h"

namespace cpy {

/******************************************************************************/

template <class T, class=void>
struct SimplifyValue {
    void operator()(Value &out, T const &t, std::type_index) const {
        if (Debug) std::cout << "    - default simplifyvalue " << typeid(T).name() << std::endl;
        out = t;
        if (Debug) std::cout << "    - default simplifyvalue " << typeid(T).name() << " 2" << std::endl;
    }
};

template <class T>
struct SimplifyValue<T, std::void_t<decltype(simplify(std::declval<T const &>(), std::declval<std::type_index &&>()))>> {
    void operator()(Value &out, T const &t, std::type_index idx) const {
        if (Debug) std::cout << "    - adl simplifyvalue" << typeid(T).name() << std::endl;
        out = simplify(t, std::move(idx));
        if (Debug) std::cout << "    - adl simplifyvalue" << typeid(T).name() << " 2" << std::endl;
    }
};

template <class T, class=void>
struct SimplifyReference {
    using custom = std::false_type;
    void * operator()(Qualifier q, T const &t, std::type_index i) const {
        if (Debug) std::cout << "    - no conversion for reference " << typeid(T).name() << " "
            << int(static_cast<unsigned char>(q)) << " " << i.name() << std::endl;
        return nullptr;
    }
};

template <class T>
struct SimplifyReference<T, std::void_t<decltype(simplify(cvalue(), std::declval<T const &>(), std::declval<std::type_index &&>()))>> {
    using custom = std::true_type;
    template <class Q>
    void * operator()(Q q, qualified<T, Q> t, std::type_index idx) const {
        if (Debug) std::cout << "    - convert reference via ADL " << typeid(T).name() << std::endl;
        return const_cast<void *>(static_cast<void const *>(simplify(q, static_cast<decltype(t) &&>(t), std::move(idx))));
    }
};


template <class T, class>
struct Simplify : SimplifyValue<T>, SimplifyReference<T> {
    static_assert(std::is_same_v<no_qualifier<T>, T>);
};

/******************************************************************************/

/// The 4 default behaviors for casting a reference to an expected type
template <class T, class>
struct Request {
    static_assert(std::is_same_v<no_qualifier<T>, T>);

    T operator()(Reference const &r, Dispatch &msg) const {
        auto v = r.request(typeid(T));
        if (auto p = v.target<T>()) return static_cast<T &&>(*p);
        throw msg.error("mismatched class type", r.type(), typeid(T));
    }
};

template <class T, class C>
struct Request<T &, C> {
    T & operator()(Reference const &r, Dispatch &msg) const {
        throw msg.error("could not bind to lvalue reference", r.type(), typeid(T));
    }
};

template <class T, class C>
struct Request<T const &, C> {
    T const & operator()(Reference const &r, Dispatch &msg) const {
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
    T && operator()(Reference const &r, Dispatch &msg) const {
        if (Debug) std::cout << "    - trying temporary && storage " << typeid(T).name() << std::endl;
        return static_cast<T &&>(msg.storage.emplace_back().emplace<T>(Request<T>()(r, msg)));
    }
};


void from_reference(int, int, int);

/// ADL version
template <class T>
struct Request<T, std::void_t<decltype(
    from_reference(Type<T>(), std::declval<Reference const &>(), std::declval<Dispatch &>()))>> {

    T operator()(Reference const &r, Dispatch &msg) const {
        return static_cast<T>(from_reference(Type<T>(), r, msg));
    }
};

/******************************************************************************/

}