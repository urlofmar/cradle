#include <cradle/services/requests.h>

#include <algorithm>
#include <thread>

namespace cradle {

request_resolution_system::request_resolution_system()
    : impl_(std::make_unique<detail::request_resolution_system>())
{
    impl_->cache.reset(immutable_cache_config(1024));

    detail::initialize_pool<detail::basic_executor>(
        impl_->execution_pool,
        std::max(std::thread::hardware_concurrency(), 1u),
        [] { return detail::basic_executor(); });

    detail::initialize_pool<http_request_executor>(
        impl_->http_pool, 16, [impl = impl_.get()] {
            return http_request_executor(impl->http_system);
        });
}

request_resolution_system::~request_resolution_system()
{
    shut_down_pool(impl_->http_pool);
    shut_down_pool(impl_->execution_pool);
}

} // namespace cradle
