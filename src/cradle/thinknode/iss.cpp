#include <cradle/thinknode/iss.hpp>

#include <cradle/core/monitoring.hpp>
#include <cradle/io/http_requests.hpp>

namespace cradle {

string
resolve_iss_object_to_immutable(
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& object_id)
{
    auto query =
        make_get_request(
            session.api_url + "/iss/" + object_id + "/immutable?context=" + context_id,
            {
                { "Authorization", "Bearer '" + session.access_token + "'" },
                { "Accept", "application/json" }
            });
    null_check_in check_in;
    null_progress_reporter reporter;
    auto response = connection.perform_request(check_in, reporter, query);
    return from_value<id_response>(parse_json_response(response)).id;
}

value
retrieve_immutable(
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& immutable_id)
{
    auto query =
        make_get_request(
            session.api_url + "/iss/immutable/" + immutable_id + "?context=" + context_id,
            {
                { "Authorization", "Bearer '" + session.access_token + "'" },
                { "Accept", "application/octet-stream" }
            });
    null_check_in check_in;
    null_progress_reporter reporter;
    auto response = connection.perform_request(check_in, reporter, query);
    return parse_msgpack_response(response);
}

}
