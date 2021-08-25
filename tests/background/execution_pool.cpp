#include <cradle/background/execution_pool.h>

#include <sstream>

#include <cradle/utilities/testing.h>

using namespace cradle;

struct test_job : background_job_interface
{
    std::atomic<bool>* executed = nullptr;

    test_job(std::atomic<bool>* executed) : executed(executed)
    {
    }

    void
    execute(
        check_in_interface& check_in,
        progress_reporter_interface& reporter) override
    {
        *this->executed = true;
    }
};

TEST_CASE("basic execution pool usage", "[background]")
{
    detail::background_execution_pool pool;
    detail::initialize_pool<detail::basic_executor>(
        pool, 1, [] { return detail::basic_executor(); });
    std::atomic<bool> executed = false;
    auto job_ptr = std::make_unique<test_job>(&executed);
    detail::add_background_job(pool, std::move(job_ptr));
    int n = 0;
    while (n < 100 && !executed)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ++n;
    }
    REQUIRE(executed);
    detail::shut_down_pool(pool);
}

TEST_CASE("on-demand threads in execution pool", "[background]")
{
    detail::background_execution_pool pool;
    detail::initialize_pool<detail::basic_executor>(
        pool, 0, [] { return detail::basic_executor(); });
    std::atomic<bool> executed = false;
    auto job_ptr = std::make_unique<test_job>(&executed);
    detail::add_background_job(
        pool, std::move(job_ptr), BACKGROUND_JOB_SKIP_QUEUE);
    int n = 0;
    while (n < 100 && !executed)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ++n;
    }
    REQUIRE(executed);
    detail::shut_down_pool(pool);
}
