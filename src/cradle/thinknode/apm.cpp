#include <cradle/thinknode/apm.h>

#include <cradle/core/monitoring.h>
#include <cradle/encodings/sha256_hash_id.h>
#include <cradle/io/http_requests.hpp>

namespace cradle {

namespace uncached {

cppcoro::task<thinknode_app_version_info>
get_app_version_info(
    service_core& service,
    thinknode_session session,
    string account,
    string app,
    string version)
{
    auto query = make_get_request(
        session.api_url + "/apm/apps/" + account + "/" + app + "/versions/"
            + version + "?include_manifest=true",
        {{"Authorization", "Bearer " + session.access_token},
         {"Accept", "application/json"}});

    auto response = co_await async_http_request(service, query);
    co_return from_dynamic<thinknode_app_version_info>(
        parse_json_response(response));
}

} // namespace uncached

cppcoro::shared_task<thinknode_app_version_info>
get_app_version_info(
    service_core& service,
    thinknode_session session,
    string account,
    string app,
    string version)
{
    auto cache_key = make_sha256_hashed_id(
        "get_app_version_info", session.api_url, account, app, version);

    return fully_cached<thinknode_app_version_info>(
        service, cache_key, [=, &service] {
            return uncached::get_app_version_info(
                service, session, account, app, version);
        });
}

} // namespace cradle
