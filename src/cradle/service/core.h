#ifndef CRADLE_SERVICE_CORE_H
#define CRADLE_SERVICE_CORE_H

#include <memory>

#include <cppcoro/task.hpp>

#include <cradle/caching/immutable.h>
#include <cradle/io/http_requests.hpp>
#include <cradle/service/internals.h>
#include <cradle/service/types.hpp>

namespace cradle {

namespace detail {

struct service_core_internals;

}

struct service_core
{
    service_core()
    {
    }
    service_core(service_config const& config)
    {
        reset(config);
    }
    ~service_core();

    void
    reset();
    void
    reset(service_config const& config);

    detail::service_core_internals&
    internals()
    {
        return *impl_;
    }

 private:
    std::unique_ptr<detail::service_core_internals> impl_;
};

http_connection_interface&
http_connection_for_thread();

cppcoro::task<http_response>
async_http_request(service_core& core, http_request request);

cppcoro::task<dynamic>
disk_cached(service_core& core, std::string key, cppcoro::task<dynamic> task);

namespace detail {

template<class Value>
struct cached_task_creator
{
    cppcoro::task<Value>* task;

    cppcoro::task<Value>
    operator()() const
    {
        return std::move(*task);
    }
};

} // namespace detail

template<class Value, class Key>
cppcoro::shared_task<Value>
cached(service_core& core, Key key, cppcoro::task<Value> task)
{
    immutable_cache_ptr<Value> ptr(
        core.internals().cache,
        key,
        detail::cached_task_creator<Value>{&task});
    return ptr.task();
}

} // namespace cradle

#endif
