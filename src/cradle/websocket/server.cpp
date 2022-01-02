#include <cradle/io/asio.h>

#include <cradle/websocket/server.h>

#include <set>
#include <thread>

// Boost.Crc triggers some warnings on MSVC.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4245)
#pragma warning(disable : 4701)
#include <boost/crc.hpp>
#pragma warning(pop)
#else
#include <boost/crc.hpp>
#endif

#include <picosha2.h>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>
#ifdef _WIN32
#include <spdlog/sinks/wincolor_sink.h>
#else
#include <spdlog/sinks/ansicolor_sink.h>
#endif

#include <cppcoro/async_scope.hpp>
#include <cppcoro/schedule_on.hpp>
#include <cppcoro/static_thread_pool.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all.hpp>

#include <cradle/caching/disk_cache.hpp>
#include <cradle/encodings/base64.h>
#include <cradle/encodings/json.h>
#include <cradle/encodings/msgpack.h>
#include <cradle/encodings/sha256_hash_id.h>
#include <cradle/encodings/yaml.h>
#include <cradle/fs/app_dirs.h>
#include <cradle/fs/file_io.h>
#include <cradle/io/http_requests.hpp>
#include <cradle/thinknode/apm.h>
#include <cradle/thinknode/calc.h>
#include <cradle/thinknode/iam.h>
#include <cradle/thinknode/iss.h>
#include <cradle/thinknode/utilities.h>
#include <cradle/utilities/diff.hpp>
#include <cradle/utilities/errors.h>
#include <cradle/utilities/functional.h>
#include <cradle/utilities/logging.h>
#include <cradle/utilities/text.h>
#include <cradle/websocket/local_calcs.h>
#include <cradle/websocket/messages.hpp>

// Include this again because some #defines snuck in to overwrite some of our
// enum constants.
#include <cradle/core/api_types.hpp>

struct ws_config : public websocketpp::config::asio
{
    typedef ws_config type;
    typedef websocketpp::config::asio base;

    typedef base::concurrency_type concurrency_type;

    typedef base::request_type request_type;
    typedef base::response_type response_type;

    typedef base::message_type message_type;
    typedef base::con_msg_manager_type con_msg_manager_type;
    typedef base::endpoint_msg_manager_type endpoint_msg_manager_type;

    typedef base::alog_type alog_type;
    typedef base::elog_type elog_type;

    typedef base::rng_type rng_type;

    struct transport_config : public base::transport_config
    {
        typedef type::concurrency_type concurrency_type;
        typedef type::alog_type alog_type;
        typedef type::elog_type elog_type;
        typedef type::request_type request_type;
        typedef type::response_type response_type;
        typedef websocketpp::transport::asio::basic_socket::endpoint
            socket_type;
    };

    typedef websocketpp::transport::asio::endpoint<transport_config>
        transport_type;

    static const size_t max_message_size = 1000000000;
};

typedef websocketpp::server<ws_config> ws_server_type;

using websocketpp::connection_hdl;
using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

