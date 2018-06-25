#pragma once

#include "Test.h"

namespace cpy {

/******************************************************************************/

using Suite = std::vector<TestCase>;

Suite & suite();


struct Timer {
    double start;
    double &duration;
    Timer(double &d) : start(current_time()), duration(d) {}
    ~Timer() {duration = current_time() - start;}
};

/******************************************************************************/

}
