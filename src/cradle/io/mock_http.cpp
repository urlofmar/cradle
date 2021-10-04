#include <cradle/io/mock_http.h>

#include <cradle/utilities/errors.h>

#include <algorithm>

namespace cradle {

void
mock_http_session::set_script(mock_http_script script)
{
    script_ = std::move(script);
    in_order_ = true;
}

bool
mock_http_session::is_complete() const
{
    return script_.empty();
}

bool
mock_http_session::is_in_order() const
{
    return in_order_;
}

http_response
mock_http_connection::perform_request(
    check_in_interface&,
    progress_reporter_interface&,
    http_request const& request)
{
    auto exchange
        = std::ranges::find_if(session_.script_, [&](auto const& exchange) {
              return exchange.request == request;
          });
    if (exchange == session_.script_.end())
    {
        CRADLE_THROW(
            internal_check_failed()
            << internal_error_message_info("unrecognized mock HTTP request"));
    }
    if (exchange != session_.script_.begin())
        session_.in_order_ = false;
    http_response response = exchange->response;
    session_.script_.erase(exchange);
    return response;
}

} // namespace cradle
