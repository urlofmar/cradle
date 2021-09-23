#include <cradle/background/requests.h>

#include <cradle/utilities/testing.h>

using namespace cradle;

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

TEST_CASE("value requests", "[background]")
{
    request_resolution_system sys;

    auto four = rq::value(4);

    bool was_evaluated = false;
    post_request(sys, four, [&](int value) {
        was_evaluated = true;
        REQUIRE(value == 4);
    });
    REQUIRE(was_evaluated);
}

TEST_CASE("pure function object apply requests", "[background]")
{
    request_resolution_system sys;

    auto four = rq::value(4);
    auto two = rq::value(2);

    auto sum = rq::apply([](auto x, auto y) { return x + y; }, four, two);
    {
        bool was_evaluated = false;
        post_request(sys, sum, [&](int value) {
            was_evaluated = true;
            REQUIRE(value == 6);
        });
        REQUIRE(was_evaluated);
    }

    auto difference
        = rq::apply([](auto x, auto y) { return x - y; }, four, two);
    {
        bool was_evaluated = false;
        post_request(sys, difference, [&](int value) {
            was_evaluated = true;
            REQUIRE(value == 2);
        });
        REQUIRE(was_evaluated);
    }
}

TEST_CASE("meta requests", "[background]")
{
    request_resolution_system sys;

    auto four = rq::value(4);
    auto two = rq::value(2);
    auto sum_generator = [](auto x, auto y) {
        return rq::apply(
            [](auto x, auto y) { return x + y; }, rq::value(x), rq::value(y));
    };
    auto sum = rq::meta(rq::apply(sum_generator, four, two));

    bool was_evaluated = false;
    post_request(sys, sum, [&](int value) {
        was_evaluated = true;
        REQUIRE(value == 6);
    });
    REQUIRE(was_evaluated);
}

TEST_CASE("async requests", "[background]")
{
    request_resolution_system sys;

    std::atomic<bool> allowed_to_execute = false;
    std::atomic<bool> executed = false;

    auto four = rq::value(4);
    auto two = rq::value(2);
    auto f = [&allowed_to_execute](auto x, auto y) {
        while (!allowed_to_execute)
            std::this_thread::yield();
        return x + y;
    };
    auto sum = rq::async(f, four, two);

    post_request(sys, sum, [&executed](int value) {
        executed = true;
        REQUIRE(value == 6);
    });
    REQUIRE(!executed);
    allowed_to_execute = true;
    REQUIRE(occurs_soon([&]() -> bool { return executed; }));
}

