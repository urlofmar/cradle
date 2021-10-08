#ifndef CRADLE_CONFIG_HPP
#define CRADLE_CONFIG_HPP

#include <cradle/service/types.hpp>

namespace cradle {

api(struct)
struct server_config : service_config
{
    // whether or not the server should be open to connections from other
    // machines (defaults to false)
    omissible<bool> open;
    // the WebSocket port on which the server will listen
    omissible<cradle::integer> port;
};

} // namespace cradle

#endif
