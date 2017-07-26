#ifndef CRADLE_WEBSOCKET_SERVER_HPP
#define CRADLE_WEBSOCKET_SERVER_HPP

#include <cradle/common.hpp>

namespace cradle {

CRADLE_DEFINE_EXCEPTION(websocket_server_error)
// This exception provides internal_error_message_info.

void
run_websocket_server(uint16_t port);

}

#endif
