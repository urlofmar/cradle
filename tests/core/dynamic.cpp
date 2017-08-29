#include <cradle/core/dynamic.hpp>

#include <cradle/common.hpp>
#include <cradle/core/testing.hpp>

using namespace cradle;

TEST_CASE("value_type streaming", "[core][dynamic]")
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

TEST_CASE("dynamic type checking", "[core][dynamic]")
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

TEST_CASE("dynamic initializer lists", "[core][dynamic]")
{
    // Test a simple initializer list.
    REQUIRE(
        (dynamic{ 0., 1., 2. }) ==
        dynamic(dynamic_array{ dynamic(0.), dynamic(1.), dynamic(2.) }));

    // Test that lists that look like maps are treated like maps.
    REQUIRE(
        (dynamic{ { "foo", 0. }, { "bar", 1. } }) ==
        dynamic(dynamic_map{ { dynamic("foo"), dynamic(0.) }, { dynamic("bar"), dynamic(1.) } }));

    // Test that the conversion to map only happens with string keys.
    REQUIRE(
        (dynamic{ { "foo", 0. }, { 0., 1. } }) ==
        dynamic(
            dynamic_array{
                dynamic_array{ dynamic("foo"), dynamic(0.) },
                dynamic_array{ dynamic(0.), dynamic(1.) } }));
}

TEST_CASE("dynamic type interface", "[core][dynamic]")
{
    test_regular_value_pair(
        dynamic(false),
        dynamic(true));

    test_regular_value_pair(
        dynamic(integer(0)),
        dynamic(integer(1)));

    test_regular_value_pair(
        dynamic(0.),
        dynamic(1.));

    test_regular_value_pair(
        dynamic(string("bar")),
        dynamic(string("foo")));

    char blob_data[] = { 'a', 'b' };
    test_regular_value_pair(
        dynamic(blob(ownership_holder(), blob_data, 1)),
        dynamic(blob(ownership_holder(), blob_data, 2)));

    test_regular_value_pair(
        dynamic(
            boost::posix_time::ptime(
                boost::gregorian::date(2017,boost::gregorian::Apr,26),
                boost::posix_time::time_duration(1,2,3))),
        dynamic(
            boost::posix_time::ptime(
                boost::gregorian::date(2017,boost::gregorian::Apr,26),
                boost::posix_time::time_duration(1,2,4))));

    test_regular_value_pair(
        dynamic(dynamic_array({ dynamic(0.), dynamic(1.) })),
        dynamic(dynamic_array({ dynamic(1.), dynamic(2.) })));

    test_regular_value_pair(
        dynamic(dynamic_map({ { dynamic(0.), dynamic(1.) } })),
        dynamic(dynamic_map({ { dynamic(1.), dynamic(2.) } })));
}

TEST_CASE("dynamic deep_sizeof", "[core][dynamic]")
{
    REQUIRE(deep_sizeof(dynamic(nil)) == sizeof(dynamic) + deep_sizeof(nil));
    REQUIRE(deep_sizeof(dynamic(false)) == sizeof(dynamic) + deep_sizeof(false));
    REQUIRE(
        deep_sizeof(dynamic(integer(0))) ==
        sizeof(dynamic) + deep_sizeof(integer(0)));
    REQUIRE(deep_sizeof(dynamic(0.)) == sizeof(dynamic) + deep_sizeof(0.));
    REQUIRE(
        deep_sizeof(dynamic(string("foo"))) ==
        sizeof(dynamic) + deep_sizeof(string("foo")));
    char blob_data[] = { 'a', 'b' };
    blob blob(ownership_holder(), blob_data, 2);
    REQUIRE(deep_sizeof(dynamic(blob)) == sizeof(dynamic) + deep_sizeof(blob));
    auto time =
        boost::posix_time::ptime(
            boost::gregorian::date(2017,boost::gregorian::Apr,26),
            boost::posix_time::time_duration(1,2,3));
    REQUIRE(deep_sizeof(dynamic(time)) == sizeof(dynamic) + deep_sizeof(time));
    auto list = dynamic_array({ dynamic(3.), dynamic(1.), dynamic(2.) });
    REQUIRE(deep_sizeof(dynamic(list)) == sizeof(dynamic) + deep_sizeof(list));
    auto map = dynamic_map({ { dynamic(0.), dynamic(1.) }, { dynamic(1.), dynamic(2.) } });
    REQUIRE(deep_sizeof(dynamic(map)) == sizeof(dynamic) + deep_sizeof(map));

    REQUIRE(deep_sizeof(dynamic_array()) == sizeof(dynamic_array));
    REQUIRE(deep_sizeof(dynamic_map()) == sizeof(dynamic_map));
}

TEST_CASE("empty list/map equivalence", "[core][dynamic]")
{
    {
        INFO("Dynamic values containing empty maps can be treated as empty lists.")
        REQUIRE(cast<dynamic_array>(dynamic(dynamic_map())) == dynamic_array());
        INFO("This doesn't work for non-empty maps.")
        REQUIRE_THROWS(cast<dynamic_array>(dynamic(dynamic_map({ { dynamic(0.), dynamic(1.) } }))));
    }
    {
        INFO("Dynamic values containing empty lists can be treated as empty maps.")
        REQUIRE(cast<dynamic_map>(dynamic(dynamic_array())) == dynamic_map());
        INFO("This doesn't work for non-empty lists.")
        REQUIRE_THROWS(cast<dynamic_map>(dynamic(dynamic_array({ dynamic(1.) }))));
    }
}

TEST_CASE("get_field", "[core][dynamic]")
{
    auto map =
        dynamic_map(
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

TEST_CASE("get_union_value_type", "[core][dynamic]")
{
    // Try getting the type from a proper union dynamic.
    REQUIRE(
        get_union_value_type(
            dynamic_map(
                {
                    { "a", 12. },
                })) ==
            "a");

    // Try with an empty map.
    try
    {
        get_union_value_type(dynamic_map());
        FAIL("no exception thrown");
    }
    catch (multifield_union& )
    {
    }

    // Try with a map with too many fields.
    try
    {
        get_union_value_type(
            dynamic_map(
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

TEST_CASE("dynamic operators", "[core][dynamic]")
{
    dynamic a;
    dynamic b(integer(0));
    dynamic c(integer(1));

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