namespace cradle {
struct client_connection
{
    int id;
    string name;
    thinknode_session session;
};

struct client_connection_list
{
    int next_id = 1;
    std::
        map<connection_hdl, client_connection, std::owner_less<connection_hdl>>
            connections;
    std::mutex mutex;
};

static void
add_client(
    client_connection_list& list,
    connection_hdl hdl,
    client_connection client = client_connection())
{
    std::scoped_lock<std::mutex> lock(list.mutex);
    client.id = list.next_id;
    ++list.next_id;
    list.connections[hdl] = std::move(client);
}

static void
remove_client(client_connection_list& list, connection_hdl hdl)
{
    std::scoped_lock<std::mutex> lock(list.mutex);
    list.connections.erase(hdl);
}

static client_connection
get_client(client_connection_list& list, connection_hdl hdl)
{
    std::scoped_lock<std::mutex> lock(list.mutex);
    return list.connections.at(hdl);
}

template<class Fn>
void
update_client(client_connection_list& list, connection_hdl hdl, Fn const& fn)
{
    std::scoped_lock<std::mutex> lock(list.mutex);
    fn(list.connections.at(hdl));
}

template<class Fn>
void
for_each_client(client_connection_list& list, Fn&& fn)
{
    std::scoped_lock<std::mutex> lock(list.mutex);
    for (auto& client : list.connections)
    {
        std::forward<Fn>(fn)(client.first, client.second);
    }
}

struct client_request
{
    connection_hdl client;
    websocket_client_message message;
};

struct websocket_server_impl
{
    server_config config;
    http_request_system http_system;
    ws_server_type ws;
    client_connection_list clients;
    service_core core;
    cppcoro::async_scope async_scope;
    cppcoro::static_thread_pool pool;
};

static void
send(
    websocket_server_impl& server,
    connection_hdl hdl,
    websocket_server_message const& message)
{
    auto dynamic = to_dynamic(message);
    auto msgpack = value_to_msgpack_string(dynamic);
    websocketpp::lib::error_code ec;
    server.ws.send(hdl, msgpack, websocketpp::frame::opcode::binary, ec);
    if (ec)
    {
        CRADLE_THROW(
            websocket_server_error()
            << internal_error_message_info(ec.message()));
    }
}

static blob
encode_object(output_data_encoding encoding, blob const& msgpack_data)
{
    if (encoding == output_data_encoding::MSGPACK)
        return msgpack_data;

    auto object = parse_msgpack_value(
        reinterpret_cast<uint8_t const*>(msgpack_data.data),
        msgpack_data.size);
    switch (encoding)
    {
        case output_data_encoding::JSON:
            return value_to_json_blob(object);
        case output_data_encoding::YAML:
            return value_to_yaml_blob(object);
        case output_data_encoding::DIAGNOSTIC_YAML:
            return value_to_diagnostic_yaml_blob(object);
        default:
            CRADLE_THROW(
                invalid_enum_value()
                << enum_id_info("output_data_encoding")
                << enum_value_info(static_cast<int>(encoding)));
    }
}

cppcoro::shared_task<dynamic>
get_iss_object(
    service_core& service,
    thinknode_session session,
    string context_id,
    string object_id,
    bool ignore_upgrades = false)
{
    CRADLE_LOG_CALL(
        << CRADLE_LOG_ARG(context_id) << CRADLE_LOG_ARG(object_id)
        << CRADLE_LOG_ARG(ignore_upgrades));

    auto immutable_id = co_await resolve_iss_object_to_immutable(
        service, session, context_id, object_id, ignore_upgrades);

    co_return co_await retrieve_immutable(
        service, session, context_id, immutable_id);
}

cppcoro::shared_task<blob>
get_iss_blob(
    service_core& service,
    thinknode_session session,
    string context_id,
    string object_id,
    bool ignore_upgrades = false)
{
    CRADLE_LOG_CALL(
        << CRADLE_LOG_ARG(context_id) << CRADLE_LOG_ARG(object_id)
        << CRADLE_LOG_ARG(ignore_upgrades));

    auto immutable_id = co_await resolve_iss_object_to_immutable(
        service, session, context_id, object_id, ignore_upgrades);

    co_return co_await retrieve_immutable_blob(
        service, session, context_id, immutable_id);
}

cppcoro::task<thinknode_app_version_info>
resolve_context_app(
    service_core& service,
    thinknode_session session,
    string context_id,
    string account,
    string app)
{
    auto context = co_await get_context_contents(service, session, context_id);
    for (auto const& app_info : context.contents)
    {
        if (app_info.account == account && app_info.app == app)
        {
            if (!is_version(app_info.source))
            {
                CRADLE_THROW(
                    websocket_server_error() << internal_error_message_info(
                        "apps must be installed as versions"));
            }
            co_return co_await get_app_version_info(
                service, session, account, app, as_version(app_info.source));
        }
    }
    CRADLE_THROW(
        websocket_server_error()
        << internal_error_message_info("app not found in context"));
}

namespace uncached {

cppcoro::task<api_type_info>
resolve_named_type_reference(
    service_core& service,
    thinknode_session session,
    string context_id,
    api_named_type_reference ref)
{
    auto version_info = co_await resolve_context_app(
        service, session, context_id, get_account_name(session), ref.app);
    for (auto const& type : version_info.manifest->types)
    {
        if (type.name == ref.name)
        {
            co_return as_api_type(type.schema);
        }
    }
    CRADLE_THROW(
        websocket_server_error()
        << internal_error_message_info("type not found in app"));
}

} // namespace uncached

cppcoro::shared_task<api_type_info>
resolve_named_type_reference(
    service_core& service,
    thinknode_session session,
    string context_id,
    api_named_type_reference ref)
{
    auto cache_key = make_sha256_hashed_id(
        "resolve_named_type_reference", session.api_url, context_id, ref);

    return fully_cached<api_type_info>(service, cache_key, [=, &service] {
        return uncached::resolve_named_type_reference(
            service, session, context_id, ref);
    });
}

namespace uncached {

// Decode an object from its encoded form, coerce it to the specified schema,
// and encode it again as msgpack.
static cppcoro::task<blob>
coerce_encoded_object(
    service_core& core,
    thinknode_session session,
    string context_id,
    thinknode_type_info schema,
    input_data_encoding encoding,
    blob encoded_object)
{
    // Decode the object.
    spdlog::get("cradle")->info("coerce_encoded_object: decoding");
    dynamic decoded_object;
    switch (encoding)
    {
        case input_data_encoding::JSON:
            decoded_object = parse_json_value(
                reinterpret_cast<char const*>(encoded_object.data),
                encoded_object.size);
            break;
        case input_data_encoding::YAML:
            decoded_object = parse_yaml_value(
                reinterpret_cast<char const*>(encoded_object.data),
                encoded_object.size);
            break;
        case input_data_encoding::MSGPACK:
            decoded_object = parse_msgpack_value(
                reinterpret_cast<uint8_t const*>(encoded_object.data),
                encoded_object.size);
            break;
    }

    // Apply type coercion.
    spdlog::get("cradle")->info("coerce_encoded_object: coercing");
    auto coerced_object = co_await coerce_value(
        [&](api_named_type_reference const& ref)
            -> cppcoro::task<api_type_info> {
            co_return co_await cradle::resolve_named_type_reference(
                core, session, context_id, ref);
        },
        as_api_type(schema),
        std::move(decoded_object));

    // Encode it again as MessagePack.
    spdlog::get("cradle")->info("coerce_encoded_object: encoding");
    co_return value_to_msgpack_blob(coerced_object);
}

} // namespace uncached

static cppcoro::shared_task<blob>
coerce_encoded_object(
    service_core& core,
    thinknode_session session,
    string context_id,
    thinknode_type_info schema,
    input_data_encoding encoding,
    blob encoded_object)
{
    std::string data_hash;
    picosha2::hash256_hex_string(
        encoded_object.data,
        encoded_object.data + encoded_object.size,
        data_hash);

    auto cache_key = make_sha256_hashed_id(
        "coerce_encoded_object",
        session.api_url,
        context_id,
        schema,
        encoding,
        data_hash);

    return fully_cached<blob>(core, cache_key, [=, &core] {
        return uncached::coerce_encoded_object(
            core, session, context_id, schema, encoding, encoded_object);
    });
}

static cppcoro::task<string>
post_iss_object(
    service_core& core,
    thinknode_session session,
    string context_id,
    thinknode_type_info schema,
    input_data_encoding encoding,
    blob encoded_object)
{
    auto coerced_object = co_await coerce_encoded_object(
        core,
        session,
        context_id,
        schema,
        encoding,
        std::move(encoded_object));

    spdlog::get("cradle")->info("post_iss_object: posting");
    co_return co_await post_iss_object(
        core,
        std::move(session),
        std::move(context_id),
        std::move(schema),
        std::move(coerced_object));
}

cppcoro::task<bool>
type_contains_references(
    service_core& service,
    std::set<api_type_info>& already_visited,
    thinknode_session const& session,
    string const& context_id,
    api_type_info const& type);

namespace uncached {

cppcoro::task<bool>
type_contains_references(
    service_core& service,
    std::set<api_type_info>& already_visited,
    thinknode_session const& session,
    string const& context_id,
    api_type_info const& type)
{
    auto recurse = [&](api_type_info const& type) -> cppcoro::task<bool> {
        return cradle::type_contains_references(
            service, already_visited, session, context_id, type);
    };

    switch (get_tag(type))
    {
        case api_type_info_tag::ARRAY_TYPE:
            co_return co_await recurse(as_array_type(type).element_schema);
        case api_type_info_tag::BLOB_TYPE:
            co_return false;
        case api_type_info_tag::BOOLEAN_TYPE:
            co_return false;
        case api_type_info_tag::DATETIME_TYPE:
            co_return false;
        case api_type_info_tag::DYNAMIC_TYPE:
            // Technically, there could be a reference stored within a
            // dynamic (or in blobs, strings, etc.), but we're only looking
            // for explicitly typed references.
            co_return false;
        case api_type_info_tag::ENUM_TYPE:
            co_return false;
        case api_type_info_tag::FLOAT_TYPE:
            co_return false;
        case api_type_info_tag::INTEGER_TYPE:
            co_return false;
        case api_type_info_tag::MAP_TYPE: {
            auto [in_key_type, in_value_type] = co_await cppcoro::when_all(
                recurse(as_map_type(type).key_schema),
                recurse(as_map_type(type).value_schema));
            co_return in_key_type || in_value_type;
        }
        case api_type_info_tag::NAMED_TYPE:
            co_return co_await recurse(
                co_await cradle::resolve_named_type_reference(
                    service, session, context_id, as_named_type(type)));
        case api_type_info_tag::NIL_TYPE:
        default:
            co_return false;
        case api_type_info_tag::OPTIONAL_TYPE:
            co_return co_await recurse(as_optional_type(type));
        case api_type_info_tag::REFERENCE_TYPE:
            co_return true;
        case api_type_info_tag::STRING_TYPE:
            co_return false;
        case api_type_info_tag::STRUCTURE_TYPE: {
            auto in_fields = co_await cppcoro::when_all(map_to_vector(
                [&](auto const& pair) -> cppcoro::task<bool> {
                    return recurse(pair.second.schema);
                },
                as_structure_type(type).fields));
            co_return std::any_of(
                in_fields.begin(), in_fields.end(), std::identity());
        }
        case api_type_info_tag::UNION_TYPE: {
            auto in_members = co_await cppcoro::when_all(map_to_vector(
                [&](auto const& pair) -> cppcoro::task<bool> {
                    return recurse(pair.second.schema);
                },
                as_union_type(type).members));
            co_return std::any_of(
                in_members.begin(), in_members.end(), std::identity());
        }
    }
}

} // namespace uncached

cppcoro::task<bool>
type_contains_references(
    service_core& service,
    std::set<api_type_info>& already_visited,
    thinknode_session const& session,
    string const& context_id,
    api_type_info const& type)
{
    // CRADLE_LOG_CALL(<< CRADLE_LOG_ARG(context_id) << CRADLE_LOG_ARG(type))

    // Check to see if we have already visited this type within this
    // evaluation. (This happens when we are processing a recursive union type,
    // and if we didn't check for it here, we would end up in an infinite
    // recursion.) We can safely return `false` here because the entire logic
    // of this function is `or`-driven (i.e., if any part of the type contains
    // a reference, the whole thing does), and the original evaluation of this
    // type will return the correct result.
    if (already_visited.contains(type))
        co_return false;
    already_visited.insert(type);

    auto cache_key = make_sha256_hashed_id(
        "type_contains_references", session.api_url, context_id, type);

    co_return co_await fully_cached<bool>(service, cache_key, [&] {
        return uncached::type_contains_references(
            service, already_visited, session, context_id, type);
    });
}

cppcoro::task<nil_t>
visit_object_references(
    service_core& service,
    thinknode_session const& session,
    string const& context_id,
    api_type_info const& type,
    dynamic const& value,
    function_view<cppcoro::task<nil_t>(string const& ref)> const& visitor)
{
    // CRADLE_LOG_CALL(
    //     << CRADLE_LOG_ARG(context_id) << CRADLE_LOG_ARG(type)
    //     << CRADLE_LOG_ARG(value))

    auto recurse = [&](api_type_info const& type,
                       dynamic const& value) -> cppcoro::task<nil_t> {
        return visit_object_references(
            service, session, context_id, type, value, visitor);
    };

    switch (get_tag(type))
    {
        case api_type_info_tag::ARRAY_TYPE:
            co_await cppcoro::when_all(map(
                [&](auto const& item) {
                    return recurse(as_array_type(type).element_schema, item);
                },
                cast<dynamic_array>(value)));
            break;
        case api_type_info_tag::BLOB_TYPE:
            break;
        case api_type_info_tag::BOOLEAN_TYPE:
            break;
        case api_type_info_tag::DATETIME_TYPE:
            break;
        case api_type_info_tag::DYNAMIC_TYPE:
            break;
        case api_type_info_tag::ENUM_TYPE:
            break;
        case api_type_info_tag::FLOAT_TYPE:
            break;
        case api_type_info_tag::INTEGER_TYPE:
            break;
        case api_type_info_tag::MAP_TYPE: {
            auto const& map_type = as_map_type(type);
            std::vector<cppcoro::task<nil_t>> subtasks;
            for (auto const& pair : cast<dynamic_map>(value))
            {
                subtasks.push_back(recurse(map_type.key_schema, pair.first));
                subtasks.push_back(
                    recurse(map_type.value_schema, pair.second));
            }
            co_await cppcoro::when_all(std::move(subtasks));
            break;
        }
        case api_type_info_tag::NAMED_TYPE: {
            auto type_info = co_await resolve_named_type_reference(
                service, session, context_id, as_named_type(type));
            co_await recurse(type_info, value);
            break;
        }
        case api_type_info_tag::NIL_TYPE:
        default:
            break;
        case api_type_info_tag::OPTIONAL_TYPE: {
            auto const& map = cast<dynamic_map>(value);
            string tag;
            from_dynamic(&tag, cradle::get_union_tag(map));
            if (tag == "some")
            {
                co_await recurse(
                    as_optional_type(type), get_field(map, "some"));
            }
            break;
        }
        case api_type_info_tag::REFERENCE_TYPE:
            co_await visitor(cast<string>(value));
        case api_type_info_tag::STRING_TYPE:
            break;
        case api_type_info_tag::STRUCTURE_TYPE: {
            auto const& fields = cast<dynamic_map>(value);
            std::vector<cppcoro::task<nil_t>> in_fields;
            for (auto const& pair : as_structure_type(type).fields)
            {
                auto const& field_info = pair.second;
                dynamic const* field_value;
                bool field_present
                    = get_field(&field_value, fields, pair.first);
                if (field_present)
                {
                    in_fields.push_back(
                        recurse(field_info.schema, *field_value));
                }
            }
            co_await cppcoro::when_all(std::move(in_fields));
            break;
        }
        case api_type_info_tag::UNION_TYPE: {
            auto const& member_map = cast<dynamic_map>(value);
            string tag;
            from_dynamic(&tag, cradle::get_union_tag(member_map));
            co_await [&]() -> cppcoro::task<nil_t> {
                return recurse(
                    as_union_type(type).members.at(tag).schema,
                    get_field(member_map, tag));
            }();
            break;
        }
    }

    co_return nil;
}

cppcoro::task<nil_t>
deeply_copy_iss_object(
    service_core& service,
    thinknode_session session,
    string source_bucket,
    string source_context_id,
    string destination_context_id,
    string object_id);

namespace uncached {

cppcoro::task<nil_t>
deeply_copy_iss_object(
    service_core& service,
    thinknode_session session,
    string source_bucket,
    string source_context_id,
    string destination_context_id,
    string object_id)
{
    CRADLE_LOG_CALL(
        << CRADLE_LOG_ARG(source_context_id)
        << CRADLE_LOG_ARG(destination_context_id) << CRADLE_LOG_ARG(object_id))

    // Copying an object requires not just copying the object itself but
    // also any objects that it references. The brute force approach is to
    // download the copied object and scan it for references, recursively
    // copying the referenced objects. We use a slightly less brute force
    // method here by first checking the type of the object to see if it
    // contains any reference types. (If not, we skip the whole
    // download/scan/recurse step.)

    // Copy the object itself.
    auto [_, metadata] = co_await cppcoro::when_all(
        shallowly_copy_iss_object(
            service,
            session,
            source_bucket,
            destination_context_id,
            object_id),
        get_iss_object_metadata(
            service, session, source_context_id, object_id));

    auto object_type
        = as_api_type(parse_url_type_string(metadata["Thinknode-Type"]));

    std::set<api_type_info> already_visited;
    if (co_await cradle::type_contains_references(
            service, already_visited, session, source_context_id, object_type))
    {
        auto object = co_await get_iss_object(
            service, session, source_context_id, object_id);
        co_await visit_object_references(
            service,
            session,
            source_context_id,
            object_type,
            object,
            [&](string const& ref) -> cppcoro::task<nil_t> {
                co_return co_await cradle::deeply_copy_iss_object(
                    service,
                    session,
                    source_bucket,
                    source_context_id,
                    destination_context_id,
                    ref);
            });
    }

    co_return nil;
}

} // namespace uncached

cppcoro::task<nil_t>
deeply_copy_iss_object(
    service_core& service,
    thinknode_session session,
    string source_bucket,
    string source_context_id,
    string destination_context_id,
    string object_id)
{
    CRADLE_LOG_CALL(
        << CRADLE_LOG_ARG(source_context_id)
        << CRADLE_LOG_ARG(destination_context_id) << CRADLE_LOG_ARG(object_id))

    auto cache_key = make_sha256_hashed_id(
        "deeply_copy_iss_object",
        session.api_url,
        source_context_id,
        destination_context_id,
        object_id);

    co_return co_await fully_cached<nil_t>(service, cache_key, [&] {
        return uncached::deeply_copy_iss_object(
            service,
            session,
            source_bucket,
            source_context_id,
            destination_context_id,
            object_id);
    });
}

cppcoro::task<nil_t>
visit_calc_references(
    service_core& service,
    thinknode_session const& session,
    string const& context_id,
    calculation_request const& request,
    function_view<cppcoro::task<nil_t>(string const& ref)> const& visitor)
{
    auto recurse
        = [&](calculation_request const& request) -> cppcoro::task<nil_t> {
        return visit_calc_references(
            service, session, context_id, request, visitor);
    };

    switch (get_tag(request))
    {
        case calculation_request_tag::REFERENCE:
            co_await visitor(as_reference(request));
            break;
        case calculation_request_tag::VALUE:
            break;
        case calculation_request_tag::FUNCTION: {
            std::vector<cppcoro::task<nil_t>> subtasks;
            for (auto const& arg : as_function(request).args)
                subtasks.push_back(recurse(arg));
            co_await cppcoro::when_all_ready(std::move(subtasks));
            break;
        }
        case calculation_request_tag::ARRAY: {
            std::vector<cppcoro::task<nil_t>> subtasks;
            for (auto const& item : as_array(request).items)
                subtasks.push_back(recurse(item));
            co_await cppcoro::when_all_ready(std::move(subtasks));
            break;
        }
        case calculation_request_tag::ITEM:
            co_await cppcoro::when_all_ready(
                recurse(as_item(request).array),
                recurse(as_item(request).index));
            break;
        case calculation_request_tag::OBJECT: {
            std::vector<cppcoro::task<nil_t>> subtasks;
            for (auto const& item : as_object(request).properties)
                subtasks.push_back(recurse(item.second));
            co_await cppcoro::when_all_ready(std::move(subtasks));
            break;
        }
        case calculation_request_tag::PROPERTY: {
            co_await cppcoro::when_all_ready(
                recurse(as_property(request).object),
                recurse(as_property(request).field));
            break;
        }
        case calculation_request_tag::CAST:
            co_await recurse(as_cast(request).object);
            break;
        default:
            CRADLE_THROW(
                invalid_enum_value()
                << enum_id_info("calculation_request_tag")
                << enum_value_info(static_cast<int>(get_tag(request))));
    }

    co_return nil;
}

cppcoro::task<nil_t>
deeply_copy_calculation(
    service_core& service,
    thinknode_session session,
    string source_bucket,
    string source_context_id,
    string destination_context_id,
    string object_id);

namespace uncached {

cppcoro::task<nil_t>
deeply_copy_calculation(
    service_core& service,
    thinknode_session session,
    string source_bucket,
    string source_context_id,
    string destination_context_id,
    string object_id)
{
    CRADLE_LOG_CALL(
        << CRADLE_LOG_ARG(source_context_id)
        << CRADLE_LOG_ARG(destination_context_id) << CRADLE_LOG_ARG(object_id))

    bool copy_needed = true;

    if (get_thinknode_service_id(object_id) == thinknode_service_id::CALC)
    {
        calculation_request calculation;
        bool found_at_destination = false;
        // We need to get the calculation request anyway (to recursively
        // process its references), so try retrieving it via the destination
        // context to see if it already exists there.
        try
        {
            calculation = co_await retrieve_calculation_request(
                service, session, destination_context_id, object_id);
            found_at_destination = true;
            copy_needed = false;
        }
        catch (bad_http_status_code& e)
        {
            // If we get a 404, it just means that the calculation actually
            // needs to be copied, so we just retrieve it via the source
            // context instead.
            if (get_required_error_info<http_response_info>(e).status_code
                != 404)
            {
                throw;
            }
        }
        if (!found_at_destination)
        {
            calculation = co_await retrieve_calculation_request(
                service, session, source_context_id, object_id);
        }

        // Even if the calculation already exists at the destination, it's
        // possible that the arguments aren't entirely there, so we still need
        // to proceed recrusively.
        co_await visit_calc_references(
            service,
            session,
            destination_context_id,
            calculation,
            [&](string const& ref) -> cppcoro::task<nil_t> {
                return cradle::deeply_copy_calculation(
                    service,
                    session,
                    source_bucket,
                    source_context_id,
                    destination_context_id,
                    ref);
            });
    }

    if (copy_needed)
    {
        co_await cradle::deeply_copy_iss_object(
            service,
            session,
            source_bucket,
            source_context_id,
            destination_context_id,
            object_id);
    }

    co_return nil;
}

} // namespace uncached

cppcoro::task<nil_t>
deeply_copy_calculation(
    service_core& service,
    thinknode_session session,
    string source_bucket,
    string source_context_id,
    string destination_context_id,
    string calculation_id)
{
    CRADLE_LOG_CALL(
        << CRADLE_LOG_ARG(source_context_id)
        << CRADLE_LOG_ARG(destination_context_id)
        << CRADLE_LOG_ARG(calculation_id))

    auto cache_key = make_sha256_hashed_id(
        "deeply_copy_calculation",
        session.api_url,
        source_context_id,
        destination_context_id,
        calculation_id);

    co_return co_await fully_cached<nil_t>(service, cache_key, [&] {
        return uncached::deeply_copy_calculation(
            service,
            session,
            source_bucket,
            source_context_id,
            destination_context_id,
            calculation_id);
    });
}

namespace detail {

cppcoro::task<calculation_request>
post_calculation_piecewise(
    service_core& core,
    thinknode_session session,
    string context_id,
    calculation_request request)
{
    auto recurse
        = [&](calculation_request calc) -> cppcoro::task<calculation_request> {
        return post_calculation_piecewise(
            core, session, context_id, std::move(calc));
    };

    calculation_request shallow_calc;
    switch (get_tag(request))
    {
        case calculation_request_tag::REFERENCE:
        case calculation_request_tag::VALUE:
            co_return std::move(request);
        case calculation_request_tag::FUNCTION: {
            auto subtasks = map(recurse, std::move(as_function(request).args));
            shallow_calc = make_calculation_request_with_function(
                make_function_application(
                    as_function(request).account,
                    as_function(request).app,
                    as_function(request).name,
                    as_function(request).level,
                    co_await cppcoro::when_all(std::move(subtasks))));
            break;
        }
        case calculation_request_tag::ARRAY: {
            auto subtasks = map(recurse, std::move(as_array(request).items));
            shallow_calc = make_calculation_request_with_array(
                make_calculation_array_request(
                    co_await cppcoro::when_all(std::move(subtasks)),
                    as_array(request).item_schema));
            break;
        }
        case calculation_request_tag::ITEM:
            shallow_calc = make_calculation_request_with_item(
                make_calculation_item_request(
                    co_await recurse(as_item(request).array),
                    as_item(request).index,
                    as_item(request).schema));
            break;
        case calculation_request_tag::OBJECT: {
            std::map<string, calculation_request> properties;
            for (auto& property : as_object(request).properties)
            {
                properties[property.first]
                    = co_await recurse(std::move(property.second));
            }
            shallow_calc = make_calculation_request_with_object(
                make_calculation_object_request(
                    properties, as_object(request).schema));
            break;
        }
        case calculation_request_tag::PROPERTY:
            shallow_calc = make_calculation_request_with_property(
                make_calculation_property_request(
                    co_await recurse(as_property(request).object),
                    as_property(request).field,
                    as_property(request).schema));
            break;
        case calculation_request_tag::LET: {
            std::map<string, calculation_request> variables;
            for (auto& property : as_let(request).variables)
            {
                variables[property.first]
                    = co_await recurse(std::move(property.second));
            }
            shallow_calc = make_calculation_request_with_let(
                make_let_calculation_request(variables, as_let(request).in));
            break;
        }
        case calculation_request_tag::VARIABLE:
            co_return request;
        case calculation_request_tag::META:
            shallow_calc = make_calculation_request_with_meta(
                make_meta_calculation_request(
                    co_await recurse(as_meta(request).generator),
                    as_meta(request).schema));
            break;
        case calculation_request_tag::CAST:
            shallow_calc = make_calculation_request_with_cast(
                make_calculation_cast_request(
                    as_cast(request).schema,
                    co_await recurse(as_cast(request).object)));
            break;
        default:
            CRADLE_THROW(
                invalid_enum_value()
                << enum_id_info("calculation_request_tag")
                << enum_value_info(static_cast<int>(get_tag(request))));
    }

    co_return make_calculation_request_with_reference(
        co_await post_calculation(
            core, session, context_id, std::move(shallow_calc)));
}

} // namespace detail

cppcoro::task<std::string>
post_calculation_piecewise(
    service_core& core,
    thinknode_session session,
    string context_id,
    calculation_request request)
{
    co_return as_reference(co_await detail::post_calculation_piecewise(
        core, std::move(session), std::move(context_id), std::move(request)));
}

static bool
is_iss_id(dynamic const& value)
{
    if (value.type() == value_type::STRING)
    {
        auto const& id = cast<string>(value);
        if (id.length() == 32)
        {
            try
            {
                return get_thinknode_service_id(id)
                       == thinknode_service_id::ISS;
            }
            catch (...)
            {
            }
        }
    }
    return false;
}

cppcoro::task<object_tree_diff>
compute_iss_tree_diff(
    service_core& service,
    thinknode_session session,
    string context_id_a,
    string object_id_a,
    string context_id_b,
    string object_id_b);

cppcoro::task<std::pair<std::optional<value_diff_item>, object_tree_diff>>
process_iss_tree_subdiff(
    service_core& service,
    thinknode_session session,
    string context_id_a,
    string context_id_b,
    value_diff_item item)
{
    if (item.a && is_iss_id(*item.a) && item.b && is_iss_id(*item.b))
    {
        auto subtree_diff = co_await compute_iss_tree_diff(
            service,
            session,
            context_id_a,
            cast<string>(*item.a),
            context_id_b,
            cast<string>(*item.b));
        for (auto& node : subtree_diff)
        {
            node.path_from_root.insert(
                node.path_from_root.begin(),
                item.path.begin(),
                item.path.end() - 1);
        }
        co_return std::make_pair(none, subtree_diff);
    }
    else
    {
        co_return std::make_pair(some(std::move(item)), object_tree_diff());
    }
}

cppcoro::task<object_tree_diff>
compute_iss_tree_diff(
    service_core& service,
    thinknode_session session,
    string context_id_a,
    string object_id_a,
    string context_id_b,
    string object_id_b)
{
    CRADLE_LOG_CALL(
        << CRADLE_LOG_ARG(session) << CRADLE_LOG_ARG(context_id_a)
        << CRADLE_LOG_ARG(object_id_a) << CRADLE_LOG_ARG(context_id_b)
        << CRADLE_LOG_ARG(object_id_b))

    auto [object_a, object_b] = co_await cppcoro::when_all(
        get_iss_object(service, session, context_id_a, object_id_a),
        get_iss_object(service, session, context_id_b, object_id_b));
    auto diff = compute_value_diff(object_a, object_b);

    auto subtasks = map(
        [=, &service](auto item)
            -> cppcoro::task<
                std::pair<std::optional<value_diff_item>, object_tree_diff>> {
            return process_iss_tree_subdiff(
                service, session, context_id_a, context_id_b, std::move(item));
        },
        std::move(diff));

    auto subdiffs = co_await cppcoro::when_all(std::move(subtasks));

    object_tree_diff tree_diff;
    value_diff relevant_diff;
    for (auto& subdiff : subdiffs)
    {
        if (subdiff.first)
        {
            relevant_diff.push_back(std::move(*subdiff.first));
        }
        else
        {
            std::move(
                subdiff.second.begin(),
                subdiff.second.end(),
                std::back_inserter(tree_diff));
        }
    }

    if (!relevant_diff.empty())
    {
        object_node_diff node_diff;
        node_diff.service = thinknode_service_id::ISS;
        node_diff.path_from_root = value_diff_path();
        node_diff.id_in_a = object_id_a;
        node_diff.id_in_b = object_id_b;
        node_diff.diff = std::move(relevant_diff);
        tree_diff.push_back(std::move(node_diff));
    }

    co_return tree_diff;
}

struct simple_calculation_submitter : calculation_submission_interface
{
    service_core& service;

