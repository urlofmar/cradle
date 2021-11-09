#ifndef CRADLE_THINKNODE_IAM_H
#define CRADLE_THINKNODE_IAM_H

#include <cradle/service/core.h>
#include <cradle/thinknode/types.hpp>

namespace cradle {

// Query the contents of a context.
cppcoro::shared_task<thinknode_context_contents>
get_context_contents(
    service_core& service, thinknode_session session, string context_id);

} // namespace cradle

#endif
