#ifndef CRADLE_WEBSOCKET_LOCAL_CALCS_H
#define CRADLE_WEBSOCKET_LOCAL_CALCS_H

#include <cradle/service/core.h>
#include <cradle/thinknode/types.hpp>

namespace cradle {

cppcoro::task<dynamic>
perform_local_calc(
    service_core& service,
    thinknode_session const& session,
    string const& context_id,
    calculation_request request);

}

#endif