    simple_calculation_submitter(service_core& service) : service(service)
    {
    }

    virtual cppcoro::task<optional<string>>
    submit(
        thinknode_session session,
        string context_id,
        calculation_request request,
        bool dry_run)
    {
        auto submitter = [](service_core& service,
                            thinknode_session session,
                            string context_id,
                            calculation_request request,
                            bool dry_run) -> cppcoro::task<optional<string>> {
            // If the calculation is simply a reference, just return the ID
            // directly.
            if (is_reference(request))
                co_return some(as_reference(request));

            if (dry_run)
            {
                CRADLE_THROW(
                    websocket_server_error() << internal_error_message_info(
                        "dry runs aren't supported yet"));
            }

            co_return some(co_await post_calculation_piecewise(
                service,
                std::move(session),
                std::move(context_id),
                std::move(request)));
        };

        return submitter(
            service,
            std::move(session),
            std::move(context_id),
            std::move(request),
            dry_run);
    }
};

static cppcoro::shared_task<string>
resolve_meta_chain(
    service_core& service,
    thinknode_session session,
    string context_id,
    calculation_request request)
{
    CRADLE_LOG_CALL(
        << CRADLE_LOG_ARG(session) << CRADLE_LOG_ARG(context_id)
        << CRADLE_LOG_ARG(request))

    simple_calculation_submitter submitter(service);
    while (is_meta(request))
    {
        auto generator = as_meta(std::move(request)).generator;
        request
            = from_dynamic<calculation_request>(co_await perform_local_calc(
                service, session, context_id, std::move(generator)));
    }
    auto submission_info = co_await submit_let_calculation_request(
        submitter,
        session,
        context_id,
        augmented_calculation_request{std::move(request), {}});
    co_return submission_info->main_calc_id;
}

static cppcoro::shared_task<dynamic>
locally_resolve_meta_chain(
    service_core& service,
    thinknode_session session,
    string context_id,
    calculation_request request)
{
    CRADLE_LOG_CALL(
        << CRADLE_LOG_ARG(session) << CRADLE_LOG_ARG(context_id)
        << CRADLE_LOG_ARG(request))

    simple_calculation_submitter submitter(service);
    while (is_meta(request))
    {
        auto generator = as_meta(std::move(request)).generator;
        request
            = from_dynamic<calculation_request>(co_await perform_local_calc(
                service, session, context_id, std::move(generator)));
    }

    co_return co_await perform_local_calc(
        service, session, context_id, std::move(request));
}

static cppcoro::task<object_tree_diff>
compute_calc_tree_diff(
    service_core& service,
    thinknode_session session,
    string context_id_a,
    string calc_id_a,
    string context_id_b,
    string calc_id_b);

cppcoro::task<std::pair<std::optional<value_diff_item>, object_tree_diff>>
process_calc_tree_subdiff(
    service_core& service,
    thinknode_session session,
    string context_id_a,
    string context_id_b,
    value_diff_item item)
{
    if (item.op == value_diff_op::UPDATE && !item.path.empty()
        && item.path.back() == "reference")
    {
        auto id_a = cast<string>(*item.a);
        auto id_b = cast<string>(*item.b);

        auto service_a = get_thinknode_service_id(id_a);
        auto service_b = get_thinknode_service_id(id_b);

        object_tree_diff subtree_diff;

        if (service_a == thinknode_service_id::CALC
            && service_b == thinknode_service_id::CALC)
        {
            subtree_diff = co_await compute_calc_tree_diff(
                service, session, context_id_a, id_a, context_id_b, id_b);
        }
        else
        {
            subtree_diff = co_await compute_iss_tree_diff(
                service, session, context_id_a, id_a, context_id_b, id_b);
        }

        for (auto& node : subtree_diff)
        {
            node.path_from_root.insert(
                node.path_from_root.begin(),
                item.path.begin(),
                item.path.end() - 1);
        }

        co_return std::make_pair(none, subtree_diff);
    }
    else
    {
        co_return std::make_pair(some(std::move(item)), object_tree_diff());
    }
}

static cppcoro::task<object_tree_diff>
compute_calc_tree_diff(
    service_core& service,
    thinknode_session session,
    string context_id_a,
    string calc_id_a,
    string context_id_b,
    string calc_id_b)
{
    auto [calc_a, calc_b] = co_await cppcoro::when_all(
        retrieve_calculation_request(
            service, session, context_id_a, calc_id_a),
        retrieve_calculation_request(
            service, session, context_id_b, calc_id_b));
    auto diff = compute_value_diff(to_dynamic(calc_a), to_dynamic(calc_b));

    auto subtasks = map(
        [=, &service](auto item)
            -> cppcoro::task<
                std::pair<std::optional<value_diff_item>, object_tree_diff>> {
            return process_calc_tree_subdiff(
                service, session, context_id_a, context_id_b, std::move(item));
        },
        std::move(diff));

    auto subdiffs = co_await cppcoro::when_all(std::move(subtasks));

    object_tree_diff tree_diff;
    value_diff relevant_diff;
    for (auto& subdiff : subdiffs)
    {
        if (subdiff.first)
        {
            relevant_diff.push_back(std::move(*subdiff.first));
        }
        else
        {
            std::move(
                subdiff.second.begin(),
                subdiff.second.end(),
                std::back_inserter(tree_diff));
        }
    }

    if (!relevant_diff.empty())
    {
        object_node_diff node_diff;
        node_diff.service = thinknode_service_id::CALC;
        node_diff.path_from_root = value_diff_path();
        node_diff.id_in_a = calc_id_a;
        node_diff.id_in_b = calc_id_b;
        node_diff.diff = std::move(relevant_diff);
        tree_diff.push_back(std::move(node_diff));
    }

    co_return tree_diff;
}

CRADLE_DEFINE_EXCEPTION(unsupported_results_api_query)
CRADLE_DEFINE_ERROR_INFO(string, plan_iss_id)
CRADLE_DEFINE_ERROR_INFO(string, function_name)

namespace uncached {

cppcoro::task<string>
uncached_resolve_results_api_query(
    service_core& service,
    thinknode_session const& session,
    string const& context_id,
    string const& plan_iss_id,
    string const& function,
    std::vector<dynamic> const& args)
{
    CRADLE_LOG_CALL(
        << CRADLE_LOG_ARG(context_id) << CRADLE_LOG_ARG(plan_iss_id)
        << CRADLE_LOG_ARG(function) << CRADLE_LOG_ARG(args))

    // Get the original context ID associated with this plan.
    auto raw_plan_data = co_await get_iss_object(
        service, session, context_id, plan_iss_id, true);
    auto plan_context = cast<string>(
        get_field(cast<dynamic_map>(raw_plan_data), "context_id"));
    // Get an ID for the plan as a dynamic value.
    // This has to be done in the plan's original context so that Thinknode
    // doesn't upgrade it.
    auto dynamic_plan_id = co_await post_calculation(
        service,
        session,
        plan_context,
        make_calculation_request_with_cast(calculation_cast_request(
            make_thinknode_type_info_with_dynamic_type(
                thinknode_dynamic_type()),
            make_calculation_request_with_reference(plan_iss_id))));

    // Issue the "api_" form of the generator request.
    std::vector<calculation_request> calc_args;
    calc_args.reserve(args.size() + 1);
    // The first argument is always the (dynamic) plan.
    calc_args.push_back(
        make_calculation_request_with_reference(dynamic_plan_id));
    for (auto const& arg : args)
        calc_args.push_back(make_calculation_request_with_value(arg));
    auto generator_request
        = make_calculation_request_with_function(function_application(
            "mgh", "planning", "api_" + function, none, calc_args));
    auto generator_calc_id = co_await post_calculation(
        service, session, context_id, generator_request);
    auto generated_request
        = from_dynamic<results_api_generated_request>(co_await get_iss_object(
            service, session, context_id, generator_calc_id));

    // If there's no generated request, this query isn't supported by the
    // version of the planning app that created the plan.
    if (!generated_request.request)
    {
        CRADLE_THROW(
            unsupported_results_api_query()
            << plan_iss_id_info(plan_iss_id) << function_name_info(function));
    }
    // Otherwise, submit the request as a meta generator.
    co_return co_await resolve_meta_chain(
        service,
        session,
        generated_request.context_id,
        make_calculation_request_with_meta(meta_calculation_request{
            *std::move(generated_request.request),
            // This isn't used.
            make_thinknode_type_info_with_dynamic_type(
                thinknode_dynamic_type())}));
}

cppcoro::task<dynamic>
locally_resolve_results_api_query(
    service_core& service,
    thinknode_session const& session,
    string const& context_id,
    string const& plan_iss_id,
    string const& function,
    std::vector<dynamic> const& args)
{
    // Get the original context ID associated with this plan.
    auto raw_plan_data = co_await get_iss_object(
        service, session, context_id, plan_iss_id, true);
    auto plan_context = cast<string>(
        get_field(cast<dynamic_map>(raw_plan_data), "context_id"));
    // Get an ID for the plan as a dynamic value.
    // This has to be done in the plan's original context so that Thinknode
    // doesn't upgrade it.
    auto dynamic_plan_id = co_await post_calculation(
        service,
        session,
        plan_context,
        make_calculation_request_with_cast(calculation_cast_request(
            make_thinknode_type_info_with_dynamic_type(
                thinknode_dynamic_type()),
            make_calculation_request_with_reference(plan_iss_id))));

    // Issue the "api_" form of the generator request.
    std::vector<calculation_request> calc_args;
    calc_args.reserve(args.size() + 1);
    // The first argument is always the (dynamic) plan.
    calc_args.push_back(
        make_calculation_request_with_reference(dynamic_plan_id));
    for (auto const& arg : args)
        calc_args.push_back(make_calculation_request_with_value(arg));
    auto generator_request
        = make_calculation_request_with_function(function_application(
            "mgh", "planning", "api_" + function, none, calc_args));
    auto generator_calc_id = co_await post_calculation(
        service, session, context_id, generator_request);
    auto generated_request
        = from_dynamic<results_api_generated_request>(co_await get_iss_object(
            service, session, context_id, generator_calc_id));

    // If there's no generated request, this query isn't supported by the
    // version of the planning app that created the plan.
    if (!generated_request.request)
    {
        CRADLE_THROW(
            unsupported_results_api_query()
            << plan_iss_id_info(plan_iss_id) << function_name_info(function));
    }
    // Otherwise, submit the request as a meta generator.
    co_return co_await locally_resolve_meta_chain(
        service,
        session,
        generated_request.context_id,
        make_calculation_request_with_meta(meta_calculation_request{
            *std::move(generated_request.request),
            // This isn't used.
            make_thinknode_type_info_with_dynamic_type(
                thinknode_dynamic_type())}));
}

} // namespace uncached

cppcoro::task<string>
resolve_results_api_query(
    service_core& service,
    thinknode_session const& session,
    string const& context_id,
    string const& plan_iss_id,
    string const& function,
    std::vector<dynamic> const& args)
{
    CRADLE_LOG_CALL(
        << CRADLE_LOG_ARG(context_id) << CRADLE_LOG_ARG(plan_iss_id)
        << CRADLE_LOG_ARG(function) << CRADLE_LOG_ARG(args))

    auto cache_key = make_sha256_hashed_id(
        "resolve_results_api_query",
        session.api_url,
        context_id,
        plan_iss_id,
        function,
        args);

    co_return co_await fully_cached<string>(service, cache_key, [&] {
        return uncached::uncached_resolve_results_api_query(
            service, session, context_id, plan_iss_id, function, args);
    });
}

cppcoro::task<dynamic>
locally_resolve_results_api_query(
    service_core& service,
    thinknode_session const& session,
    string const& context_id,
    string const& plan_iss_id,
    string const& function,
    std::vector<dynamic> const& args)
{
    CRADLE_LOG_CALL(
        << CRADLE_LOG_ARG(context_id) << CRADLE_LOG_ARG(plan_iss_id)
        << CRADLE_LOG_ARG(function) << CRADLE_LOG_ARG(args))

    auto cache_key = make_sha256_hashed_id(
        "locally_resolve_results_api_query",
        session.api_url,
        context_id,
        plan_iss_id,
        function,
        args);

    co_return co_await fully_cached<dynamic>(service, cache_key, [&] {
        return uncached::locally_resolve_results_api_query(
            service, session, context_id, plan_iss_id, function, args);
    });
}

static void
send_response(
    websocket_server_impl& server,
    client_request const& request,
    server_message_content const& content)
{
    CRADLE_LOG_CALL(
        << CRADLE_LOG_ARG(request.message) << CRADLE_LOG_ARG(content))
    send(
        server,
        request.client,
        make_websocket_server_message(request.message.request_id, content));
}

static cppcoro::task<>
process_message(websocket_server_impl& server, client_request request)
{
    CRADLE_LOG_CALL(<< CRADLE_LOG_ARG(request.message))

    auto const& content = request.message.content;
    if (get_client(server.clients, request.client).session.api_url.empty()
        && !is_registration(content))
    {
        spdlog::get("cradle")->error("unregistered client");
        send_response(
            server,
            request,
            make_server_message_content_with_error(
                make_error_response_with_unregistered_client(nil)));
        co_return;
    }
    switch (get_tag(content))
    {
        case client_message_content_tag::REGISTRATION: {
            auto const& registration = as_registration(content);
            update_client(server.clients, request.client, [&](auto& client) {
                client.name = registration.name;
                client.session = registration.session;
            });
            send_response(
                server,
                request,
                make_server_message_content_with_registration_acknowledgement(
                    nil));
            break;
        }
        case client_message_content_tag::TEST: {
            websocket_test_response response;
            response.name = get_client(server.clients, request.client).name;
            response.message = as_test(content).message;
            send_response(
                server,
                request,
                make_server_message_content_with_test(response));
            break;
        }
        case client_message_content_tag::CACHE_INSERT: {
            auto const& insertion = as_cache_insert(content);
            server.core.internals().disk_cache.insert(
                insertion.key, insertion.value);
            send_response(
                server,
                request,
                make_server_message_content_with_cache_insert_acknowledgement(
                    nil));
            break;
        }
        case client_message_content_tag::CACHE_QUERY: {
            auto const& key = as_cache_query(content);
            auto entry = server.core.internals().disk_cache.find(key);
            send_response(
                server,
                request,
                make_server_message_content_with_cache_response(
                    make_websocket_cache_response(
                        key, entry ? entry->value : none)));
            break;
        }
        case client_message_content_tag::ISS_OBJECT: {
            auto const& gio = as_iss_object(content);
            auto msgpack_data = co_await get_iss_blob(
                server.core,
                get_client(server.clients, request.client).session,
                gio.context_id,
                gio.object_id,
                gio.ignore_upgrades);
            auto encoded_object = encode_object(gio.encoding, msgpack_data);
            send_response(
                server,
                request,
                make_server_message_content_with_iss_object_response(
                    iss_object_response{std::move(encoded_object)}));
            break;
        }
        case client_message_content_tag::RESOLVE_ISS_OBJECT: {
            auto const& rio = as_resolve_iss_object(content);
            auto immutable_id = co_await resolve_iss_object_to_immutable(
                server.core,
                get_client(server.clients, request.client).session,
                rio.context_id,
                rio.object_id,
                rio.ignore_upgrades);
            send_response(
                server,
                request,
                make_server_message_content_with_resolve_iss_object_response(
                    resolve_iss_object_response{immutable_id}));
            break;
        }
        case client_message_content_tag::ISS_OBJECT_METADATA: {
            auto const& giom = as_iss_object_metadata(content);
            auto metadata = co_await get_iss_object_metadata(
                server.core,
                get_client(server.clients, request.client).session,
                giom.context_id,
                giom.object_id);
            send_response(
                server,
                request,
                make_server_message_content_with_iss_object_metadata_response(
                    iss_object_metadata_response{std::move(metadata)}));
            break;
        }
        case client_message_content_tag::POST_ISS_OBJECT: {
            auto const& pio = as_post_iss_object(content);
            auto object_id = co_await post_iss_object(
                server.core,
                get_client(server.clients, request.client).session,
                pio.context_id,
                parse_url_type_string(pio.schema),
                pio.encoding,
                pio.object);
            send_response(
                server,
                request,
                make_server_message_content_with_post_iss_object_response(
                    make_post_iss_object_response(object_id)));
            break;
        }
        case client_message_content_tag::COPY_ISS_OBJECT: {
            auto const& cio = as_copy_iss_object(content);
            auto source_bucket
                = (co_await get_context_contents(
                       server.core,
                       get_client(server.clients, request.client).session,
                       cio.source_context_id))
                      .bucket;
            co_await deeply_copy_iss_object(
                server.core,
                get_client(server.clients, request.client).session,
                source_bucket,
                cio.source_context_id,
                cio.destination_context_id,
                cio.object_id);
            send_response(
                server,
                request,
                make_server_message_content_with_copy_iss_object_response(
                    nil));
            break;
        }
        case client_message_content_tag::COPY_CALCULATION: {
            auto const& cc = as_copy_calculation(content);
            auto source_bucket
                = (co_await get_context_contents(
                       server.core,
                       get_client(server.clients, request.client).session,
                       cc.source_context_id))
                      .bucket;
            co_await deeply_copy_calculation(
                server.core,
                get_client(server.clients, request.client).session,
                source_bucket,
                cc.source_context_id,
                cc.destination_context_id,
                cc.calculation_id);
            send_response(
                server,
                request,
                make_server_message_content_with_copy_calculation_response(
                    nil));
            break;
        }
        case client_message_content_tag::CALCULATION_REQUEST: {
            auto const& gcr = as_calculation_request(content);
            auto calc = co_await retrieve_calculation_request(
                server.core,
                get_client(server.clients, request.client).session,
                gcr.context_id,
                gcr.calculation_id);
            send_response(
                server,
                request,
                make_server_message_content_with_calculation_request_response(
                    make_calculation_request_response(calc)));
            break;
        }
        case client_message_content_tag::POST_CALCULATION: {
            auto const& pc = as_post_calculation(content);
            auto calc_id = co_await post_calculation_piecewise(
                server.core,
                get_client(server.clients, request.client).session,
                pc.context_id,
                pc.calculation);
            send_response(
                server,
                request,
                make_server_message_content_with_post_calculation_response(
                    make_post_calculation_response(calc_id)));
            break;
        }
        case client_message_content_tag::ISS_DIFF: {
            auto const& idr = as_iss_diff(content);
            auto diff = co_await compute_iss_tree_diff(
                server.core,
                get_client(server.clients, request.client).session,
                idr.context_a,
                idr.id_a,
                idr.context_b,
                idr.id_b);
            send_response(
                server,
                request,
                make_server_message_content_with_iss_diff_response(diff));
            break;
        }
        case client_message_content_tag::CALCULATION_SEARCH: {
            auto const& csr = as_calculation_search(content);
            auto matches = co_await search_calculation(
                server.core,
                get_client(server.clients, request.client).session,
                csr.context_id,
                csr.calculation_id,
                csr.search_string);
            send_response(
                server,
                request,
                make_server_message_content_with_calculation_search_response(
                    make_calculation_search_response(matches)));
            break;
        }
        case client_message_content_tag::CALCULATION_DIFF: {
            auto const& cdr = as_calculation_diff(content);
            auto diff = co_await compute_calc_tree_diff(
                server.core,
                get_client(server.clients, request.client).session,
                cdr.context_a,
                cdr.id_a,
                cdr.context_b,
                cdr.id_b);
            send_response(
                server,
                request,
                make_server_message_content_with_calculation_diff_response(
                    diff));
            break;
        }
        case client_message_content_tag::RESOLVE_META_CHAIN: {
            auto const& rmc = as_resolve_meta_chain(content);
            auto calc_id = co_await resolve_meta_chain(
                server.core,
                get_client(server.clients, request.client).session,
                rmc.context_id,
                make_calculation_request_with_meta(meta_calculation_request{
                    std::move(rmc.generator),
                    // This isn't used.
                    make_thinknode_type_info_with_dynamic_type(
                        thinknode_dynamic_type())}));
            send_response(
                server,
                request,
                make_server_message_content_with_resolve_meta_chain_response(
                    make_resolve_meta_chain_response(calc_id)));
            break;
        }
        case client_message_content_tag::PERFORM_LOCAL_CALC: {
            auto const& pc = as_perform_local_calc(content);
            auto result = co_await perform_local_calc(
                server.core,
                get_client(server.clients, request.client).session,
                pc.context_id,
                pc.calculation);
            send_response(
                server,
                request,
                make_server_message_content_with_local_calc_result(result));
            break;
        }
        case client_message_content_tag::RESULTS_API_QUERY: {
            auto const& raq = as_results_api_query(content);
            auto result = co_await resolve_results_api_query(
                server.core,
                get_client(server.clients, request.client).session,
                raq.context_id,
                raq.plan_iss_id,
                raq.function,
                raq.args);
            send_response(
                server,
                request,
                make_server_message_content_with_results_api_response(
                    make_results_api_response(result)));
            break;
        }
        case client_message_content_tag::LOCAL_RESULTS_API_QUERY: {
            auto const& raq = as_local_results_api_query(content);
            auto result = co_await locally_resolve_results_api_query(
                server.core,
                get_client(server.clients, request.client).session,
                raq.context_id,
                raq.plan_iss_id,
                raq.function,
                raq.args);
            send_response(
                server,
                request,
                make_server_message_content_with_local_results_api_response(
                    make_local_results_api_response(result)));
            break;
        }
        default:
        case client_message_content_tag::KILL: {
            break;
        }
    }
}

static cppcoro::task<>
process_message_with_error_handling(
    websocket_server_impl& server, client_request request)
{
    try
    {
        co_await process_message(server, request);
    }
    catch (bad_http_status_code& e)
    {
        spdlog::get("cradle")->error(e.what());
        send_response(
            server,
            request,
            make_server_message_content_with_error(
                make_error_response_with_bad_status_code(
                    make_http_failure_info(
                        get_required_error_info<attempted_http_request_info>(
                            e),
                        get_required_error_info<http_response_info>(e)))));
    }
    catch (std::exception& e)
    {
        spdlog::get("cradle")->error(e.what());
        send_response(
            server,
            request,
            make_server_message_content_with_error(
                make_error_response_with_unknown(e.what())));
    }
    co_return;
}

static void
on_open(websocket_server_impl& server, connection_hdl hdl)
{
    add_client(server.clients, hdl);
}

static void
on_close(websocket_server_impl& server, connection_hdl hdl)
{
    remove_client(server.clients, hdl);
}

static void
on_message(
    websocket_server_impl& server,
    connection_hdl hdl,
    ws_server_type::message_ptr raw_message)
{
    string request_id;
    try
    {
        auto dynamic_message = parse_msgpack_value(raw_message->get_payload());
        request_id = cast<string>(
            get_field(cast<dynamic_map>(dynamic_message), "request_id"));
        websocket_client_message message;
        from_dynamic(&message, dynamic_message);
        if (is_kill(message.content))
        {
            server.ws.stop_listening();
            for_each_client(
                server.clients,
                [&](connection_hdl hdl, client_connection const& client) {
                    server.ws.close(
                        hdl, websocketpp::close::status::going_away, "killed");
                });
        }
        else
        {
            server.async_scope.spawn(schedule_on(
                server.pool,
                process_message_with_error_handling(
                    server, client_request{hdl, std::move(message)})));
        }
    }
    catch (std::exception& e)
    {
        spdlog::get("cradle")->error("error processing message: {}", e.what());
        send(
            server,
            hdl,
            make_websocket_server_message(
                request_id,
                make_server_message_content_with_error(
                    make_error_response_with_unknown(e.what()))));
    }
}

static void
initialize(websocket_server_impl& server, server_config const& config)
{
    server.config = config;

    server.core.reset(config);

    server.ws.clear_access_channels(websocketpp::log::alevel::all);
    server.ws.init_asio();
    server.ws.set_open_handler(
        [&](connection_hdl hdl) { on_open(server, hdl); });
    server.ws.set_close_handler(
        [&](connection_hdl hdl) { on_close(server, hdl); });
    server.ws.set_message_handler(
        [&](connection_hdl hdl, ws_server_type::message_ptr message) {
            on_message(server, hdl, message);
        });

    // Create and register the logger.
    if (!spdlog::get("cradle"))
    {
        std::vector<spdlog::sink_ptr> sinks;
#ifdef _WIN32
        sinks.push_back(
            std::make_shared<spdlog::sinks::wincolor_stdout_sink_mt>());
#else
        sinks.push_back(
            std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>());
#endif
        auto log_path = get_user_logs_dir(none, "cradle") / "log";
        sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_path.string(), 262144, 2));
        auto combined_logger = std::make_shared<spdlog::logger>(
            "cradle", begin(sinks), end(sinks));
        spdlog::register_logger(combined_logger);
    }
}

websocket_server::websocket_server(server_config const& config)
{
    impl_ = new websocket_server_impl;
    initialize(*impl_, config);
}

websocket_server::~websocket_server()
{
    if (impl_)
    {
        cppcoro::sync_wait(impl_->async_scope.join());
        delete impl_;
    }
}

void
websocket_server::listen()
{
    auto& server = *impl_;
    bool open = server.config.open ? *server.config.open : false;
    auto port = server.config.port ? *server.config.port : 41071;
    if (open)
    {
        server.ws.listen(port);
    }
    else
    {
        server.ws.listen(boost::asio::ip::tcp::endpoint(
            boost::asio::ip::address::from_string("127.0.0.1"), port));
    }
    server.ws.start_accept();
}

void
websocket_server::run()
{
    auto& server = *impl_;

    server.ws.run();

    cppcoro::sync_wait(server.async_scope.join());
}

} // namespace cradle
