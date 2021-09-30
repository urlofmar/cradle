#ifndef CRADLE_UTILTIES_CONCURRENCY_TESTING_H
#define CRADLE_UTILTIES_CONCURRENCY_TESTING_H

#include <chrono>
#include <thread>
#include <utility>

namespace cradle {

// Wait up to a second to see if a condition occurs (i.e., returns true).
// Check once per millisecond to see if it occurs.
// Return whether or not it occurs.
// A second argument can be supplied to change the time to wait.
template<class Condition>
bool
occurs_soon(Condition&& condition, int wait_time_in_ms = 1000)
{
    int n = 0;
    while (true)
    {
        if (std::forward<Condition>(condition)())
            return true;
        if (++n > wait_time_in_ms)
            return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

} // namespace cradle

#endif
