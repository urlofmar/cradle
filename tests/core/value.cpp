#include <cradle/core/value.hpp>

#include <cradle/common.hpp>
#include <cradle/core/testing.hpp>

using namespace cradle;

TEST_CASE("value_type streaming", "[core][value]")
{
    REQUIRE(boost::lexical_cast<string>(value_type::NIL) == "nil");
    REQUIRE(boost::lexical_cast<string>(value_type::BOOLEAN) == "boolean");
    REQUIRE(boost::lexical_cast<string>(value_type::INTEGER) == "integer");
    REQUIRE(boost::lexical_cast<string>(value_type::FLOAT) == "float");
    REQUIRE(boost::lexical_cast<string>(value_type::STRING) == "string");
    REQUIRE(boost::lexical_cast<string>(value_type::BLOB) == "blob");
    REQUIRE(boost::lexical_cast<string>(value_type::DATETIME) == "datetime");
    REQUIRE(boost::lexical_cast<string>(value_type::LIST) == "list");
    REQUIRE(boost::lexical_cast<string>(value_type::MAP) == "map");
    REQUIRE_THROWS_AS(
        boost::lexical_cast<string>(value_type(-1)),
        invalid_enum_value const&);
}

TEST_CASE("value type checking", "[core][value]")
{
    try
    {
        check_type(value_type::NIL, value_type::BOOLEAN);
        FAIL("no exception thrown");
    }
    catch (type_mismatch& e)
    {
        REQUIRE(get_required_error_info<expected_value_type_info>(e) == value_type::NIL);
        REQUIRE(get_required_error_info<actual_value_type_info>(e) == value_type::BOOLEAN);
    }

    REQUIRE_NOTHROW(check_type(value_type::NIL, value_type::NIL));
}

TEST_CASE("value initializer lists", "[core][value]")
{
    // Test a simple initializer list.
    REQUIRE(
        (value{ 0., 1., 2. }) ==
        value(value_list{ value(0.), value(1.), value(2.) }));

    // Test that lists that look like maps are treated like maps.
    REQUIRE(
        (value{ { "foo", 0. }, { "bar", 1. } }) ==
        value(value_map{ { value("foo"), value(0.) }, { value("bar"), value(1.) } }));

    // Test that the conversion to map only happens with string keys.
    REQUIRE(
        (value{ { "foo", 0. }, { 0., 1. } }) ==
        value(
            value_list{
                value_list{ value("foo"), value(0.) },
                value_list{ value(0.), value(1.) } }));
}

TEST_CASE("value type interface", "[core][value]")
{
    test_regular_value_pair(
        value(false),
        value(true));

    test_regular_value_pair(
        value(integer(0)),
        value(integer(1)));

    test_regular_value_pair(
        value(0.),
        value(1.));

    test_regular_value_pair(
        value(string("bar")),
        value(string("foo")));

    char blob_data[] = { 'a', 'b' };
    test_regular_value_pair(
        value(blob(ownership_holder(), blob_data, 1)),
        value(blob(ownership_holder(), blob_data, 2)));

    test_regular_value_pair(
        value(
            boost::posix_time::ptime(
                boost::gregorian::date(2017,boost::gregorian::Apr,26),
                boost::posix_time::time_duration(1,2,3))),
        value(
            boost::posix_time::ptime(
                boost::gregorian::date(2017,boost::gregorian::Apr,26),
                boost::posix_time::time_duration(1,2,4))));

    test_regular_value_pair(
        value(value_list({ value(0.), value(1.) })),
        value(value_list({ value(1.), value(2.) })));

    test_regular_value_pair(
        value(value_map({ { value(0.), value(1.) } })),
        value(value_map({ { value(1.), value(2.) } })));
}

