#ifndef CRADLE_IO_MOCK_HTTP_H
#define CRADLE_IO_MOCK_HTTP_H

#include <cradle/io/http_requests.hpp>

namespace cradle {

struct mock_http_exchange
{
    http_request request;
    http_response response;
};

typedef std::vector<mock_http_exchange> mock_http_script;

struct mock_http_session
{
    mock_http_session()
    {
    }

    mock_http_session(mock_http_script script)
    {
        set_script(std::move(script));
    }

    // Set the script of expected exchanges for this mock HTTP session.
    void
    set_script(mock_http_script script);

    // Have all exchanges in the script been executed?
    bool
    is_complete() const;

    // Has the script been executed in order so far?
    bool
    is_in_order() const;

 private:
    friend struct mock_http_connection;

    mock_http_script script_;

    // Has the script been executed in order so far?
    bool in_order_ = true;
};

struct mock_http_connection : http_connection_interface
{
    mock_http_connection(mock_http_session& session) : session_(session)
    {
    }

    http_response
    perform_request(
        check_in_interface& check_in,
        progress_reporter_interface& reporter,
        http_request const& request) override;

 private:
    mock_http_session& session_;
};

} // namespace cradle

#endif
