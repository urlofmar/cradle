#include <cradle/io/mock_http.h>

#include <cradle/utilities/testing.h>

using namespace cradle;

TEST_CASE("mock GET request", "[io][mock_http]")
{
    mock_http_session session;
    session.set_script(
        {{make_get_request(
              "http://postman-echo.com/get?color=navy", http_header_list()),
          make_http_200_response(
              R"(
                {
                    "args": {
                        "color": "navy"
                    }
                }
              )")},
         {make_get_request(
              "http://postman-echo.com/get?color=red", http_header_list()),
          make_http_200_response(
              R"(
                {
                    "args": {
                        "color": "red"
                    }
                }
              )")},
         {make_get_request(
              "http://postman-echo.com/get?color=indigo", http_header_list()),
          make_http_200_response(
              R"(
                {
                    "args": {
                        "color": "indigo"
                    }
                }
              )")},
         {make_get_request(
              "http://postman-echo.com/get?color=violet", http_header_list()),
          make_http_200_response(
              R"(
                {
                    "args": {
                        "color": "violet"
                    }
                }
              )")}});

    REQUIRE(!session.is_complete());
    REQUIRE(session.is_in_order());

    mock_http_connection conn(session);
    null_check_in check_in;
    null_progress_reporter reporter;

    REQUIRE(
        conn.perform_request(
            check_in,
            reporter,
            make_get_request(
                "http://postman-echo.com/get?color=navy", http_header_list()))
        == make_http_200_response(
            R"(
                {
                    "args": {
                        "color": "navy"
                    }
                }
              )"));
    REQUIRE(!session.is_complete());
    REQUIRE(session.is_in_order());

    REQUIRE(
        conn.perform_request(
            check_in,
            reporter,
            make_get_request(
                "http://postman-echo.com/get?color=red", http_header_list()))
        == make_http_200_response(
            R"(
                {
                    "args": {
                        "color": "red"
                    }
                }
              )"));
    REQUIRE(!session.is_complete());
    REQUIRE(session.is_in_order());

    REQUIRE(
        conn.perform_request(
            check_in,
            reporter,
            make_get_request(
                "http://postman-echo.com/get?color=violet",
                http_header_list()))
        == make_http_200_response(
            R"(
                {
                    "args": {
                        "color": "violet"
                    }
                }
              )"));
    REQUIRE(!session.is_complete());
    REQUIRE(!session.is_in_order());

    REQUIRE(
        conn.perform_request(
            check_in,
            reporter,
            make_get_request(
                "http://postman-echo.com/get?color=indigo",
                http_header_list()))
        == make_http_200_response(
            R"(
                {
                    "args": {
                        "color": "indigo"
                    }
                }
              )"));
    REQUIRE(session.is_complete());
    REQUIRE(!session.is_in_order());
}
