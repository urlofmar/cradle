#ifndef CRADLE_THINKNODE_ISS_HPP
#define CRADLE_THINKNODE_ISS_HPP

#include <cradle/thinknode/types.hpp>

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

// Get the URL form of a schema.
string
get_url_type_string(api_type_info const& schema);

// Post an ISS object and return its ID.
string
post_iss_object(
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id,
    api_type_info const& schema,
    value const& data);

}

#endif
