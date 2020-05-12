#pragma once
#include "Impl.h"
#include "Ref.h"
#include "Structs.h"
#include <cstdlib>
#include <tuple>
#include <vector>

namespace ara {

/******************************************************************************/

template <>
struct Dumpable<Str> {
    bool operator()(Target &v, Str s) const {
        if (v.accepts<String>()) return v.emplace_if<String>(s);
        return false;
    };
};


template <>
struct Dumpable<Bin> {
    bool operator()(Target &v, Bin s) const {
        if (v.accepts<Binary>()) return v.emplace_if<Binary>(s);
        return false;
    };
};

// We don't need Loadable<Str> since C++ functionality already covered in Dumpable

/******************************************************************************/

template <class S, class T>
struct DumpableString {
    bool operator()(Target &v, S const &s) const {return false;};
    bool operator()(Target &v, S &&s) const {return false;};
};

template <>
struct Dumpable<String> : DumpableString<String, char> {};

template <>
struct Dumpable<Binary> : DumpableString<Binary, unsigned char> {};

// We don't need Loadable<String> since C++ functionality already covered in Dumpable

/******************************************************************************/

// Not sure about the wisdom of including these...?
template <>
struct Dumpable<char const *> {
    bool operator()(Target &v, char const *s) const {
        if (v.accepts<Str>())
            return v.set_if(s ? Str(s) : Str());

        if (v.accepts<String>()) {
            if (s) return v.emplace_if<String>(s);
            return v.emplace_if<String>();
        }
        return false;
    }
};

template <>
struct Loadable<char const *> {
    std::optional<char const *> operator()(Ref &v) const {
        DUMP("loading char const *");
        std::optional<char const *> out;
        if (!v || v.load<std::nullptr_t>()) out.emplace(nullptr);
        else if (auto p = v.load<Str>()) out.emplace(p->data());
        // else if (auto p = v.load<char const &>()) out.emplace(std::addressof(*p));
        return out;
    }
};

template <class T>
struct Loadable<T *> {
    std::optional<T *> operator()(Ref &v) const {
        std::optional<T *> out;
        if (!v || v.load<std::nullptr_t>()) {
            out.emplace(nullptr);
        } else {
            if constexpr(!std::is_function_v<T>) {
                if (auto p = v.load<T &>()) out.emplace(std::addressof(*p));
            }
        }
        return out;
    }
};

// template <>
// struct Loadable<void *> {
//     std::optional<void *> operator()(Ref &v) const {
//         std::optional<void *> out;
//         // if (v.qualifier() != Const) out.emplace(v.address());
//         return out;
//     }
// };

// template <>
// struct Loadable<void const *> {
//     std::optional<void const *> operator()(Ref &v) const {
//         // return v.address();
//         return {};
//     }
// };

/******************************************************************************/

/// Default Dumpable for floating point allows conversion to Float or Integer
template <class T>
struct Dumpable<T, std::enable_if_t<(std::is_floating_point_v<T>)>> {
    bool operator()(Target &v, T t) const {
        if (v.accepts<Float>()) return v.emplace_if<Float>(t);
        if (v.accepts<Integer>()) return v.emplace_if<Integer>(t);
        return false;
    }
};

/*
Default Loadable for integer type tries to go through double precision
long double is not expected to be a useful route (it's assumed there are not multiple floating types larger than Float)
*/
template <class T>
struct Loadable<T, std::enable_if_t<std::is_floating_point_v<T>>> {
    std::optional<T> operator()(Ref &v) const {
        DUMP("convert to floating");
        if (!std::is_same_v<Float, T>) if (auto p = v.load<Float>()) return static_cast<T>(*p);
        return {}; //s.error("not convertible to floating point", Index::of<T>());
    }
};

/******************************************************************************/

template <class T>
struct Dumpable<T, std::enable_if_t<(std::is_integral_v<T>)>> {
    bool operator()(Target &v, T t) const {
        DUMP("Dumpable ", type_name<T>(), " to ", v.name());
        if (v.accepts<Integer>()) return v.emplace_if<Integer>(t);
        if (v.accepts<Float>()) return v.emplace_if<Float>(t);
        DUMP("Dumpable ", type_name<T>(), " to ", v.name(), " failed");
        return false;
    }
};

/*
Default Loadable for integer type tries to go through Integer
*/
template <class T>
struct Loadable<T, std::enable_if_t<std::is_integral_v<T>>> {
    std::optional<T> operator()(Ref &v) const {
        DUMP("trying convert to integer ", v.name(), Index::of<T>());
        if (!std::is_same_v<Integer, T>) if (auto p = v.load<Integer>()) return static_cast<T>(*p);
        DUMP("failed to convert to integer", v.name(), Index::of<T>());
        return {}; //s.error("not convertible to integer", Index::of<T>());
    }
};

/******************************************************************************/


/// Default Dumpable for enum permits conversion to integer types
template <class T>
struct Dumpable<T, std::enable_if_t<(std::is_enum_v<T>)>> {
    bool operator()(Target &v, T t) const {
        if (v.accepts<std::underlying_type_t<T>>())
            return v.emplace_if<std::underlying_type_t<T>>(t);
        if (v.accepts<Integer>())
            return v.emplace_if<Integer>(t);
        return false;
    }
};

/*
Default Loadable for enum permits conversion from integer types
*/
template <class T>
struct Loadable<T, std::enable_if_t<std::is_enum_v<T>>> {
    std::optional<T> operator()(Ref &v) const {
        DUMP("trying convert to enum", v.name(), Index::of<T>());
        if (auto p = v.load<std::underlying_type_t<T>>()) return static_cast<T>(*p);
        return {}; //s.error("not convertible to enum", Index::of<T>());
    }
};

/******************************************************************************/

}

/******************************************************************************/

ARA_DECLARE(void, void);
ARA_DECLARE(bool, bool);
ARA_DECLARE(char, char);
ARA_DECLARE(uchar, unsigned char);
ARA_DECLARE(int, int);
ARA_DECLARE(long, long);
ARA_DECLARE(longlong, long long);
ARA_DECLARE(ulonglong, unsigned long long);
ARA_DECLARE(unsigned, unsigned);
ARA_DECLARE(float, float);
ARA_DECLARE(double, double);