#pragma once

namespace ara {


template <class T, class=void>
struct ImplicitConversions {
    using types = Pack<>;
};

template <class U, class T>
bool implicit_match(Variable &out, Type<U>, TargetQualifier const q, T &&t) {
    DUMP("implicit_match", typeid(U).name(), typeid(Type<T &&>).name(), q);
    if constexpr(std::is_convertible_v<T &&, U>)
        if (q == Value) out = {Type<U>(), static_cast<T &&>(t)};
    if constexpr(std::is_convertible_v<T &&, U &>)
        if (q == Lvalue) out = {Type<U &>(), static_cast<T &&>(t)};
    if constexpr(std::is_convertible_v<T &&, U &&>)
        if (q == Rvalue) out = {Type<U &&>(), static_cast<T &&>(t)};
    if constexpr(std::is_convertible_v<T &&, U const &>)
        if (q == Const) out = {Type<U const &>(), static_cast<T &&>(t)};
    DUMP("implicit_response result ", out.has_value(), typeid(Type<T &&>).name(), typeid(U).name(), q);
    return out.has_value();
}

template <class U, class T>
bool recurse_implicit(Variable &out, Type<U>, Index const &idx, TargetQualifier q, T &&t);

/******************************************************************************/

template <class T>
bool implicit_response(Variable &out, Index const &idx, TargetQualifier q, T &&t) {
    DUMP("implicit_response", typeid(Type<T &&>).name(), idx.name(), typeid(typename ImplicitConversions<std::decay_t<T>>::types).name(), q);
    return ImplicitConversions<std::decay_t<T>>::types::apply([&](auto ...ts) {
        static_assert((!decltype(is_same(+Type<T>(), +ts))::value && ...), "Implicit conversion creates a cycle");
        return ((idx.matches(ts) && implicit_match(out, ts, q, static_cast<T &&>(t))) || ...)
            || (recurse_implicit(out, ts, idx, q, static_cast<T &&>(t)) || ...);
    });
}

template <class U, class T>
bool recurse_implicit(Variable &out, Type<U>, Index const &idx, TargetQualifier q, T &&t) {
    if constexpr(std::is_convertible_v<T &&, U &&>)
        return implicit_response(out, idx, q, static_cast<U &&>(t));
    else if constexpr(std::is_convertible_v<T &&, U &>)
        return implicit_response(out, idx, q, static_cast<U &>(t));
    else if constexpr(std::is_convertible_v<T &&, U const &>)
        return implicit_response(out, idx, q, static_cast<U const &>(t));
    else if constexpr(std::is_convertible_v<T &&, U>)
        return implicit_response(out, idx, q, static_cast<U>(t));
    return false;
}


}