#ifndef CRADLE_THINKNODE_SUPERVISOR_H
#define CRADLE_THINKNODE_SUPERVISOR_H

#include <cradle/core.h>
#include <cradle/service/core.h>
#include <cradle/thinknode/types.hpp>

namespace cradle {

// This exception indicates a failure in the execution of a local calculation.
CRADLE_DEFINE_EXCEPTION(local_calculation_failure)
// TODO: Provide exception info.

// Execute a local Thinknode calculation by invoking a provider via Docker.
dynamic
supervise_thinknode_calculation(
    service_core& service,
    string const& account,
    string const& app,
    thinknode_provider_image_info const& image,
    string const& function_name,
    std::vector<dynamic> args);

} // namespace cradle

#endif
