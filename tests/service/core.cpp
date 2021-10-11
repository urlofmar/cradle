#include <cradle/service/core.h>

#include <cppcoro/sync_wait.hpp>

#include <filesystem>

#include <cradle/fs/utilities.h>
#include <cradle/service/internals.h>
#include <cradle/utilities/concurrency_testing.h>

#include <cradle/utilities/testing.h>

using namespace cradle;

namespace {

void
init_test_service(service_core& core)
{
    auto cache_dir = file_path("service_disk_cache");

    reset_directory(cache_dir);

    core.reset(service_config(
        immutable_cache_config(0x40'00'00'00),
        disk_cache_config(some(cache_dir.string()), 0x40'00'00'00)));
}

} // namespace

TEST_CASE("HTTP requests", "[service][core]")
{
    service_core core;
    init_test_service(core);

    auto async_response = async_http_request(
        core,
        make_get_request(
            "http://postman-echo.com/get?color=navy", http_header_list()));

    auto response = cppcoro::sync_wait(async_response);
    REQUIRE(response.status_code == 200);
    auto body = parse_json_response(response);
    REQUIRE(
        get_field(cast<dynamic_map>(body), "args")
        == dynamic({{"color", "navy"}}));
}

TEST_CASE("small value disk caching", "[service][core]")
{
    service_core core;
    init_test_service(core);

    int execution_count = 0;
    auto counted_task = [&](int answer) -> cppcoro::task<dynamic> {
        ++execution_count;
        co_return dynamic(integer(answer));
    };

    {
        auto result = disk_cached(core, "id_12", counted_task(12));
        REQUIRE(cppcoro::sync_wait(result) == dynamic(integer(12)));
        REQUIRE(execution_count == 1);
    }
    {
        auto result = disk_cached(core, "id_42", counted_task(42));
        REQUIRE(cppcoro::sync_wait(result) == dynamic(integer(42)));
        REQUIRE(execution_count == 2);
    }
    // Data is written to the disk cache in a background thread, so we need to
    // wait for that to finish.
    REQUIRE(occurs_soon([&] {
        return core.internals().disk_write_pool.get_tasks_total() == 0;
    }));
    // Now redo the 'id_12' task to see that it's not actually rerun.
    {
        auto result = disk_cached(core, "id_12", counted_task(12));
        REQUIRE(cppcoro::sync_wait(result) == dynamic(integer(12)));
        REQUIRE(execution_count == 2);
    }
}

TEST_CASE("large value disk caching", "[service][core]")
{
    service_core core;
    init_test_service(core);

    auto generate_random_data = [](uint32_t seed) {
        std::vector<integer> result;
        for (int i = 0; i < 4096; ++i)
        {
            std::minstd_rand eng(seed);
            std::uniform_int_distribution<integer> dist(0, 0x1'0000'0000);
            result.push_back(dist(eng));
        }
        return result;
    };

    int execution_count = 0;
    auto counted_task = [&](uint32_t seed) -> cppcoro::task<dynamic> {
        ++execution_count;
        co_return to_dynamic(generate_random_data(seed));
    };

    {
        auto result = disk_cached(core, "id_12", counted_task(12));
        REQUIRE(
            cppcoro::sync_wait(result)
            == to_dynamic(generate_random_data(12)));
        REQUIRE(execution_count == 1);
    }
    {
        auto result = disk_cached(core, "id_42", counted_task(42));
        REQUIRE(
            cppcoro::sync_wait(result)
            == to_dynamic(generate_random_data(42)));
        REQUIRE(execution_count == 2);
    }
    // Data is written to the disk cache in a background thread, so we need to
    // wait for that to finish.
    REQUIRE(occurs_soon([&] {
        return core.internals().disk_write_pool.get_tasks_total() == 0;
    }));
    // Now redo the 'id_12' task to see that it's not actually rerun.
    {
        auto result = disk_cached(core, "id_12", counted_task(12));
        REQUIRE(
            cppcoro::sync_wait(result)
            == to_dynamic(generate_random_data(12)));
        REQUIRE(execution_count == 2);
    }
}
