#pragma once
#include "Pointer.h"
#include "Error.h"
#include <optional>

namespace rebind {

/******************************************************************************/

class Value : protected Opaque {
    using Opaque::Opaque;
public:

    using Opaque::name;
    using Opaque::index;
    using Opaque::has_value;
    using Opaque::operator bool;

    constexpr Value() noexcept = default;

    Value(Value const &v) : Opaque(v.allocate_copy()) {}

    Value(Value &&v) noexcept : Opaque(static_cast<Opaque &&>(v)) {v.reset();}

    Value &operator=(Value const &v);

    Value &operator=(Value &&v);

    template <class T>
    Value(T t) noexcept : Opaque(new T(std::move(t))) {}

    template <class T, class ...Args>
    T &emplace(Type<T>, Args &&...args) {return *new T(static_cast<Args &&>(args)...);}

    template <class T>
    T *target() &;

    template <class T>
    T *target() &&;

    template <class T>
    T const *target() const &;

    template <class T, class ...Args>
    static Value from(Args &&...args) {
        static_assert(std::is_constructible_v<T, Args &&...>);
        return Value(new T{static_cast<Args &&>(args)...});
    }

    template <class T>
    std::optional<T> request() const & {
        std::optional<T> out;
        if (has_value()) if (auto p = Opaque::request_value<T>(Const)) {out.emplace(std::move(*p)); delete p;}
        return out;
    }

    template <class T>
    std::optional<T> request() && {
        std::optional<T> out;
        if (has_value()) if (auto p = Opaque::request_value<T>(Rvalue)) {out.emplace(std::move(*p)); delete p;}
        return out;
    }

    ~Value() {Opaque::try_destroy();}
};

/******************************************************************************/

}