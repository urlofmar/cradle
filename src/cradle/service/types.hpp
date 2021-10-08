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
};

} // namespace cradle

#endif
