#include <cradle/service/core.h>

#include <thread>

// Boost.Crc triggers some warnings on MSVC.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4245)
#pragma warning(disable : 4701)
#include <boost/crc.hpp>
#pragma warning(pop)
#else
#include <boost/crc.hpp>
#endif

#include <spdlog/spdlog.h>

#include <cppcoro/schedule_on.hpp>

#include <cradle/encodings/base64.h>
#include <cradle/encodings/lz4.h>
#include <cradle/encodings/native.h>
#include <cradle/fs/file_io.h>
#include <cradle/service/internals.h>

namespace cradle {

void
service_core::reset()
{
    impl_.reset();
}

void
service_core::reset(service_config const& config)
{
    impl_.reset(new detail::service_core_internals{
        .cache = immutable_cache(
            config.immutable_cache ? *config.immutable_cache
                                   : immutable_cache_config(0x40'00'00'00)),
        .compute_pool = cppcoro::static_thread_pool(
            config.compute_concurrency ? *config.compute_concurrency
                                       : std::thread::hardware_concurrency()),
        .http_pool = cppcoro::static_thread_pool(
            config.http_concurrency ? *config.http_concurrency : 24),
        .disk_cache = disk_cache(
            config.disk_cache ? *config.disk_cache
                              : disk_cache_config(none, 0x1'00'00'00'00)),
        .disk_read_pool = cppcoro::static_thread_pool(2),
        .disk_write_pool = thread_pool(2)});
}

service_core::~service_core()
{
}

http_connection_interface&
http_connection_for_thread()
{
    static http_request_system the_system;
    static thread_local http_connection the_connection(the_system);
    return the_connection;
}

cppcoro::task<http_response>
async_http_request(service_core& core, http_request request)
{
    co_await core.internals().http_pool.schedule();
    null_check_in check_in;
    null_progress_reporter reporter;
    co_return http_connection_for_thread().perform_request(
        check_in, reporter, request);
}

cppcoro::task<std::string>
read_file_contents(service_core& core, file_path const& path)
{
    co_await core.internals().disk_read_pool.schedule();
    co_return read_file_contents(path);
}

cppcoro::task<dynamic>
disk_cached(service_core& core, std::string key, cppcoro::task<dynamic> task)
{
    // Check the cache for an existing value.
    auto& cache = core.internals().disk_cache;
    try
    {
        auto entry = cache.find(key);
        if (entry)
        {
            if (entry->value)
            {
                auto natively_encoding = base64_decode(
                    *entry->value, get_mime_base64_character_set());
                co_return read_natively_encoded_value(
                    reinterpret_cast<uint8_t const*>(natively_encoding.data()),
                    natively_encoding.size());
            }
            else
            {
                auto data = co_await read_file_contents(
                    core, cache.get_path_for_id(entry->id));

                std::unique_ptr<uint8_t[]> decompressed_data(
                    new uint8_t[entry->original_size]);
                lz4::decompress(
                    decompressed_data.get(),
                    entry->original_size,
                    data.data(),
                    data.size());

                boost::crc_32_type crc;
                crc.process_bytes(
                    decompressed_data.get(), entry->original_size);
                if (crc.checksum() == entry->crc32)
                {
                    spdlog::get("cradle")->info("disk cache hit on {}", key);
                    co_return read_natively_encoded_value(
                        decompressed_data.get(), entry->original_size);
                }
            }
        }
    }
    catch (...)
    {
        // Something went wrong trying to load the cached value, so just
        // pretend it's not there. (It will be overwritten.)
        spdlog::get("cradle")->warn("error on disk cache entry {}", key);
    }
    spdlog::get("cradle")->info("disk cache miss on {}", key);

    // We didn't get it from the cache, so actually invoke the task to compute
    // the result.
    auto result = co_await task;

    // Cache the result.
    core.internals().disk_write_pool.push_task([&core, key, result] {
        auto& cache = core.internals().disk_cache;
        try
        {
            auto encoded_data = write_natively_encoded_value(result);
            if (encoded_data.size() > 1024)
            {
                size_t max_compressed_size
                    = lz4::max_compressed_size(encoded_data.size());

                std::unique_ptr<uint8_t[]> compressed_data(
                    new uint8_t[max_compressed_size]);
                size_t actual_compressed_size = lz4::compress(
                    compressed_data.get(),
                    max_compressed_size,
                    encoded_data.data(),
                    encoded_data.size());

                auto cache_id = cache.initiate_insert(key);
                {
                    auto entry_path = cache.get_path_for_id(cache_id);
                    std::ofstream output;
                    open_file(
                        output,
                        entry_path,
                        std::ios::out | std::ios::trunc | std::ios::binary);
                    output.write(
                        reinterpret_cast<char const*>(compressed_data.get()),
                        actual_compressed_size);
                }
                boost::crc_32_type crc;
                crc.process_bytes(encoded_data.data(), encoded_data.size());
                cache.finish_insert(
                    cache_id, crc.checksum(), encoded_data.size());
            }
            else
            {
                cache.insert(
                    key,
                    base64_encode(
                        encoded_data.data(),
                        encoded_data.size(),
                        get_mime_base64_character_set()));
            }
        }
        catch (...)
        {
            // Something went wrong trying to write the cached value, so issue
            // a warning and move on.
            spdlog::get("cradle")->warn(
                "error writing disk cache entry {}", key);
        }
    });

    co_return result;
}

} // namespace cradle
