#include <cradle/service/core.h>

#include <cradle/service/internals.h>

namespace cradle {

service_core::service_core()
{
    impl_.reset(new detail::service_core_internals{
        .cache = immutable_cache(immutable_cache_config(1024)),
        .compute_pool = cppcoro::static_thread_pool(),
        .http_pool = cppcoro::static_thread_pool(24)});
}

service_core::~service_core()
{
}

http_connection&
http_connection_for_thread()
{
    static http_request_system the_system;
    static thread_local http_connection the_connection(the_system);
    return the_connection;
}

cppcoro::task<http_response>
async_http_request(service_core& core, http_request request)
{
    co_await core.internals().http_pool.schedule();
    null_check_in check_in;
    null_progress_reporter reporter;
    co_return http_connection_for_thread().perform_request(
        check_in, reporter, request);
}

} // namespace cradle
