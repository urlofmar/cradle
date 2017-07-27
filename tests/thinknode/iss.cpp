#include <cradle/thinknode/iss.hpp>

#include <cstring>

#include <boost/format.hpp>

#include <fakeit.hpp>

#include <cradle/core/monitoring.hpp>
#include <cradle/core/testing.hpp>
#include <cradle/io/http_requests.hpp>
#include <cradle/io/msgpack_io.hpp>

using namespace cradle;
using namespace fakeit;

http_response static
make_mock_response(string const& body)
{
    http_response mock_response;
    mock_response.status_code = 200;
    mock_response.body = make_string_blob(body);
    return mock_response;
}

TEST_CASE("ISS object resolution", "[thinknode]")
{
    Mock<http_connection_interface> mock_connection;

    When(Method(mock_connection, perform_request)).Do(
        [&](check_in_interface& check_in,
            progress_reporter_interface& reporter,
            http_request const& request)
        {
            auto expected_request =
                make_get_request(
                    "https://mgh.thinknode.io/api/v1.0/iss/abc/immutable?context=123",
                    {
                        { "Authorization", "Bearer 'xyz'" },
                        { "Accept", "application/json" }
                    });
            REQUIRE(request == expected_request);

            return make_mock_response("{ \"id\": \"def\" }");
        });

    thinknode_session session;
    session.api_url = "https://mgh.thinknode.io/api/v1.0";
    session.access_token = "xyz";

    auto id = resolve_iss_object_to_immutable(mock_connection.get(), session, "123", "abc");
    REQUIRE(id == "def");
}

TEST_CASE("ISS immutable retrieval", "[thinknode]")
{
    Mock<http_connection_interface> mock_connection;

    When(Method(mock_connection, perform_request)).Do(
        [&](check_in_interface& check_in,
            progress_reporter_interface& reporter,
            http_request const& request)
        {
            auto expected_request =
                make_get_request(
                    "https://mgh.thinknode.io/api/v1.0/iss/immutable/abc?context=123",
                    {
                        { "Authorization", "Bearer 'xyz'" },
                        { "Accept", "application/octet-stream" }
                    });
            REQUIRE(request == expected_request);

            return make_mock_response(value_to_msgpack_string(value("the-data")));
        });

    thinknode_session session;
    session.api_url = "https://mgh.thinknode.io/api/v1.0";
    session.access_token = "xyz";

    auto data = retrieve_immutable(mock_connection.get(), session, "123", "abc");
    REQUIRE(data == value("the-data"));
}

TEST_CASE("URL type string", "[thinknode]")
{
    api_named_type_reference named_info;
    named_info.account = "my_account";
    named_info.app = "my_app";
    named_info.name = "my_type";
    auto named_type = construct_api_type_info_with_named(named_info);
    REQUIRE(get_url_type_string(named_type) == "named/my_account/my_app/my_type");

    auto integer_type = construct_api_type_info_with_integer(api_integer_type());
    REQUIRE(get_url_type_string(integer_type) == "integer");

    auto float_type = construct_api_type_info_with_float(api_float_type());
    REQUIRE(get_url_type_string(float_type) == "float");

    auto string_type = construct_api_type_info_with_string(api_string_type());
    REQUIRE(get_url_type_string(string_type) == "string");

    auto boolean_type = construct_api_type_info_with_boolean(api_boolean_type());
    REQUIRE(get_url_type_string(boolean_type) == "boolean");

    auto blob_type = construct_api_type_info_with_blob(api_blob_type());
    REQUIRE(get_url_type_string(blob_type) == "blob");

    auto dynamic_type = construct_api_type_info_with_dynamic(api_dynamic_type());
    REQUIRE(get_url_type_string(dynamic_type) == "dynamic");

    auto nil_type = construct_api_type_info_with_nil(api_nil_type());
    REQUIRE(get_url_type_string(nil_type) == "nil");

    auto datetime_type = construct_api_type_info_with_datetime(api_datetime_type());
    REQUIRE(get_url_type_string(datetime_type) == "datetime");

    api_array_info array_info;
    array_info.element_schema = boolean_type;
    auto array_type = construct_api_type_info_with_array(array_info);
    REQUIRE(get_url_type_string(array_type) == "array/boolean");

    api_map_info map_info;
    map_info.key_schema = array_type;
    map_info.value_schema = blob_type;
    auto map_type = construct_api_type_info_with_map(map_info);
    REQUIRE(get_url_type_string(map_type) == "map/array/boolean/blob");

    api_structure_info struct_info;
    struct_info["def"].schema = array_type;
    struct_info["abc"].schema = blob_type;
    auto struct_type = construct_api_type_info_with_structure(struct_info);
    REQUIRE(get_url_type_string(struct_type) == "structure/2/abc/blob/def/array/boolean");

    api_union_info union_info;
    union_info["def"].schema = array_type;
    union_info["abc"].schema = blob_type;
    union_info["ghi"].schema = string_type;
    auto union_type = construct_api_type_info_with_union(union_info);
    REQUIRE(get_url_type_string(union_type) == "union/3/abc/blob/def/array/boolean/ghi/string");

    auto optional_type = construct_api_type_info_with_optional(map_type);
    REQUIRE(get_url_type_string(optional_type) == "optional/map/array/boolean/blob");

    api_enum_info enum_info;
    enum_info["def"] = api_enum_value_info();
    enum_info["abc"] = api_enum_value_info();
    auto enum_type = construct_api_type_info_with_enum(enum_info);
    REQUIRE(get_url_type_string(enum_type) == "enum/2/abc/def");

    auto ref_type = construct_api_type_info_with_reference(named_type);
    REQUIRE(get_url_type_string(ref_type) == "reference/named/my_account/my_app/my_type");
}

TEST_CASE("ISS POST", "[thinknode]")
{
    Mock<http_connection_interface> mock_connection;

    When(Method(mock_connection, perform_request)).Do(
        [&](check_in_interface& check_in,
            progress_reporter_interface& reporter,
            http_request const& request)
        {
            auto expected_request =
                make_post_request(
                    "https://mgh.thinknode.io/api/v1.0/iss/string?context=123",
                    value_to_msgpack_blob(value("payload")),
                    {
                        { "Authorization", "Bearer 'xyz'" },
                        { "Accept", "application/json" },
                        { "Content-Type", "application/octet-stream" }
                    });
            REQUIRE(request == expected_request);

            return make_mock_response("{ \"id\": \"def\" }");
        });

    thinknode_session session;
    session.api_url = "https://mgh.thinknode.io/api/v1.0";
    session.access_token = "xyz";

    auto id =
        post_iss_object(
            mock_connection.get(),
            session,
            "123",
            construct_api_type_info_with_string(api_string_type()),
            value("payload"));
    REQUIRE(id == "def");
}
