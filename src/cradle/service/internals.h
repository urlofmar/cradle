#ifndef CRADLE_SERVICE_INTERNALS_H
#define CRADLE_SERVICE_INTERNALS_H

#include <cppcoro/static_thread_pool.hpp>

#include <thread-pool/thread_pool.hpp>

#include <cradle/caching/disk_cache.hpp>
#include <cradle/caching/immutable.h>

namespace cradle {

namespace detail {

struct service_core_internals
{
    cradle::immutable_cache cache;

    cppcoro::static_thread_pool compute_pool, http_pool;

    cradle::disk_cache disk_cache;
    cppcoro::static_thread_pool disk_read_pool;
    thread_pool disk_write_pool;
};

} // namespace detail

} // namespace cradle

#endif
