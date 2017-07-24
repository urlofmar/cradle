#ifndef CRADLE_THINKNODE_ISS_HPP
#define CRADLE_THINKNODE_ISS_HPP

#include <cradle/common.hpp>

namespace cradle {

struct http_connection_interface;

// Resolve an ISS object to an immutable ID.
string
resolve_iss_object_to_immutable(
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& object_id);

// Retrieve an immutable data object.
value
retrieve_immutable(
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& immutable_id);

}

#endif
