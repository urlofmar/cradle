#include <cradle/caching/immutable/internals.h>

#include <mutex>

#include <cradle/utilities/text.h>

namespace cradle {

namespace detail {

void
reduce_memory_cache_size(immutable_cache& cache, uint64_t desired_size)
{
    std::scoped_lock<std::mutex> lock(cache.mutex);
    while (!cache.eviction_list.records.empty()
           && cache.eviction_list.total_size > desired_size)
    {
        auto const& record = cache.eviction_list.records.front();
        auto data_size = record->size;
        cache.records.erase(&*record->key);
        cache.eviction_list.records.pop_front();
        cache.eviction_list.total_size -= data_size;
    }
}

} // namespace detail

} // namespace cradle
