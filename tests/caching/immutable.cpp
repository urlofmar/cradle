#include <cradle/caching/immutable.h>

#include <sstream>

#include <cppcoro/sync_wait.hpp>

#include <cradle/core/immutable.h>
#include <cradle/utilities/testing.h>

using namespace cradle;

namespace {

// Sort the entry lists in a cache snapshot so that it's consistent across
// unordered_map implementations.
immutable_cache_snapshot
sort_cache_snapshot(immutable_cache_snapshot snapshot)
{
    auto comparator = [](immutable_cache_entry_snapshot const& a,
                         immutable_cache_entry_snapshot const& b) {
        return a.key < b.key;
    };
    sort(snapshot.in_use.begin(), snapshot.in_use.end(), comparator);
    sort(
        snapshot.pending_eviction.begin(),
        snapshot.pending_eviction.end(),
        comparator);
    return snapshot;
}

cppcoro::task<int>
test_task(int the_answer)
{
    co_return the_answer;
}

template<class Value>
Value
await_cache_value(immutable_cache_ptr<Value>& ptr)
{
    return cppcoro::sync_wait(ptr.task());
}

} // namespace

TEST_CASE("basic immutable cache usage", "[immutable_cache]")
{
    immutable_cache cache;
    {
        INFO("Cache reset() and is_initialized() work as expected.");
        REQUIRE(!cache.is_initialized());
        cache.reset(immutable_cache_config(1024));
        REQUIRE(cache.is_initialized());
        cache.reset();
        REQUIRE(!cache.is_initialized());
        cache.reset(immutable_cache_config(1024));
        REQUIRE(cache.is_initialized());
    }

    immutable_cache_ptr<int> p;
    {
        INFO("A default-constructed immutable_cache_ptr is uninitialized.");
        REQUIRE(!p.is_initialized());
    }

    {
        INFO(
            "The first time an immutable_cache_ptr is attached to a key, its"
            ":create_job callback is invoked.");
        bool p_needed_creation = false;
        p.reset(cache, make_id(0), [&] {
            p_needed_creation = true;
            return test_task(42);
        });
        REQUIRE(p_needed_creation);
        // Also check all that all the ptr accessors work.
        REQUIRE(p.is_initialized());
        REQUIRE(p.is_loading());
        REQUIRE(!p.is_ready());
        REQUIRE(!p.is_failed());
        REQUIRE(p.key() == make_id(0));
    }

    {
        INFO("get_cache_snapshot reflects that entry 0 is loading.");
        REQUIRE(
            sort_cache_snapshot(get_cache_snapshot(cache))
            == (immutable_cache_snapshot{
                {{"0", immutable_cache_entry_state::LOADING, 0}}, {}}));
    }

    bool q_needed_creation = false;
    immutable_cache_ptr<int> q(cache, make_id(1), [&] {
        q_needed_creation = true;
        return test_task(112);
    });
    {
        INFO(
            "The first time an immutable_cache_ptr is attached to a new key, "
            "its :create_job callback is invoked.");
        REQUIRE(q_needed_creation);
    }
    // Also check all that all the ptr accessors work.
    REQUIRE(q.is_initialized());
    REQUIRE(!q.is_ready());
    REQUIRE(q.key() == make_id(1));

    {
        INFO(
            "get_cache_snapshot reflects that there are two entries loading.");
        REQUIRE(
            sort_cache_snapshot(get_cache_snapshot(cache))
            == (immutable_cache_snapshot{
                {{"0", immutable_cache_entry_state::LOADING, 0},
                 {"1", immutable_cache_entry_state::LOADING, 0}},
                {}}));
    }

    bool r_needed_creation = false;
    immutable_cache_ptr<int> r(cache, make_id(0), [&] {
        r_needed_creation = true;
        return test_task(42);
    });
    {
        INFO(
            "The second time an immutable_cache_ptr is attached to a key, "
            "its :create_job callback is NOT invoked.");
        REQUIRE(!r_needed_creation);
    }

    {
        INFO("get_cache_snapshot shows no change.");
        REQUIRE(
            sort_cache_snapshot(get_cache_snapshot(cache))
            == (immutable_cache_snapshot{
                {{"0", immutable_cache_entry_state::LOADING, 0},
                 {"1", immutable_cache_entry_state::LOADING, 0}},
                {}}));
    }

    {
        INFO("immutable_cache_ptr copying works as expected.");
        p = q;
        REQUIRE(p.is_initialized());
        REQUIRE(!p.is_ready());
        REQUIRE(p.key() == make_id(1));
    }

    {
        INFO(
            "When a cache pointer is waited on, this triggers production of "
            "the value. The value is correctly received and reflected in the "
            "cache snapshot.");
        REQUIRE(await_cache_value(p) == 112);
        REQUIRE(
            sort_cache_snapshot(get_cache_snapshot(cache))
            == (immutable_cache_snapshot{
                {{"0", immutable_cache_entry_state::LOADING, 0},
                 {"1", immutable_cache_entry_state::READY, sizeof(int)}},
                {}}));
        REQUIRE(p.is_ready());
        REQUIRE(q.is_ready());
    }

    {
        INFO("immutable_cache_ptr movement works as expected.");

        immutable_cache_ptr<int> inner_p = std::move(p);
        REQUIRE(inner_p.is_initialized());
        REQUIRE(inner_p.is_ready());
        REQUIRE(await_cache_value(inner_p) == 112);
        REQUIRE(!p.is_initialized());
    }

    {
        INFO(
            "q wasn't affected by p having its contents moved out, even "
            "though it referenced the same entry.");

        REQUIRE(q.is_ready());
        REQUIRE(await_cache_value(q) == 112);
    }

    {
        INFO(
            "The cache snapshot also still reflects that q's value is in "
            "use.");

        REQUIRE(
            sort_cache_snapshot(get_cache_snapshot(cache))
            == (immutable_cache_snapshot{
                {{"0", immutable_cache_entry_state::LOADING, 0},
                 {"1", immutable_cache_entry_state::READY, sizeof(int)}},
                {}}));
    }

    {
        INFO(
            "Resetting q will change the snapshot to reflect that q's value "
            "is now pending eviction.");

        q.reset();

        REQUIRE(
            sort_cache_snapshot(get_cache_snapshot(cache))
            == (immutable_cache_snapshot{
                {{"0", immutable_cache_entry_state::LOADING, 0}},
                {{"1", immutable_cache_entry_state::READY, sizeof(int)}}}));
    }

    {
        INFO("Recreating q will retrieve the entry from the eviction list.");

        q.reset(cache, make_id(1), [&] { return test_task(112); });
        REQUIRE(q.is_ready());
        REQUIRE(await_cache_value(q) == 112);

        REQUIRE(
            sort_cache_snapshot(get_cache_snapshot(cache))
            == (immutable_cache_snapshot{
                {{"0", immutable_cache_entry_state::LOADING, 0},
                 {"1", immutable_cache_entry_state::READY, sizeof(int)}},
                {}}));
    }

    {
        INFO("Resetting q again will put it back into the eviction list.");

        q.reset();

        REQUIRE(
            sort_cache_snapshot(get_cache_snapshot(cache))
            == (immutable_cache_snapshot{
                {{"0", immutable_cache_entry_state::LOADING, 0}},
                {{"1", immutable_cache_entry_state::READY, sizeof(int)}}}));
    }

    {
        INFO(
            "Clearing unused entries in the cache will clear out q's old "
            "value.");

        clear_unused_entries(cache);

        REQUIRE(
            sort_cache_snapshot(get_cache_snapshot(cache))
            == (immutable_cache_snapshot{
                {{"0", immutable_cache_entry_state::LOADING, 0}}, {}}));
    }
}

