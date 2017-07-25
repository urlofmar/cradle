#include <cradle/thinknode/iss.hpp>

#include <cradle/core/monitoring.hpp>
#include <cradle/io/http_requests.hpp>
#include <cradle/io/msgpack_io.hpp>

namespace cradle {

string
resolve_iss_object_to_immutable(
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& object_id)
{
    auto query =
        make_get_request(
            session.api_url + "/iss/" + object_id + "/immutable?context=" + context_id,
            {
                { "Authorization", "Bearer '" + session.access_token + "'" },
                { "Accept", "application/json" }
            });
    null_check_in check_in;
    null_progress_reporter reporter;
    auto response = connection.perform_request(check_in, reporter, query);
    return from_value<id_response>(parse_json_response(response)).id;
}

value
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
                { "Authorization", "Bearer '" + session.access_token + "'" },
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
     case api_type_info_tag::ARRAY_TYPE:
        return "array/" + get_url_type_string(as_array_type(schema).element_schema);
     case api_type_info_tag::BLOB_TYPE:
        return "blob";
     case api_type_info_tag::BOOLEAN_TYPE:
        return "boolean";
     case api_type_info_tag::DATETIME_TYPE:
        return "datetime";
     case api_type_info_tag::DYNAMIC_TYPE:
        return "dynamic";
     case api_type_info_tag::ENUM_TYPE:
      {
        std::stringstream ss;
        auto const& e = as_enum_type(schema);
        ss << "enum/" << e.values.size();
        for (auto const& v : e.values)
        {
            ss << "/" << v.first;
        }
        return ss.str();
      }
     case api_type_info_tag::FLOAT_TYPE:
        return "float";
     case api_type_info_tag::INTEGER_TYPE:
        return "integer";
     case api_type_info_tag::MAP_TYPE:
      {
        auto const& m = as_map_type(schema);
        return "map/" + get_url_type_string(m.key_schema) + "/" +
            get_url_type_string(m.value_schema);
      }
     case api_type_info_tag::NAMED_TYPE:
      {
        auto const& n = as_named_type(schema);
        return "named/" + n.account + "/" + n.app + "/" + n.name;
      }
     case api_type_info_tag::NIL_TYPE:
     default:
        return "nil";
     case api_type_info_tag::OPTIONAL_TYPE:
        return "optional/" + get_url_type_string(as_optional_type(schema));
     case api_type_info_tag::REFERENCE_TYPE:
        return "reference/" + get_url_type_string(as_reference_type(schema));
     case api_type_info_tag::STRING_TYPE:
        return "string";
     case api_type_info_tag::STRUCTURE_TYPE:
      {
        std::stringstream ss;
        auto const& s = as_structure_type(schema);
        ss << "structure/" << s.fields.size();
        for (auto const& f : s.fields)
        {
            ss << "/" << f.first << "/" << get_url_type_string(f.second.schema);
        }
        return ss.str();
      }
     case api_type_info_tag::UNION_TYPE:
      {
        std::stringstream ss;
        auto const& u = as_union_type(schema);
        ss << "union/" << u.members.size();
        for (auto const& m : u.members)
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
    value const& data)
{
    auto query =
        make_post_request(
            session.api_url + "/iss/" + get_url_type_string(schema) + "?context=" + context_id,
            value_to_msgpack_blob(data),
            {
                { "Authorization", "Bearer '" + session.access_token + "'" },
                { "Accept", "application/json" },
                { "Content-Type", "application/octet-stream" }
            });
    null_check_in check_in;
    null_progress_reporter reporter;
    auto response = connection.perform_request(check_in, reporter, query);
    return from_value<id_response>(parse_json_response(response)).id;
}

}
