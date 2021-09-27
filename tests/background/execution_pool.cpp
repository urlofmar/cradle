#include <cradle/background/execution_pool.h>

#include <sstream>

#include <cradle/background/testing.h>
#include <cradle/utilities/testing.h>

using namespace cradle;

struct basic_test_job : background_job_interface
{
    std::atomic<bool>* completed = nullptr;

    basic_test_job(std::atomic<bool>* completed) : completed(completed)
    {
    }

    void
    execute(
        check_in_interface& check_in,
        progress_reporter_interface& reporter) override
    {
        *this->completed = true;
    }
};

struct delayed_test_job : background_job_interface
{
    std::atomic<bool>* allowed_to_complete = nullptr;
    std::atomic<bool>* completed = nullptr;

    delayed_test_job(
        std::atomic<bool>* allowed_to_complete, std::atomic<bool>* completed)
        : allowed_to_complete(allowed_to_complete), completed(completed)
    {
    }

    void
    execute(
        check_in_interface& check_in,
        progress_reporter_interface& reporter) override
    {
        while (!*this->allowed_to_complete)
        {
            std::this_thread::yield();
            check_in();
        }
        *this->completed = true;
    }
};

TEST_CASE("basic execution pool usage", "[background]")
{
    detail::background_execution_pool pool;
    detail::initialize_pool<detail::basic_executor>(
        pool, 1, [] { return detail::basic_executor(); });
    REQUIRE(occurs_soon([&] { return is_pool_idle(pool); }));

    std::atomic<bool> completed = false;
    auto job_ptr = std::make_unique<basic_test_job>(&completed);
    detail::add_background_job(pool, std::move(job_ptr));

    REQUIRE(occurs_soon([&]() -> bool { return completed; }));

    detail::shut_down_pool(pool);
}

TEST_CASE("on-demand threads in execution pool", "[background]")
{
    detail::background_execution_pool pool;
    detail::initialize_pool<detail::basic_executor>(
        pool, 0, [] { return detail::basic_executor(); });
    REQUIRE(occurs_soon([&] { return is_pool_idle(pool); }));

    std::atomic<bool> completed = false;
    auto job_ptr = std::make_unique<basic_test_job>(&completed);
    detail::add_background_job(
        pool, std::move(job_ptr), BACKGROUND_JOB_SKIP_QUEUE);

    REQUIRE(occurs_soon([&]() -> bool { return completed; }));

    detail::shut_down_pool(pool);
}

TEST_CASE("job cancellation", "[background]")
{
    detail::background_execution_pool pool;
    detail::initialize_pool<detail::basic_executor>(
        pool, 1, [] { return detail::basic_executor(); });
    REQUIRE(occurs_soon([&] { return is_pool_idle(pool); }));

    // Add the job (but don't allow it to complete).
    std::atomic<bool> allowed_to_complete = false;
    std::atomic<bool> completed = false;
    auto job_ptr = std::make_unique<delayed_test_job>(
        delayed_test_job(&allowed_to_complete, &completed));
    auto controller = detail::add_background_job(pool, std::move(job_ptr));
    REQUIRE(controller.is_valid());
    auto initial_state = controller.state();
    REQUIRE(
        (initial_state == background_job_state::QUEUED
         || initial_state == background_job_state::RUNNING));

    // Wait to see it start running.
    REQUIRE(occurs_soon(
        [&] { return controller.state() == background_job_state::RUNNING; }));

    REQUIRE(!is_pool_idle(pool));

    // Tell the job to cancel.
    controller.cancel();

    // Wait to see it cancel.
    REQUIRE(occurs_soon(
        [&] { return controller.state() == background_job_state::CANCELED; }));

    // Check that everything is consistent.
    REQUIRE(!completed);
    REQUIRE(occurs_soon([&] { return is_pool_idle(pool); }));

    detail::shut_down_pool(pool);
}
