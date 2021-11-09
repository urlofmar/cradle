#ifndef CRADLE_THINKNODE_ISS_H
#define CRADLE_THINKNODE_ISS_H

#include <cradle/service/core.h>
#include <cradle/thinknode/types.hpp>

namespace cradle {

struct http_connection_interface;

// Resolve an ISS object to an immutable ID.
cppcoro::shared_task<string>
resolve_iss_object_to_immutable(
    service_core& service,
    thinknode_session session,
    string context_id,
    string object_id,
    bool ignore_upgrades);

// Get the metadata for an ISS object.
cppcoro::shared_task<std::map<string, string>>
get_iss_object_metadata(
    service_core& service,
    thinknode_session session,
    string context_id,
    string object_id);

// Retrieve an immutable data object.
cppcoro::shared_task<dynamic>
retrieve_immutable(
    service_core& service,
    thinknode_session session,
    string context_id,
    string immutable_id);

// Retrieve an immutable object as a raw blob of data (in MessagePack format).
cppcoro::shared_task<blob>
retrieve_immutable_blob(
    service_core& service,
    thinknode_session session,
    string context_id,
    string immutable_id);

// Get the URL form of a schema.
string
get_url_type_string(
    thinknode_session const& session, thinknode_type_info const& schema);

// Parse a schema in URL form.
thinknode_type_info
parse_url_type_string(string const& url_type);

// Post an ISS object and return its ID.
cppcoro::shared_task<string>
post_iss_object(
    service_core& service,
    thinknode_session session,
    string context_id,
    thinknode_type_info schema,
    dynamic data);

// Post an ISS object and return its ID.
// This form takes the object already encoded in msgpack.
cppcoro::shared_task<string>
post_iss_object(
    service_core& service,
    thinknode_session session,
    string context_id,
    thinknode_type_info schema,
    blob msgpack_data);

// Shallowly copy an ISS object from one bucket to another.
cppcoro::task<nil_t>
shallowly_copy_iss_object(
    service_core& service,
    thinknode_session session,
    string source_bucket,
    string destination_context_id,
    string object_id);

} // namespace cradle

#endif
