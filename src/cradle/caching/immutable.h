#ifndef CRADLE_CACHING_IMMUTABLE_H
#define CRADLE_CACHING_IMMUTABLE_H

// This file provides a small framework for caching immutable data in memory.
//
// The cache is designed with the following requirements in mind:
//
// - Cached data may be large.
//
// - Data may take time to retrieve or compute. This must be allowed to happen
//   concurrently as callers continue to access the cache.
//
// - Multiple callers may be interested in the same data.
//   - These calls may come concurrently from different threads.
//   - This must not result in duplicated data instances or retrieval/
//     computation tasks.
//
// - Callers will be interested in individual data objects for some period of
//   time. (The API must allow them to indicate this duration by holding a
//   handle/pointer to data that they're interested in. Ideally, this should
//   simply be a std::shared_ptr.)
//
// - It is useful to retain data that is no longer in use.
//   - But this must be subject to constraints on total memory used for this
//     effort (or the amount that remains available to the system).
//
// - A single cache instance should be able to store heterogenous data types
//   while preserving type safety for callers.
//
// - Efforts to retrieve or compute data may fail.
//
// - The cache must provide an inspection interface.
//
// This version of the cache assumes that callers are always cppcoro
// coroutines. It uses the mechanics of cppcoro to issue tasks to
// compute/retrieve data and to wait for values to appear.
//
// The keys to the cache are CRADLE IDs, which allows for efficient usage of
// heterogenous keys (without necessarily resorting to string
// conversion/hashes).
//

#include <cradle/caching/immutable/cache.hpp>
#include <cradle/caching/immutable/ptr.h>

#endif
