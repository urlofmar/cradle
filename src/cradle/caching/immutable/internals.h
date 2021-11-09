#ifndef CRADLE_CACHING_IMMUTABLE_INTERNALS_H
#define CRADLE_CACHING_IMMUTABLE_INTERNALS_H

#include <any>
#include <atomic>
#include <list>
#include <mutex>
#include <unordered_map>

#include <cradle/caching/immutable/cache.hpp>

namespace cradle {

struct immutable_cache_entry_watcher;

namespace detail {

struct immutable_cache;

struct immutable_cache_record
{
    // These remain constant for the life of the record.
    immutable_cache* owner_cache;
    captured_id key;

    // All of the following fields are protected by the cache mutex. The only
    // exception is that the :state and :progress fields can be polled for
    // informational purposes. However, before accessing any other fields based
    // on the value of :state, you should acquire the mutex and recheck state.

    // This is a count of how many active pointers reference this data.
    // If this is 0, the data is no longer actively in use and is queued for
    // eviction. In this case, :eviction_list_iterator points to this record's
    // entry in the eviction list.
    unsigned ref_count = 0;

    // (See :ref_count comment.)
    std::list<immutable_cache_record*>::iterator eviction_list_iterator;

    // Is the data ready?
    std::atomic<immutable_cache_entry_state> state
        = immutable_cache_entry_state::LOADING;

    // the associated cppcoro task - This should probably be stored more
    // efficiently. It holds a cppcoro::shared_task<T>, where T is the type
    // of data associated with this record.
    std::any task;

    // the size of the data (if it's ready)
    std::size_t size = 0;
};

typedef std::unordered_map<
    id_interface const*,
    std::unique_ptr<immutable_cache_record>,
    id_interface_pointer_hash,
    id_interface_pointer_equality_test>
    cache_record_map;

struct cache_record_eviction_list
{
    std::list<immutable_cache_record*> records;
    size_t total_size;
    cache_record_eviction_list() : total_size(0)
    {
    }
};

struct immutable_cache : noncopyable
{
    immutable_cache_config config;
    cache_record_map records;
    cache_record_eviction_list eviction_list;
    std::mutex mutex;
};

// Evict unused entries (in LRU order) until the total size of unused entries
// in the cache is at most :desired_size (in bytes).
void
reduce_memory_cache_size(immutable_cache& cache, uint64_t desired_size);

} // namespace detail
} // namespace cradle

#endif