TEST_CASE("immutable cache LRU eviction", "[immutable_cache]")
{
    // Initialize the cache with 1.5kB of space for unused data.
    immutable_cache cache(immutable_cache_config(1536));

    auto one_kb_string_task = [](char content) -> cppcoro::task<std::string> {
        co_return std::string(1024, content);
    };

    // Declare an interest in ID(1).
    bool p_needed_creation = false;
    immutable_cache_ptr<std::string> p(cache, make_id(1), [&] {
        p_needed_creation = true;
        return one_kb_string_task('a');
    });
    REQUIRE(p_needed_creation);
    REQUIRE(await_cache_value(p) == std::string(1024, 'a'));

    // Declare an interest in ID(2).
    bool q_needed_creation = false;
    immutable_cache_ptr<std::string> q(cache, make_id(2), [&] {
        q_needed_creation = true;
        return one_kb_string_task('b');
    });
    REQUIRE(q_needed_creation);
    REQUIRE(await_cache_value(q) == std::string(1024, 'b'));

    // Revoke interest in both IDs.
    // Since only one will fit in the cache, this should evict ID(1).
    p.reset();
    q.reset();

    // If we redeclare interest in ID(1), it should require creation.
    bool r_needed_creation = false;
    immutable_cache_ptr<std::string> r(cache, make_id(1), [&] {
        r_needed_creation = true;
        return one_kb_string_task('a');
    });
    REQUIRE(r_needed_creation);
    REQUIRE(!r.is_ready());
    REQUIRE(await_cache_value(r) == std::string(1024, 'a'));

    // If we redeclare interest in ID(2), it should NOT require creation.
    bool s_needed_creation = false;
    immutable_cache_ptr<std::string> s(cache, make_id(2), [&] {
        s_needed_creation = true;
        return one_kb_string_task('b');
    });
    REQUIRE(!s_needed_creation);
    REQUIRE(s.is_ready());
    REQUIRE(await_cache_value(s) == std::string(1024, 'b'));
}
