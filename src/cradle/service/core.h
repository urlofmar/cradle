#ifndef CRADLE_SERVICE_CORE_H
#define CRADLE_SERVICE_CORE_H

#include <memory>

#include <cppcoro/task.hpp>

#include <cradle/io/http_requests.hpp>

namespace cradle {

namespace detail {

struct service_core_internals;

}

struct service_core
{
    service_core();
    ~service_core();

    detail::service_core_internals&
    internals()
    {
        return *impl_;
    }

 private:
    std::unique_ptr<detail::service_core_internals> impl_;
};

cppcoro::task<http_response>
async_http_request(service_core& core, http_request request);

} // namespace cradle

#endif
