#pragma once
#include <any>
#include <string>

namespace lilwil {

using Target = std::string;
using Conversion = Target(*)(std::any const &);

/******************************************************************************/

template <class T, class SFINAE=void>
struct ToTarget {
    Target operator()(T const &) const {return "unknown";}
};

template <class T, class SFINAE=void>
struct FromTarget {
    T operator()(std::any const &a) const {throw std::runtime_error("bad");}
};

template <class T>
Target to_target(std::any const &a) {return ToTarget<T>()(std::any_cast<T>(a));};

/******************************************************************************/

class Value {
    std::any val;
    Conversion conv = nullptr;
public:
    Target convert() const {return conv(val);}
    std::any const & any() const {return val;}

    template <class T>
    Value(T t) : val(std::move(t)), conv(to_target<T>) {}

    template <class T>
    T target() const {
        if (auto p = std::any_cast<T>(&val)) return *p;
        return FromTarget<T>()(val);
    }

    Value() = default;
};

/******************************************************************************/

using ArgPack = Vector<Value>;

}