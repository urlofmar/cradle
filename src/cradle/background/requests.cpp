#include <cradle/background/requests.h>

#include <algorithm>
#include <thread>

namespace cradle {

request_resolution_system::request_resolution_system()
    : impl_(std::make_unique<detail::request_resolution_system>())
{
    detail::initialize_pool<detail::basic_executor>(
        impl_->execution_pool,
        std::max(std::thread::hardware_concurrency(), 1u),
        [] { return detail::basic_executor(); });

    impl_->cache.reset(immutable_cache_config(1024));
}

request_resolution_system::~request_resolution_system()
{
    shut_down_pool(impl_->execution_pool);
}

} // namespace cradle
