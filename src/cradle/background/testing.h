#ifndef CRADLE_BACKGROUND_TESTING_H
#define CRADLE_BACKGROUND_TESTING_H

#include <chrono>
#include <thread>
#include <utility>

namespace cradle {

// Wait up to a second to see if a condition occurs (i.e., returns true).
// Check once per millisecond to see if it occurs.
// Return whether or not it occurs.
template<class Condition>
bool
occurs_soon(Condition&& condition)
{
    int n = 0;
    while (true)
    {
        if (std::forward<Condition>(condition)())
            return true;
        if (++n > 1000)
            return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

} // namespace cradle

#endif