TEST_CASE("value deep_sizeof", "[core][value]")
{
    REQUIRE(deep_sizeof(value(nil)) == sizeof(value) + deep_sizeof(nil));
    REQUIRE(deep_sizeof(value(false)) == sizeof(value) + deep_sizeof(false));
    REQUIRE(
        deep_sizeof(value(integer(0))) ==
        sizeof(value) + deep_sizeof(integer(0)));
    REQUIRE(deep_sizeof(value(0.)) == sizeof(value) + deep_sizeof(0.));
    REQUIRE(
        deep_sizeof(value(string("foo"))) ==
        sizeof(value) + deep_sizeof(string("foo")));
    char blob_data[] = { 'a', 'b' };
    blob blob(ownership_holder(), blob_data, 2);
    REQUIRE(deep_sizeof(value(blob)) == sizeof(value) + deep_sizeof(blob));
    auto time =
        boost::posix_time::ptime(
            boost::gregorian::date(2017,boost::gregorian::Apr,26),
            boost::posix_time::time_duration(1,2,3));
    REQUIRE(deep_sizeof(value(time)) == sizeof(value) + deep_sizeof(time));
    auto list = value_list({ value(3.), value(1.), value(2.) });
    REQUIRE(deep_sizeof(value(list)) == sizeof(value) + deep_sizeof(list));
    auto map = value_map({ { value(0.), value(1.) }, { value(1.), value(2.) } });
    REQUIRE(deep_sizeof(value(map)) == sizeof(value) + deep_sizeof(map));

    REQUIRE(deep_sizeof(value_list()) == sizeof(value_list));
    REQUIRE(deep_sizeof(value_map()) == sizeof(value_map));
}

TEST_CASE("empty list/map equivalence", "[core][value]")
{
    {
        INFO("Dynamic values containing empty maps can be treated as empty lists.")
        REQUIRE(cast<value_list>(value(value_map())) == value_list());
        INFO("This doesn't work for non-empty maps.")
        REQUIRE_THROWS(cast<value_list>(value(value_map({ { value(0.), value(1.) } }))));
    }
    {
        INFO("Dynamic values containing empty lists can be treated as empty maps.")
        REQUIRE(cast<value_map>(value(value_list())) == value_map());
        INFO("This doesn't work for non-empty lists.")
        REQUIRE_THROWS(cast<value_map>(value(value_list({ value(1.) }))));
    }
}

TEST_CASE("get_field", "[core][value]")
{
    auto map =
        value_map(
            {
                { "a", 12. },
                { "b", false }
            });

    // Try getting both fields.
    REQUIRE(get_field(map, "a") == 12.);
    REQUIRE(get_field(map, "b") == false);

    // Try a missing field.
    try
    {
        get_field(map, "c");
        FAIL("no exception thrown");
    }
    catch (missing_field& e)
    {
        REQUIRE(get_required_error_info<field_name_info>(e) == "c");
    }
}

TEST_CASE("get_union_value_type", "[core][value]")
{
    // Try getting the type from a proper union value.
    REQUIRE(
        get_union_value_type(
            value_map(
                {
                    { "a", 12. },
                })) ==
            "a");

    // Try with an empty map.
    try
    {
        get_union_value_type(value_map());
        FAIL("no exception thrown");
    }
    catch (multifield_union& )
    {
    }

    // Try with a map with too many fields.
    try
    {
        get_union_value_type(
            value_map(
                {
                    { "a", 12. },
                    { "b", false }
                }));
        FAIL("no exception thrown");
    }
    catch (multifield_union& )
    {
    }
}

TEST_CASE("value operators", "[core][value]")
{
    value a;
    value b(integer(0));
    value c(integer(1));

    REQUIRE(a == a);
    REQUIRE(b == b);
    REQUIRE(c == c);

    REQUIRE(a != b);
    REQUIRE(b != c);
    REQUIRE(a != c);

    REQUIRE(a < b);
    REQUIRE(b < c);
    REQUIRE(a < c);

    REQUIRE(b > a);
    REQUIRE(c > b);
    REQUIRE(c > a);

    REQUIRE(a <= a);
    REQUIRE(b <= b);
    REQUIRE(c <= c);

    REQUIRE(a <= b);
    REQUIRE(b <= c);
    REQUIRE(a <= c);

    REQUIRE(a >= a);
    REQUIRE(b >= b);
    REQUIRE(c >= c);

    REQUIRE(b >= a);
    REQUIRE(c >= b);
    REQUIRE(c >= a);
}
