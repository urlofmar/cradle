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

#include <boost/lexical_cast.hpp>

#include <spdlog/spdlog.h>

#include <cppcoro/schedule_on.hpp>

#include <cradle/encodings/base64.h>
#include <cradle/encodings/lz4.h>
#include <cradle/encodings/native.h>
#include <cradle/fs/file_io.h>
#include <cradle/fs/utilities.h>
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
http_connection_for_thread(service_core& core)
{
    if (core.internals().mock_http)
    {
        thread_local mock_http_connection the_connection(
            *core.internals().mock_http);
        return the_connection;
    }
    else
    {
        static http_request_system the_system;
        thread_local http_connection the_connection(the_system);
        return the_connection;
    }
}

cppcoro::task<http_response>
async_http_request(service_core& core, http_request request)
{
    co_await core.internals().http_pool.schedule();
    null_check_in check_in;
    null_progress_reporter reporter;
    co_return http_connection_for_thread(core).perform_request(
        check_in, reporter, request);
}

cppcoro::task<std::string>
read_file_contents(service_core& core, file_path const& path)
{
    co_await core.internals().disk_read_pool.schedule();
    co_return read_file_contents(path);
}

namespace detail {

namespace {

void
serialize(blob* dst, blob src)
{
    *dst = std::move(src);
}

void
deserialize(blob* dst, std::string src)
{
    *dst = make_blob(std::move(src));
}

void
deserialize(blob* dst, std::unique_ptr<uint8_t[]> ptr, size_t size)
{
    dst->data = reinterpret_cast<char const*>(ptr.get());
    dst->ownership = std::shared_ptr<uint8_t[]>{std::move(ptr)};
    dst->size = size;
}

void
serialize(blob* dst, dynamic src)
{
    *dst = make_blob(write_natively_encoded_value(src));
}

void
deserialize(dynamic* dst, string x)
{
    *dst = read_natively_encoded_value(
        reinterpret_cast<uint8_t const*>(x.data()), x.size());
}

void
deserialize(dynamic* dst, std::unique_ptr<uint8_t[]> ptr, size_t size)
{
    *dst = read_natively_encoded_value(ptr.get(), size);
}

} // namespace

} // namespace detail

template<class T>
cppcoro::task<T>
generic_disk_cached(
    service_core& core,
    std::string key,
    std::function<cppcoro::task<T>()> create_task)
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
                auto natively_encoded_data = base64_decode(
                    *entry->value, get_mime_base64_character_set());

                T x;
                detail::deserialize(&x, std::move(natively_encoded_data));
                co_return x;
            }
            else
            {
                spdlog::get("cradle")->info("disk cache hit on {}", key);

                spdlog::get("cradle")->info("reading file", key);
                auto data = co_await read_file_contents(
                    core, cache.get_path_for_id(entry->id));

                spdlog::get("cradle")->info("decompressing", key);
                auto original_size
                    = boost::numeric_cast<size_t>(entry->original_size);
                std::unique_ptr<uint8_t[]> decompressed_data(
                    new uint8_t[original_size]);
                lz4::decompress(
                    decompressed_data.get(),
                    original_size,
                    data.data(),
                    data.size());

                spdlog::get("cradle")->info("checking CRC", key);
                boost::crc_32_type crc;
                crc.process_bytes(decompressed_data.get(), original_size);
                if (crc.checksum() == entry->crc32)
                {
                    spdlog::get("cradle")->info("decoding", key);
                    T decoded;
                    detail::deserialize(
                        &decoded, std::move(decompressed_data), original_size);
                    spdlog::get("cradle")->info("returning", key);
                    co_return decoded;
                }
            }
        }
    }
    catch (...)
    {
        // Something went wrong trying to load the cached value, so just
        // pretend it's not there. (It will be overwritten.)
        spdlog::get("cradle")->warn("error reading disk cache entry {}", key);
    }
    spdlog::get("cradle")->info("disk cache miss on {}", key);

    // We didn't get it from the cache, so actually create the task to compute
    // the result.
    auto result = co_await create_task();

    // Cache the result.
    core.internals().disk_write_pool.push_task([&core, key, result] {
        auto& cache = core.internals().disk_cache;
        try
        {
            blob encoded_data;
            detail::serialize(&encoded_data, std::move(result));
            if (encoded_data.size > 1024)
            {
                size_t max_compressed_size
                    = lz4::max_compressed_size(encoded_data.size);

                std::unique_ptr<uint8_t[]> compressed_data(
                    new uint8_t[max_compressed_size]);
                size_t actual_compressed_size = lz4::compress(
                    compressed_data.get(),
                    max_compressed_size,
                    encoded_data.data,
                    encoded_data.size);

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
                crc.process_bytes(encoded_data.data, encoded_data.size);
                cache.finish_insert(
                    cache_id, crc.checksum(), encoded_data.size);
            }
            else
            {
                cache.insert(
                    key,
                    base64_encode(
                        reinterpret_cast<uint8_t const*>(encoded_data.data),
                        encoded_data.size,
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

cppcoro::task<dynamic>
disk_cached(
    service_core& core,
    std::string key,
    std::function<cppcoro::task<dynamic>()> create_task)
{
    return generic_disk_cached<dynamic>(
        core, std::move(key), std::move(create_task));
}

cppcoro::task<dynamic>
disk_cached(
    service_core& core,
    id_interface const& key,
    std::function<cppcoro::task<dynamic>()> create_task)
{
    return generic_disk_cached<dynamic>(
        core, boost::lexical_cast<std::string>(key), std::move(create_task));
}

cppcoro::task<blob>
disk_cached(
    service_core& core,
    std::string key,
    std::function<cppcoro::task<blob>()> create_task)
{
    return generic_disk_cached<blob>(
        core, std::move(key), std::move(create_task));
}

cppcoro::task<blob>
disk_cached(
    service_core& core,
    id_interface const& key,
    std::function<cppcoro::task<blob>()> create_task)
{
    return generic_disk_cached<blob>(
        core, boost::lexical_cast<std::string>(key), std::move(create_task));
}

void
init_test_service(service_core& core)
{
    auto cache_dir = file_path("service_disk_cache");

    reset_directory(cache_dir);

    core.reset(service_config(
        immutable_cache_config(0x40'00'00'00),
        disk_cache_config(some(cache_dir.string()), 0x40'00'00'00),
        2,
        2,
        2));
}

mock_http_session&
enable_http_mocking(service_core& core)
{
    core.internals().mock_http = std::make_unique<mock_http_session>();
    return *core.internals().mock_http;
}

} // namespace cradle
