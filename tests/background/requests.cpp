#include <cradle/background/requests.h>

#include <cradle/utilities/testing.h>

using namespace cradle;

TEST_CASE("value requests", "[background]")
{
    auto four = rq::value(4);
    auto two = rq::value(2);
    auto another_four = rq::value(4);

    REQUIRE(four.is_resolved());

    REQUIRE(four.value_id() == another_four.value_id());
    REQUIRE(four.value_id() != two.value_id());

    bool was_dispatched = false;
    four.dispatch([&](int value) {
        was_dispatched = true;
        REQUIRE(value == 4);
    });
    REQUIRE(was_dispatched);
}

TEST_CASE("function requests", "[background]")
{
    auto four = rq::value(4);
    auto two = rq::value(2);
    auto f = [](auto x, auto y) { return x + y; };
    auto sum = rq::apply(f, four, two);

    REQUIRE(!sum.is_resolved());

    auto same_sum = rq::apply(f, four, two);
    auto commuted_sum = rq::apply(f, two, four);
    REQUIRE(sum.value_id() == same_sum.value_id());
    REQUIRE(sum.value_id() != commuted_sum.value_id());

    bool was_dispatched = false;
    sum.dispatch([&](int value) {
        was_dispatched = true;
        REQUIRE(value == 6);
    });
    REQUIRE(was_dispatched);
}
