#pragma once

#include "Test.h"

namespace cpy {

/******************************************************************************/

using Suite = std::vector<TestCase>;

Suite & suite();

struct Timer {
    Clock::time_point start;
    double &duration;
    Timer(double &d) : start(Clock::now()), duration(d) {}
    ~Timer() {duration = std::chrono::duration<double>(Clock::now() - start).count();}
};

/******************************************************************************/

}
