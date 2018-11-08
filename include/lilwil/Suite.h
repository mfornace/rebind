#pragma once

#include "Test.h"
#include <deque>

namespace lilwil {

/******************************************************************************/

template <class X>
using Suite = std::deque<TestCase<X>>;

template <class X>
Suite<X> & suite();

struct Timer {
    Clock::time_point start;
    double &duration;
    Timer(double &d) : start(Clock::now()), duration(d) {}
    ~Timer() {duration = std::chrono::duration<double>(Clock::now() - start).count();}
};

template <class X, class... Ts>
void add_test(Ts &&...ts) {
    suite<X>().emplace_back(static_cast<Ts &&>(ts)...);
}

/******************************************************************************/

}
