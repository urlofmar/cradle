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
    auto two = rq::value(2);
    auto another_four = rq::value(4);

    REQUIRE(four.value_id() == another_four.value_id());
    REQUIRE(four.value_id() != two.value_id());

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
    auto f = rq::pure([](auto x, auto y) { return x + y; });
    auto sum = rq::apply(f, four, two);

    auto same_sum = rq::apply(f, four, two);
    auto commuted_sum = rq::apply(f, two, four);
    REQUIRE(sum.value_id() == same_sum.value_id());
    REQUIRE(sum.value_id() != commuted_sum.value_id());

    bool was_evaluated = false;
    post_request(sys, sum, [&](int value) {
        was_evaluated = true;
        REQUIRE(value == 6);
    });
    REQUIRE(was_evaluated);
}

static int add(int x, int y) { return x + y; }

TEST_CASE("pure function pointer apply requests", "[background]")
{
    request_resolution_system sys;

    auto four = rq::value(4);
    auto two = rq::value(2);
    auto f = rq::pure(add);
    auto sum = rq::apply(f, four, two);

    auto same_sum = rq::apply(f, four, two);
    auto commuted_sum = rq::apply(f, two, four);
    REQUIRE(sum.value_id() == same_sum.value_id());
    REQUIRE(sum.value_id() != commuted_sum.value_id());

    bool was_evaluated = false;
    post_request(sys, sum, [&](int value) {
        was_evaluated = true;
        REQUIRE(value == 6);
    });
    REQUIRE(was_evaluated);
}

TEST_CASE("impure function object apply requests", "[background]")
{
    request_resolution_system sys;

    auto four = rq::value(4);
    auto two = rq::value(2);
    auto f = rq::impure([](auto x, auto y) { return x + y; });

    auto sum = rq::apply(f, four, two);
    REQUIRE(sum.value_id() == null_id);

    bool was_evaluated = false;
    post_request(sys, sum, [&](int value) {
        was_evaluated = true;
        REQUIRE(value == 6);
    });
    REQUIRE(was_evaluated);
}

TEST_CASE("impure function pointer apply requests", "[background]")
{
    request_resolution_system sys;

    auto four = rq::value(4);
    auto two = rq::value(2);

    auto sum = rq::apply(rq::impure(add), four, two);
    REQUIRE(sum.value_id() == null_id);

    bool was_evaluated = false;
    post_request(sys, sum, [&](int value) {
        was_evaluated = true;
        REQUIRE(value == 6);
    });
    REQUIRE(was_evaluated);
}

TEST_CASE("meta requests", "[background]")
{
    request_resolution_system sys;

    auto four = rq::value(4);
    auto two = rq::value(2);
    auto sum_generator = rq::pure([](auto x, auto y) {
        return rq::apply(
            rq::pure([](auto x, auto y) { return x + y; }),
            rq::value(x),
            rq::value(y));
    });
    auto sum = rq::meta(rq::apply(sum_generator, four, two));

    auto same_sum = rq::meta(rq::apply(sum_generator, four, two));
    auto commuted_sum = rq::meta(rq::apply(sum_generator, two, four));
    REQUIRE(sum.value_id() == same_sum.value_id());
    REQUIRE(sum.value_id() != commuted_sum.value_id());

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
    auto f = rq::pure([&allowed_to_execute](auto x, auto y) {
        while (!allowed_to_execute)
            std::this_thread::yield();
        return x + y;
    });
    auto sum = rq::async(f, four, two);

    auto same_sum = rq::async(f, four, two);
    auto commuted_sum = rq::async(f, two, four);
    REQUIRE(sum.value_id() == same_sum.value_id());
    REQUIRE(sum.value_id() != commuted_sum.value_id());

    post_request(sys, sum, [&executed](int value) {
        executed = true;
        REQUIRE(value == 6);
    });
    REQUIRE(!executed);
    allowed_to_execute = true;
    REQUIRE(occurs_soon([&]() -> bool { return executed; }));
}
