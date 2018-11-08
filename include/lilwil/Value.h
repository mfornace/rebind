#pragma once
#include <any>
#include <string>

namespace lilwil {

using Target = std::string;
using Conversion = Target(*)(std::any const &);

class Value {
    std::any val;
    Conversion conv;
public:
    Target convert() const {return conv(val);}
    std::any const & any() const {return val;}
};


}