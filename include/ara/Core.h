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
struct Impl<Str> : Default<Str> {
    static bool dump(Target &v, Str s) {
        if (v.accepts<String>()) return v.emplace<String>(s);
        return false;
    };
};


template <>
struct Impl<Bin> : Default<Bin> {
    static bool dump(Target &v, Bin s) {
        if (v.accepts<Binary>()) return v.emplace<Binary>(s);
        return false;
    };
};

// We don't need Loadable<Str> since C++ functionality already covered in Dumpable

/******************************************************************************/

template <class S, class T>
struct DumpableString {
    static bool dump(Target &v, S const &s) {return false;};
    static bool dump(Target &v, S &&s) {return false;};
};

template <>
struct Impl<String> : Default<String>, DumpableString<String, char> {};

template <>
struct Impl<Binary> : Default<Binary>, DumpableString<Binary, unsigned char> {};

// We don't need Loadable<String> since C++ functionality already covered in Dumpable

/******************************************************************************/

// Not sure about the wisdom of including these...?
template <>
struct Impl<char const *> : Default<char const *> {
    static bool dump(Target &v, char const *s) {
        if (v.accepts<Str>())
            return v.emplace<Str>(s ? Str(s) : Str());

        if (v.accepts<String>()) {
            if (s) return v.emplace<String>(s);
            return v.emplace<String>();
        }
        return false;
    }

    static std::optional<char const *> load(Ref &v) {
        DUMP("loading char const *");
        std::optional<char const *> out;
        if (!v) out.emplace(nullptr);
        else if (auto p = v.load<Str>()) out.emplace(p->data());
        // else if (auto p = v.load<char const &>()) out.emplace(std::addressof(*p));
        return out;
    }
};

/******************************************************************************/

template <class T>
struct Impl<T *> : Default<T *> {
    static std::optional<T *> load(Ref &v) {
        std::optional<T *> out;
        if (!v) {
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
//         // if (v.qualifier() != Read) out.emplace(v.address());
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

template <class T>
struct Impl<T, std::enable_if_t<(std::is_floating_point_v<T>)>> : Default<T> {
    /// Default Dumpable for floating point allows conversion to Float or Integer
    static bool dump(Target &v, T t) {
        if (v.accepts<Float>()) return v.emplace<Float>(t);
        if (v.accepts<Integer>()) return v.emplace<Integer>(t);
        return false;
    }

    /*
    Default Loadable for integer type tries to go through double precision
    long double is not expected to be a useful route (it's assumed there are not multiple floating types larger than Float)
    */
    static std::optional<T> load(Ref &v) {
        DUMP("convert to floating");
        if (!std::is_same_v<Float, T>) if (auto p = v.load<Float>()) return static_cast<T>(*p);
        return {}; //s.error("not convertible to floating point", Index::of<T>());
    }
};

/******************************************************************************/

template <>
struct Impl<bool> : Default<bool> {
    static bool dump(Target &v, bool t) {
        DUMP("Dumpable <bool> to ", v.name());
        if (v.accepts<Bool>()) return v.emplace<Bool>(t);
        if (v.accepts<Integer>()) return v.emplace<Integer>(t);
        DUMP("Dumpable <bool> to ", v.name(), " failed");
        return false;
    }

    /// Default Loadable for integer type tries to go through Integer
    static std::optional<bool> load(Ref &v) {
        DUMP("trying convert to bool ", v.name());
        if (auto p = v.load<Bool>()) return static_cast<bool>(p->value);
        if (auto p = v.load<Integer>()) return static_cast<bool>(*p);
        DUMP("failed to convert to bool", v.name());
        return {}; //s.error("not convertible to integer", Index::of<T>());
    }
};

/******************************************************************************/

template <class T>
struct Impl<T, std::enable_if_t<(std::is_integral_v<T>)>> : Default<T> {
    static bool dump(Target &v, T t) {
        DUMP("Dumpable<", type_name<T>(), "> to ", v.name());
        if (v.accepts<Integer>()) return v.emplace<Integer>(t);
        if (v.accepts<Float>()) return v.emplace<Float>(t);
        DUMP("Dumpable<", type_name<T>(), "> to ", v.name(), " failed");
        return false;
    }

    /// Default Loadable for integer type tries to go through Integer
    static std::optional<T> load(Ref &v) {
        DUMP("trying convert to integer ", v.name(), Index::of<T>());
        if (!std::is_same_v<Integer, T>) if (auto p = v.load<Integer>()) return static_cast<T>(*p);
        DUMP("failed to convert to integer", v.name(), Index::of<T>());
        return {}; //s.error("not convertible to integer", Index::of<T>());
    }
};

/******************************************************************************/


/// Default Dumpable for enum permits conversion to integer types
template <class T>
struct Impl<T, std::enable_if_t<(std::is_enum_v<T>)>> {
    static bool dump(Target &v, T t) {
        if (v.accepts<std::underlying_type_t<T>>())
            return v.emplace<std::underlying_type_t<T>>(t);
        if (v.accepts<Integer>())
            return v.emplace<Integer>(t);
        return false;
    }

    // Default Loadable for enum permits conversion from integer types
    static std::optional<T> load(Ref &v) {
        DUMP("trying convert to enum", v.name(), Index::of<T>());
        if (auto p = v.load<std::underlying_type_t<T>>()) return static_cast<T>(*p);
        return {}; //s.error("not convertible to enum", Index::of<T>());
    }
};

/******************************************************************************/

}

/******************************************************************************/

ARA_DECLARE(void, void);
ARA_DECLARE(cpp_bool, bool);
ARA_DECLARE(bool, ara_bool);
ARA_DECLARE(char, char);
ARA_DECLARE(uchar, unsigned char);
ARA_DECLARE(int, int);
ARA_DECLARE(long, long);
ARA_DECLARE(longlong, long long);
ARA_DECLARE(ulonglong, unsigned long long);
ARA_DECLARE(unsigned, unsigned);
ARA_DECLARE(float, float);
ARA_DECLARE(double, double);