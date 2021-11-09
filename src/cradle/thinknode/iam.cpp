#include <cradle/thinknode/iam.h>

#include <cradle/core/monitoring.h>
#include <cradle/encodings/sha256_hash_id.h>
#include <cradle/io/http_requests.hpp>

namespace cradle {

namespace uncached {

cppcoro::task<thinknode_context_contents>
get_context_contents(
    service_core& service, thinknode_session session, string context_id)
{
    auto query = make_get_request(
        session.api_url + "/iam/contexts/" + context_id,
        {{"Authorization", "Bearer " + session.access_token},
         {"Accept", "application/json"}});

    auto response = co_await async_http_request(service, query);

    co_return from_dynamic<thinknode_context_contents>(
        parse_json_response(response));
}

} // namespace uncached

cppcoro::shared_task<thinknode_context_contents>
get_context_contents(
    service_core& service, thinknode_session session, string context_id)
{
    auto cache_key = make_sha256_hashed_id(
        "get_context_contents", session.api_url, context_id);

    return fully_cached<thinknode_context_contents>(
        service, cache_key, [=, &service] {
            return uncached::get_context_contents(
                service, session, context_id);
        });
}

} // namespace cradle
