#include <cradle/thinknode/iss.hpp>

#include <cradle/core/monitoring.hpp>
#include <cradle/encodings/msgpack.hpp>
#include <cradle/io/http_requests.hpp>
#include <cradle/thinknode/calc.hpp>

namespace cradle {

string
resolve_iss_object_to_immutable(
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& object_id,
    bool ignore_upgrades)
{
    auto query =
        make_get_request(
            session.api_url + "/iss/" + object_id + "/immutable?context=" + context_id +
                "&ignore_upgrades=" + (ignore_upgrades ? "true" : "false"),
            {
                { "Authorization", "Bearer " + session.access_token },
                { "Accept", "application/json" }
            });
    null_check_in check_in;
    null_progress_reporter reporter;
    auto response = connection.perform_request(check_in, reporter, query);
    switch (response.status_code)
    {
     case 200:
      {
        return from_dynamic<id_response>(parse_json_response(response)).id;
      }
     case 202:
      {
        // The ISS object we're interested in is the result of a calculation
        // that hasn't finished yet, so wait for it to resolve and try again.
        long_poll_calculation_status(
            check_in,
            [ ](calculation_status const& status)
            {
            },
            connection,
            session,
            context_id,
            response.headers["Thinknode-Reference-Id"]);
        return
            resolve_iss_object_to_immutable(
                connection,
                session,
                context_id,
                object_id,
                ignore_upgrades);
      }
     case 204:
      {
        // The ISS object we're interested in is the result of a calculation
        // that failed.
      }
     default:
      {
        CRADLE_THROW(
            bad_http_status_code() <<
                attempted_http_request_info(query) <<
                http_response_info(response));
      }
    }
}

dynamic
retrieve_immutable(
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& immutable_id)
{
    auto query =
        make_get_request(
            session.api_url + "/iss/immutable/" + immutable_id + "?context=" + context_id,
            {
                { "Authorization", "Bearer " + session.access_token },
                { "Accept", "application/octet-stream" }
            });
    null_check_in check_in;
    null_progress_reporter reporter;
    auto response = connection.perform_request(check_in, reporter, query);
    return parse_msgpack_response(response);
}

string
get_url_type_string(api_type_info const& schema)
{
    switch (get_tag(schema))
    {
     case api_type_info_tag::ARRAY:
        return "array/" + get_url_type_string(as_array(schema).element_schema);
     case api_type_info_tag::BLOB:
        return "blob";
     case api_type_info_tag::BOOLEAN:
        return "boolean";
     case api_type_info_tag::DATETIME:
        return "datetime";
     case api_type_info_tag::DYNAMIC:
        return "dynamic";
     case api_type_info_tag::ENUM:
      {
        std::stringstream ss;
        auto const& e = as_enum(schema);
        ss << "enum/" << e.size();
        for (auto const& v : e)
        {
            ss << "/" << v.first;
        }
        return ss.str();
      }
     case api_type_info_tag::FLOAT:
        return "float";
     case api_type_info_tag::INTEGER:
        return "integer";
     case api_type_info_tag::MAP:
      {
        auto const& m = as_map(schema);
        return "map/" + get_url_type_string(m.key_schema) + "/" +
            get_url_type_string(m.value_schema);
      }
     case api_type_info_tag::NAMED:
      {
        auto const& n = as_named(schema);
        return "named/" + n.account + "/" + n.app + "/" + n.name;
      }
     case api_type_info_tag::NIL:
     default:
        return "nil";
     case api_type_info_tag::OPTIONAL:
        return "optional/" + get_url_type_string(as_optional(schema));
     case api_type_info_tag::REFERENCE:
        return "reference/" + get_url_type_string(as_reference(schema));
     case api_type_info_tag::STRING:
        return "string";
     case api_type_info_tag::STRUCTURE:
      {
        std::stringstream ss;
        auto const& s = as_structure(schema);
        ss << "structure/" << s.size();
        for (auto const& f : s)
        {
            ss << "/" << f.first << "/" << get_url_type_string(f.second.schema);
        }
        return ss.str();
      }
     case api_type_info_tag::UNION:
      {
        std::stringstream ss;
        auto const& u = as_union(schema);
        ss << "union/" << u.size();
        for (auto const& m : u)
        {
            ss << "/" << m.first << "/" << get_url_type_string(m.second.schema);
        }
        return ss.str();
      }
    }
}

string
post_iss_object(
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id,
    api_type_info const& schema,
    dynamic const& data)
{
    auto query =
        make_http_request(
            http_request_method::POST,
            session.api_url + "/iss/" + get_url_type_string(schema) + "?context=" + context_id,
            {
                { "Authorization", "Bearer " + session.access_token },
                { "Accept", "application/json" },
                { "Content-Type", "application/octet-stream" }
            },
            value_to_msgpack_blob(data));
    null_check_in check_in;
    null_progress_reporter reporter;
    auto response = connection.perform_request(check_in, reporter, query);
    return from_dynamic<id_response>(parse_json_response(response)).id;
}

}
