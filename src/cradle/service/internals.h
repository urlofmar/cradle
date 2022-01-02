#ifndef CRADLE_SERVICE_INTERNALS_H
#define CRADLE_SERVICE_INTERNALS_H

#include <cppcoro/static_thread_pool.hpp>

#include <thread-pool/thread_pool.hpp>

#include <cradle/caching/disk_cache.hpp>
#include <cradle/caching/immutable.h>
#include <cradle/io/mock_http.h>
#include <cradle/thinknode/types.hpp>

namespace cradle {

namespace detail {

struct service_core_internals
{
    cradle::immutable_cache cache;

    cppcoro::static_thread_pool http_pool;

    std::map<
        std::pair<string, thinknode_provider_image_info>,
        cppcoro::static_thread_pool>
        local_compute_pool;

    cradle::disk_cache disk_cache;
    cppcoro::static_thread_pool disk_read_pool;
    thread_pool disk_write_pool;

    std::unique_ptr<mock_http_session> mock_http;
};

} // namespace detail

} // namespace cradle

#endif
