#ifndef CRADLE_SERVICE_TYPES
#define CRADLE_SERVICE_TYPES

#include <cradle/caching/disk_cache.hpp>
#include <cradle/caching/immutable.h>

namespace cradle {

api(struct)
struct service_config
{
    // config for the immutable memory cache
    omissible<immutable_cache_config> immutable_cache;

    // config for the disk cache
    omissible<disk_cache_config> disk_cache;

    // how many concurrent threads to use for request handling -
    // The default is one thread for each processor core.
    omissible<integer> request_concurrency;

    // how many concurrent threads to use for computing -
    // The default is one thread for each processor core.
    omissible<integer> compute_concurrency;

    // how many concurrent threads to use for HTTP requests
    omissible<integer> http_concurrency;
};

} // namespace cradle

#endif
