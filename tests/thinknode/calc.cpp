#include <cradle/thinknode/calc.hpp>

#include <cstring>

#include <boost/format.hpp>

#include <fakeit.hpp>

#include <cradle/core/monitoring.hpp>
#include <cradle/core/testing.hpp>
#include <cradle/io/http_requests.hpp>
#include <cradle/io/json_io.hpp>

using namespace cradle;
using namespace fakeit;

TEST_CASE("calc status utilities", "[thinknode]")
{
    // We can test most cases in the status ordering and query string
    // translation by simply constructing the expected order of query strings
    // and seeing if that's what repeated application of those functions
    // produces.
    std::vector<string> expected_query_order =
        {
            "status=waiting",
            "status=queued&queued=pending",
            "status=queued&queued=ready",
            // Do a couple of these manually just to make sure we're not
            // generating the same wrong string as in the actual code.
            "status=calculating&progress=0.00",
            "status=calculating&progress=0.01"
        };
    for (int i = 2; i != 100; ++i)
    {
        expected_query_order.push_back(
            str(boost::format("status=calculating&progress=%4.2f") % (i / 100.0)));
    }
    expected_query_order.push_back("status=uploading&progress=0.00");
    for (int i = 1; i != 100; ++i)
    {
        expected_query_order.push_back(
            str(boost::format("status=uploading&progress=%4.2f") % (i / 100.0)));
    }
    expected_query_order.push_back("status=completed");
    // Go through the entire progression, starting with the waiting status.
    auto status = some(construct_calculation_status_with_waiting(nil));
    for (auto query_string : expected_query_order)
    {
        REQUIRE(status);
        REQUIRE(calc_status_as_query_string(*status) == query_string);
        status = get_next_calculation_status(*status);
    }
    // Nothing further is possible.
    REQUIRE(!status);

    // Test the other cases that aren't covered above.
    {
        auto failed =
            construct_calculation_status_with_failed(calculation_failure_status());
        REQUIRE(get_next_calculation_status(failed) == none);
        REQUIRE(calc_status_as_query_string(failed) == "status=failed");
    }
    {
        auto canceled = construct_calculation_status_with_canceled(nil);
        REQUIRE(get_next_calculation_status(canceled) == none);
        REQUIRE(calc_status_as_query_string(canceled) == "status=canceled");
    }
    {
        auto generating = construct_calculation_status_with_generating(nil);
        REQUIRE(
            get_next_calculation_status(generating) ==
            some(construct_calculation_status_with_queued(calculation_queue_type::READY)));
        REQUIRE(calc_status_as_query_string(generating) == "status=generating");
    }
}

http_response static
make_mock_response(string const& body)
{
    http_response mock_response;
    mock_response.status_code = 200;
    mock_response.body = make_string_blob(body);
    return mock_response;
}

TEST_CASE("calc status query", "[thinknode]")
{
    Mock<http_connection_interface> mock_connection;

    When(Method(mock_connection, perform_request)).Do(
        [&](check_in_interface& check_in,
            progress_reporter_interface& reporter,
            http_request const& request)
        {
            auto expected_request =
                make_get_request(
                    "https://mgh.thinknode.io/api/v1.0/calc/abc/status?context=123",
                    {
                        { "Authorization", "Bearer 'xyz'" },
                        { "Accept", "application/json" }
                    });
            REQUIRE(request == expected_request);

            return make_mock_response("{ \"completed\": null }");
        });

    thinknode_session session;
    session.api_url = "https://mgh.thinknode.io/api/v1.0";
    session.access_token = "xyz";

    auto status = query_calculation_status(mock_connection.get(), session, "123", "abc");
    REQUIRE(status == construct_calculation_status_with_completed(nil));
}

TEST_CASE("calc status long polling", "[thinknode]")
{
    Mock<http_connection_interface> mock_connection;

    std::vector<http_request> expected_requests =
        {
            make_get_request(
                "https://mgh.thinknode.io/api/v1.0/calc/abc/status?context=123",
                {
                    { "Authorization", "Bearer 'xyz'" },
                    { "Accept", "application/json" }
                }),
            make_get_request(
                "https://mgh.thinknode.io/api/v1.0/calc/abc/status"
                    "?status=calculating&progress=0.12&timeout=120&context=123",
                {
                    { "Authorization", "Bearer 'xyz'" },
                    { "Accept", "application/json" }
                }),
            make_get_request(
                "https://mgh.thinknode.io/api/v1.0/calc/abc/status"
                    "?status=completed&timeout=120&context=123",
                {
                    { "Authorization", "Bearer 'xyz'" },
                    { "Accept", "application/json" }
                })
        };

    std::vector<calculation_status> mock_responses =
        {
            construct_calculation_status_with_calculating(calculation_calculating_status{0.115}),
            construct_calculation_status_with_uploading(calculation_uploading_status{0.995}),
            construct_calculation_status_with_completed(nil)
        };

    unsigned request_counter = 0;
    When(Method(mock_connection, perform_request)).AlwaysDo(
        [&](check_in_interface& check_in,
            progress_reporter_interface& reporter,
            http_request const& request)
        {
            REQUIRE(request == expected_requests.at(request_counter));
            auto response =
                make_mock_response(value_to_json(to_value(mock_responses.at(request_counter))));
            ++request_counter;
            return response;
        });

    unsigned status_counter = 0;
    auto status_checker =
        [&](calculation_status const& status)
        {
            REQUIRE(status == mock_responses.at(status_counter));
            ++status_counter;
        };

    thinknode_session session;
    session.api_url = "https://mgh.thinknode.io/api/v1.0";
    session.access_token = "xyz";

    null_check_in check_in;
    long_poll_calculation_status(
        check_in,
        status_checker,
        mock_connection.get(),
        session,
        "123",
        "abc");
}
