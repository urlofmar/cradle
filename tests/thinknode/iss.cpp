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
