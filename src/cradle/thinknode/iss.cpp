#include <cradle/thinknode/iss.h>

#include <boost/tokenizer.hpp>

#include <cradle/core/monitoring.h>
#include <cradle/encodings/msgpack.h>
#include <cradle/encodings/sha256_hash_id.h>
#include <cradle/io/http_requests.hpp>
#include <cradle/thinknode/calc.h>
#include <cradle/thinknode/utilities.h>
#include <cradle/utilities/logging.h>
#include <cradle/utilities/text.h>

namespace cradle {

namespace uncached {

cppcoro::task<string>
resolve_iss_object_to_immutable(
    service_core& service,
    thinknode_session session,
    string context_id,
    string object_id,
    bool ignore_upgrades)
{
    auto query = make_get_request(
        session.api_url + "/iss/" + object_id
            + "/immutable?context=" + context_id
            + "&ignore_upgrades=" + (ignore_upgrades ? "true" : "false"),
        {{"Authorization", "Bearer " + session.access_token},
         {"Accept", "application/json"}});

    auto response = co_await async_http_request(service, query);
    switch (response.status_code)
    {
        case 200: {
            co_return from_dynamic<id_response>(parse_json_response(response))
                .id;
        }
        case 202: {
            // The ISS object we're interested in is the result of a
            // calculation that hasn't finished yet, so wait for it to resolve
            // and try again.
            auto progression = long_poll_calculation_status(
                service,
                session,
                context_id,
                response.headers["Thinknode-Reference-Id"]);
            co_await for_async(std::move(progression), [](auto status) {});
            co_return co_await uncached::resolve_iss_object_to_immutable(
                service,
                std::move(session),
                std::move(context_id),
                std::move(object_id),
                ignore_upgrades);
        }
        case 204: {
            // The ISS object we're interested in is the result of a
            // calculation that failed.
            // (Maybe do something more informative here in the future.)
        }
        default: {
            CRADLE_THROW(
                bad_http_status_code()
                << attempted_http_request_info(redact_request(query))
                << http_response_info(response));
        }
    }
}

} // namespace uncached

cppcoro::shared_task<string>
resolve_iss_object_to_immutable(
    service_core& service,
    thinknode_session session,
    string context_id,
    string object_id,
    bool ignore_upgrades)
{
    auto cache_key = make_sha256_hashed_id(
        "resolve_iss_object_to_immutable",
        session.api_url,
        ignore_upgrades ? "n/a" : context_id,
        object_id);

    return fully_cached<string>(service, cache_key, [=, &service] {
        return uncached::resolve_iss_object_to_immutable(
            service, session, context_id, object_id, ignore_upgrades);
    });
}

namespace uncached {

cppcoro::task<std::map<string, string>>
get_iss_object_metadata(
    service_core& service,
    thinknode_session session,
    string context_id,
    string object_id)
{
    auto query = make_http_request(
        http_request_method::HEAD,
        session.api_url + "/iss/" + object_id + "?context=" + context_id,
        {{"Authorization", "Bearer " + session.access_token}},
        blob());

    auto response = co_await async_http_request(service, query);
    switch (response.status_code)
    {
        case 200: {
            co_return response.headers;
        }
        case 202: {
            // The ISS object we're interested in is the result of a
            // calculation that hasn't finished yet, so wait for it to resolve
            // and try again.
            auto progression = long_poll_calculation_status(
                service,
                session,
                context_id,
                response.headers["Thinknode-Reference-Id"]);
            co_await for_async(std::move(progression), [](auto status) {});
            co_return co_await uncached::get_iss_object_metadata(
                service,
                std::move(session),
                std::move(context_id),
                std::move(object_id));
        }
        case 204: {
            // The ISS object we're interested in is the result of a
            // calculation that failed.
        }
        default: {
            CRADLE_THROW(
                bad_http_status_code()
                << attempted_http_request_info(redact_request(query))
                << http_response_info(response));
        }
    }
}

} // namespace uncached

cppcoro::shared_task<std::map<string, string>>
get_iss_object_metadata(
    service_core& service,
    thinknode_session session,
    string context_id,
    string object_id)
{
    CRADLE_LOG_CALL(<< CRADLE_LOG_ARG(context_id) << CRADLE_LOG_ARG(object_id))

    auto cache_key = make_sha256_hashed_id(
        "get_iss_object_metadata", session.api_url, context_id, object_id);

    return fully_cached<std::map<string, string>>(
        service, cache_key, [=, &service] {
            return uncached::get_iss_object_metadata(
                service, session, context_id, object_id);
        });
}

namespace uncached {

cppcoro::task<dynamic>
retrieve_immutable(
    service_core& service,
    thinknode_session session,
    string context_id,
    string immutable_id)
{
    auto query = make_get_request(
        session.api_url + "/iss/immutable/" + immutable_id
            + "?context=" + context_id,
        {{"Authorization", "Bearer " + session.access_token},
         {"Accept", "application/octet-stream"}});
    auto response = co_await async_http_request(service, query);
    co_return parse_msgpack_response(response);
}

} // namespace uncached

cppcoro::shared_task<dynamic>
retrieve_immutable(
    service_core& service,
    thinknode_session session,
    string context_id,
    string immutable_id)
{
    // CRADLE_LOG_CALL(
    //     << CRADLE_LOG_ARG(context_id) << CRADLE_LOG_ARG(immutable_id));

    auto cache_key = make_sha256_hashed_id(
        "retrieve_immutable", session.api_url, immutable_id);

    return fully_cached<dynamic>(service, cache_key, [=, &service] {
        return uncached::retrieve_immutable(
            service, session, context_id, immutable_id);
    });
}

namespace uncached {

cppcoro::task<blob>
retrieve_immutable_blob(
    service_core& service,
    thinknode_session session,
    string context_id,
    string immutable_id)
{
    auto query = make_get_request(
        session.api_url + "/iss/immutable/" + immutable_id
            + "?context=" + context_id,
        {{"Authorization", "Bearer " + session.access_token},
         {"Accept", "application/octet-stream"}});
    auto response = co_await async_http_request(service, query);
    co_return response.body;
}

} // namespace uncached

cppcoro::shared_task<blob>
retrieve_immutable_blob(
    service_core& service,
    thinknode_session session,
    string context_id,
    string immutable_id)
{
    // CRADLE_LOG_CALL(
    //     << CRADLE_LOG_ARG(context_id) << CRADLE_LOG_ARG(immutable_id));

    auto cache_key = make_sha256_hashed_id(
        "retrieve_immutable_blob", session.api_url, immutable_id);

    return fully_cached<blob>(service, cache_key, [=, &service] {
        return uncached::retrieve_immutable_blob(
            service, session, context_id, immutable_id);
    });
}

string
get_url_type_string(
    thinknode_session const& session, thinknode_type_info const& schema)
{
    switch (get_tag(schema))
    {
        case thinknode_type_info_tag::ARRAY_TYPE:
            return "array/"
                   + get_url_type_string(
                       session, as_array_type(schema).element_schema);
        case thinknode_type_info_tag::BLOB_TYPE:
            return "blob";
        case thinknode_type_info_tag::BOOLEAN_TYPE:
            return "boolean";
        case thinknode_type_info_tag::DATETIME_TYPE:
            return "datetime";
        case thinknode_type_info_tag::DYNAMIC_TYPE:
            return "dynamic";
        case thinknode_type_info_tag::ENUM_TYPE: {
            std::stringstream ss;
            auto const& e = as_enum_type(schema);
            ss << "enum/" << e.values.size();
            for (auto const& v : e.values)
            {
                ss << "/" << v.first;
            }
            return ss.str();
        }
        case thinknode_type_info_tag::FLOAT_TYPE:
            return "float";
        case thinknode_type_info_tag::INTEGER_TYPE:
            return "integer";
        case thinknode_type_info_tag::MAP_TYPE: {
            auto const& m = as_map_type(schema);
            return "map/" + get_url_type_string(session, m.key_schema) + "/"
                   + get_url_type_string(session, m.value_schema);
        }
        case thinknode_type_info_tag::NAMED_TYPE: {
            auto const& n = as_named_type(schema);
            return "named/"
                   + (n.account ? *n.account : get_account_name(session)) + "/"
                   + n.app + "/" + n.name;
        }
        case thinknode_type_info_tag::NIL_TYPE:
        default:
            return "nil";
        case thinknode_type_info_tag::OPTIONAL_TYPE:
            return "optional/"
                   + get_url_type_string(session, as_optional_type(schema));
        case thinknode_type_info_tag::REFERENCE_TYPE:
            return "reference/"
                   + get_url_type_string(session, as_reference_type(schema));
        case thinknode_type_info_tag::STRING_TYPE:
            return "string";
        case thinknode_type_info_tag::STRUCTURE_TYPE: {
            std::stringstream ss;
            auto const& s = as_structure_type(schema);
            ss << "structure/" << s.fields.size();
            for (auto const& f : s.fields)
            {
                ss << "/" << f.first << "/"
                   << get_url_type_string(session, f.second.schema);
            }
            return ss.str();
        }
        case thinknode_type_info_tag::UNION_TYPE: {
            std::stringstream ss;
            auto const& u = as_union_type(schema);
            ss << "union/" << u.members.size();
            for (auto const& m : u.members)
            {
                ss << "/" << m.first << "/"
                   << get_url_type_string(session, m.second.schema);
            }
            return ss.str();
        }
    }
}

namespace {

typedef boost::tokenizer<boost::char_separator<char>> tokenizer_t;
typedef tokenizer_t::const_iterator token_iter_t;
typedef std::pair<token_iter_t, token_iter_t> token_range_t;

// Get (and consume) the next from the token range.
// If none is available, an throw an exception.
string
get_token(token_range_t& tokens)
{
    if (tokens.first == tokens.second)
        throw "missing type components";
    string token = *tokens.first;
    ++tokens.first;
    return token;
}

thinknode_type_info
parse_url_type(token_range_t& tokens)
{
    auto type_code = get_token(tokens);
    if (type_code == "array")
    {
        return make_thinknode_type_info_with_array_type(
            make_thinknode_array_info(parse_url_type(tokens), none));
    }
    if (type_code == "blob")
    {
        return make_thinknode_type_info_with_blob_type(thinknode_blob_type());
    }
    if (type_code == "boolean")
    {
        return make_thinknode_type_info_with_boolean_type(
            thinknode_boolean_type());
    }
    if (type_code == "datetime")
    {
        return make_thinknode_type_info_with_datetime_type(
            thinknode_datetime_type());
    }
    if (type_code == "dynamic")
    {
        return make_thinknode_type_info_with_dynamic_type(
            thinknode_dynamic_type());
    }
    if (type_code == "enum")
    {
        auto n_values = lexical_cast<size_t>(get_token(tokens));
        thinknode_enum_info enum_info;
        for (size_t i = 0; i != n_values; ++i)
        {
            enum_info.values[get_token(tokens)] = thinknode_enum_value_info();
        }
        return make_thinknode_type_info_with_enum_type(enum_info);
    }
    if (type_code == "float")
    {
        return make_thinknode_type_info_with_float_type(
            thinknode_float_type());
    }
    if (type_code == "integer")
    {
        return make_thinknode_type_info_with_integer_type(
            thinknode_integer_type());
    }
    if (type_code == "map")
    {
        auto key_schema = parse_url_type(tokens);
        auto value_schema = parse_url_type(tokens);
        return make_thinknode_type_info_with_map_type(
            make_thinknode_map_info(key_schema, value_schema));
    }
    if (type_code == "named")
    {
        auto account = get_token(tokens);
        auto app = get_token(tokens);
        auto name = get_token(tokens);
        return make_thinknode_type_info_with_named_type(
            make_thinknode_named_type_reference(account, app, name));
    }
    if (type_code == "nil")
    {
        return make_thinknode_type_info_with_nil_type(thinknode_nil_type());
    }
    if (type_code == "optional")
    {
        return make_thinknode_type_info_with_optional_type(
            parse_url_type(tokens));
    }
    if (type_code == "reference")
    {
        return make_thinknode_type_info_with_reference_type(
            parse_url_type(tokens));
    }
    if (type_code == "string")
    {
        return make_thinknode_type_info_with_string_type(
            thinknode_string_type());
    }
    if (type_code == "structure")
    {
        auto n_fields = lexical_cast<size_t>(get_token(tokens));
        thinknode_structure_info structure_info;
        for (size_t i = 0; i != n_fields; ++i)
        {
            auto name = get_token(tokens);
            auto schema = parse_url_type(tokens);
            structure_info.fields[name]
                = make_thinknode_structure_field_info("", none, schema);
        }
        return make_thinknode_type_info_with_structure_type(structure_info);
    }
    if (type_code == "union")
    {
        auto n_members = lexical_cast<size_t>(get_token(tokens));
        thinknode_union_info union_info;
        for (size_t i = 0; i != n_members; ++i)
        {
            auto name = get_token(tokens);
            auto schema = parse_url_type(tokens);
            union_info.members[name]
                = make_thinknode_union_member_info("", schema);
        }
        return make_thinknode_type_info_with_union_type(union_info);
    }
    throw "unrecognized type code: " + type_code;
}

} // namespace

thinknode_type_info
parse_url_type_string(string const& url_type)
{
    try
    {
        tokenizer_t tokenizer{url_type, boost::char_separator<char>{"/"}};
        auto tokens = std::make_pair(tokenizer.begin(), tokenizer.end());
        auto type = parse_url_type(tokens);
        if (tokens.first != tokens.second)
        {
            throw "extra type components";
        }
        return type;
    }
    catch (char const* message)
    {
        CRADLE_THROW(
            parsing_error()
            << expected_format_info("Thinknode-style URL type string")
            << parsing_error_info(message) << parsed_text_info(url_type));
    }
}

namespace uncached {

cppcoro::task<string>
post_iss_object(
    service_core& service,
    thinknode_session session,
    string context_id,
    thinknode_type_info schema,
    blob msgpack_data)
{
    auto query = make_http_request(
        http_request_method::POST,
        session.api_url + "/iss/" + get_url_type_string(session, schema)
            + "?context=" + context_id,
        {{"Authorization", "Bearer " + session.access_token},
         {"Accept", "application/json"},
         {"Content-Type", "application/octet-stream"}},
        msgpack_data);

    auto response = co_await async_http_request(service, query);

    co_return from_dynamic<id_response>(parse_json_response(response)).id;
}

} // namespace uncached

cppcoro::shared_task<string>
post_iss_object(
    service_core& service,
    thinknode_session session,
    string context_id,
    thinknode_type_info schema,
    blob msgpack_data)
{
    std::string data_hash;
    picosha2::hash256_hex_string(
        msgpack_data.data, msgpack_data.data + msgpack_data.size, data_hash);

    auto cache_key = make_sha256_hashed_id(
        "post_iss_object",
        session.api_url,
        context_id,
        get_url_type_string(session, schema),
        data_hash);

    return fully_cached<string>(service, cache_key, [=, &service] {
        return uncached::post_iss_object(
            service, session, context_id, schema, msgpack_data);
    });
}

cppcoro::shared_task<string>
post_iss_object(
    service_core& service,
    thinknode_session session,
    string context_id,
    thinknode_type_info schema,
    dynamic data)
{
    blob msgpack_data = value_to_msgpack_blob(data);
    return post_iss_object(
        service,
        std::move(session),
        std::move(context_id),
        std::move(schema),
        std::move(msgpack_data));
}

cppcoro::task<nil_t>
shallowly_copy_iss_object(
    service_core& service,
    thinknode_session session,
    string source_bucket,
    string destination_context_id,
    string object_id)
{
    auto query = make_http_request(
        http_request_method::POST,
        session.api_url + "/iss/" + object_id + "/buckets/" + source_bucket
            + "?context=" + destination_context_id,
        {{"Authorization", "Bearer " + session.access_token}},
        blob());

    co_await async_http_request(service, query);

    co_return nil;
}

} // namespace cradle
