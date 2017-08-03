#ifndef CRADLE_DISK_CACHE_HPP
#define CRADLE_DISK_CACHE_HPP

#include <cradle/io/file.hpp>
#include <cradle/cache_types.hpp>

#include <vector>

namespace cradle {

// A disk cache is used for caching immutable data on the local hard drive to
// avoid redownloading it or recomputing it.

// The cache is implemented as a directory of files with an SQLite index
// database file that aids in tracking usage information.

// Note that a disk cache will generate exceptions any time an operation fails.
// Of course, since caching is by definition not essential to the correct
// operation of a program, there should always be a way to recover from these
// exceptions.

// A cache is internally protected by a mutex, so it can be used concurrently
// from multiple threads.

// This exception indicates a failure in the operation of the disk cache.
CRADLE_DEFINE_EXCEPTION(disk_cache_failure)
// This provides the path to the disk cache directory.
CRADLE_DEFINE_ERROR_INFO(file_path, disk_cache_path)
// This exception also provides internal_error_message_info.

// This exception indicates an attempt to use an uninitialized disk cache.
CRADLE_DEFINE_EXCEPTION(disk_cache_uninitialized)

struct disk_cache_interface
{
    virtual ~disk_cache_interface() {}

    // Reset the cache with a new config.
    // After a successful call to this, the cache is considered initialized.
    virtual void
    reset(disk_cache_config const& config) = 0;

    // Reset the cache to an uninitialized state.
    virtual void
    reset() = 0;

    // Is the cache initialized?
    virtual bool
    is_initialized() = 0;

    // The rest of this interface should only be used if is_initialized() returns true.
    // It will throw an exception otherwise.

    // Get summary information about the cache.
    virtual disk_cache_info
    get_summary_info() = 0;

    // Get a list of all entries in the cache.
    virtual std::vector<disk_cache_entry>
    get_entry_list() = 0;

    // Remove an individual entry from the cache.
    virtual void
    remove_entry(int64_t id) = 0;

    // Clear the cache of all data.
    virtual void
    clear() = 0;

    // Check if the given key exists in the cache.
    // If it does, the 64-bit ID associated with the key is written to *id.
    // Also, if crc32 is not null, *crc32 receives the 32-bit CRC of the entry.
    virtual bool
    entry_exists(
        string const& key,
        int64_t* id,
        uint32_t* crc32 = nullptr) = 0;

    // Adding an entry to the cache is a two-part process.
    // First, you initiate the insert to get the ID for the entry.
    // Then, once the entry is written to disk, you finish the insert.
    // (If an error occurs, it's OK to simply abandon the entry, as it will be
    // marked as invalid initially.)
    virtual int64_t
    initiate_insert(string const& key) = 0;
    virtual void
    finish_insert(int64_t id, uint32_t crc32) = 0;

    // Given an ID within the cache, this computes the path of the file that would
    // store the data associated with that ID.
    virtual file_path
    get_path_for_id(int64_t id) = 0;

    // Record that an ID within the cache was just used.
    // When a lot of small objects are being read from the cache, the calls to
    // record_usage() can significantly slow down the loading process, as they
    // require locking the database file.
    // To address this, calls are buffered and sent all at once when the cache is
    // idle.
    virtual void
    record_usage(int64_t id) = 0;

    // If you know that the cache is idle, you can call this to force the cache to
    // write out its buffered usage records.
    // (This is automatically called when the cache is destructed.)
    virtual void
    write_usage_records() = 0;

    // Another approach is to call this function periodically.
    // It checks to see how long it's been since the cache was last used, and if
    // the cache appears idle, it automatically writes the usage records.
    virtual void
    do_idle_processing() = 0;
};

struct disk_cache_impl;

struct disk_cache : disk_cache_interface, boost::noncopyable
{
    // The default constructor creates an invalid disk cache that must be initialized via reset().
    disk_cache();

    // Create a disk cache that's initialized with the given config.
    disk_cache(disk_cache_config const& config);

    ~disk_cache();

    void
    reset(disk_cache_config const& config);

    void
    reset();

    bool
    is_initialized();

    disk_cache_info
    get_summary_info();

    std::vector<disk_cache_entry>
    get_entry_list();

    void
    remove_entry(int64_t id);

    void
    clear();

    bool
    entry_exists(
        string const& key,
        int64_t* id,
        uint32_t* crc32 = nullptr);

    int64_t
    initiate_insert(string const& key);
    void
    finish_insert(int64_t id, uint32_t crc32);

    file_path
    get_path_for_id(int64_t id);

    void
    record_usage(int64_t id);

    void
    write_usage_records();

    void
    do_idle_processing();

 private:
    disk_cache_impl* impl_;
};

}

#endif
