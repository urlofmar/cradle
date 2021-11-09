#include <cradle/caching/immutable/ptr.h>

#include <cradle/caching/immutable/internals.h>

namespace cradle {
namespace detail {

void
record_immutable_cache_value(
    immutable_cache& cache, id_interface const& key, size_t size)
{
    std::scoped_lock<std::mutex> lock(cache.mutex);
    cache_record_map::iterator i = cache.records.find(&key);
    if (i != cache.records.end())
    {
        immutable_cache_record& record = *i->second;
        record.state.store(
            immutable_cache_entry_state::READY, std::memory_order_relaxed);
        record.size = size;
    }
}

void
record_immutable_cache_failure(immutable_cache& cache, id_interface const& key)
{
    std::scoped_lock<std::mutex> lock(cache.mutex);
    cache_record_map::iterator i = cache.records.find(&key);
    if (i != cache.records.end())
    {
        immutable_cache_record& record = *i->second;
        record.state.store(
            immutable_cache_entry_state::FAILED, std::memory_order_relaxed);
    }
}

namespace {

void
remove_from_eviction_list(
    immutable_cache& cache, immutable_cache_record* record)
{
    auto& list = cache.eviction_list;
    assert(record->eviction_list_iterator != list.records.end());
    list.records.erase(record->eviction_list_iterator);
    record->eviction_list_iterator = list.records.end();
    if (record->state.load(std::memory_order_relaxed)
        == immutable_cache_entry_state::READY)
    {
        list.total_size -= record->size;
    }
}

void
acquire_cache_record_no_lock(immutable_cache_record* record)
{
    ++record->ref_count;
    auto& evictions = record->owner_cache->eviction_list.records;
    if (record->eviction_list_iterator != evictions.end())
    {
        assert(record->ref_count == 1);
        remove_from_eviction_list(*record->owner_cache, record);
    }
}

immutable_cache_record*
acquire_cache_record(
    immutable_cache& cache,
    id_interface const& key,
    function_view<std::any(
        immutable_cache& cache, id_interface const& key)> const& create_task)
{
    std::scoped_lock<std::mutex> lock(cache.mutex);
    cache_record_map::iterator i = cache.records.find(&key);
    if (i == cache.records.end())
    {
        auto record = std::make_unique<immutable_cache_record>();
        record->owner_cache = &cache;
        record->eviction_list_iterator = cache.eviction_list.records.end();
        record->key.capture(key);
        record->ref_count = 0;
        record->task = create_task(cache, key);
        i = cache.records.emplace(&*record->key, std::move(record)).first;
    }
    immutable_cache_record* record = i->second.get();
    // TODO: Better (optional) retry logic.
    if (record->state.load(std::memory_order_relaxed)
        == immutable_cache_entry_state::FAILED)
    {
        record->task = create_task(cache, key);
        record->state.store(
            immutable_cache_entry_state::LOADING, std::memory_order_relaxed);
    }
    acquire_cache_record_no_lock(record);
    return record;
}

void
acquire_cache_record(immutable_cache_record* record)
{
    std::scoped_lock<std::mutex> lock(record->owner_cache->mutex);
    acquire_cache_record_no_lock(record);
}

void
add_to_eviction_list(immutable_cache& cache, immutable_cache_record* record)
{
    auto& list = cache.eviction_list;
    assert(record->eviction_list_iterator == list.records.end());
    record->eviction_list_iterator
        = list.records.insert(list.records.end(), record);
    list.total_size += record->size;
}

void
release_cache_record(immutable_cache_record* record)
{
    auto& cache = *record->owner_cache;
    bool do_lru_eviction = false;
    {
        std::scoped_lock<std::mutex> lock(cache.mutex);
        --record->ref_count;
        if (record->ref_count == 0)
        {
            add_to_eviction_list(cache, record);
            do_lru_eviction = true;
        }
    }
    if (do_lru_eviction)
    {
        reduce_memory_cache_size(cache, cache.config.unused_size_limit);
    }
}

} // namespace

// UNTYPED_IMMUTABLE_CACHE_PTR

void
untyped_immutable_cache_ptr::reset()
{
    if (record_)
    {
        detail::release_cache_record(record_);
        record_ = nullptr;
    }
    key_.clear();
}

void
untyped_immutable_cache_ptr::acquire(
    cradle::immutable_cache& cache,
    id_interface const& key,
    function_view<std::any(
        immutable_cache& cache, id_interface const& key)> const& create_task)
{
    record_ = detail::acquire_cache_record(*cache.impl, key, create_task);
    key_.capture(key);
}

void
untyped_immutable_cache_ptr::copy(untyped_immutable_cache_ptr const& other)
{
    record_ = other.record_;
    if (record_)
        detail::acquire_cache_record(record_);
    key_ = other.key_;
}

void
untyped_immutable_cache_ptr::move_in(untyped_immutable_cache_ptr&& other)
{
    record_ = other.record_;
    other.record_ = nullptr;
    key_ = std::move(other.key_);
}

} // namespace detail

} // namespace cradle
