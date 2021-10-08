#ifndef CRADLE_SERVICE_CORE_H
#define CRADLE_SERVICE_CORE_H

#include <memory>

#include <cppcoro/task.hpp>

#include <cradle/io/http_requests.hpp>
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

} // namespace cradle

#endif
