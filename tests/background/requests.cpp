#include <cradle/background/requests.h>

#include <cradle/utilities/testing.h>

using namespace cradle;

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

TEST_CASE("apply requests", "[background]")
{
    request_resolution_system sys;

    auto four = rq::value(4);
    auto two = rq::value(2);
    auto f = [](auto x, auto y) { return x + y; };
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
