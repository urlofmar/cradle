#include <cradle/websocket/local_calcs.h>

#include <picosha2.h>

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

#include <cppcoro/sync_wait.hpp>

#include <cradle/core/dynamic.h>
#include <cradle/encodings/msgpack.h>
#include <cradle/encodings/sha256_hash_id.h>
#include <cradle/fs/file_io.h>
#include <cradle/service/core.h>
#include <cradle/thinknode/supervisor.h>
#include <cradle/thinknode/utilities.h>
#include <cradle/utilities/errors.h>
#include <cradle/utilities/functional.h>
#include <cradle/utilities/logging.h>

namespace cradle {

// signatures for functions that we're temporarily borrowing from server.cpp:

cppcoro::shared_task<dynamic>
get_iss_object(
    service_core& service,
    thinknode_session session,
    string context_id,
    string object_id,
    bool ignore_upgrades = false);

cppcoro::shared_task<api_type_info>
resolve_named_type_reference(
    service_core& service,
    thinknode_session session,
    string context_id,
    api_named_type_reference ref);

cppcoro::task<thinknode_app_version_info>
resolve_context_app(
    service_core& service,
    thinknode_session session,
    string context_id,
    string account,
    string app);

// end of temporary borrowing

namespace uncached {

cppcoro::task<dynamic>
perform_local_function_calc(
    service_core& service,
    thinknode_session const& session,
    string const& context_id,
    string const& account,
    string const& app,
    string const& name,
    std::vector<dynamic> const& args)
{
    auto version_info = co_await resolve_context_app(
        service, session, context_id, account, app);

    co_return supervise_thinknode_calculation(
        http_connection_for_thread(service),
        account,
        app,
        as_private(*version_info.manifest->provider).image,
        name,
        args);
}

} // namespace uncached

cppcoro::task<dynamic>
perform_local_function_calc(
    service_core& service,
    thinknode_session const& session,
    string const& context_id,
    string const& account,
    string const& app,
    string const& name,
    std::vector<dynamic> const& args)
{
    auto cache_key = make_sha256_hashed_id(
        "local_function_calc",
        session.api_url,
        context_id,
        account,
        app,
        name,
        args);

    co_return co_await fully_cached<dynamic>(service, cache_key, [&] {
        return uncached::perform_local_function_calc(
            service, session, context_id, account, app, name, args);
    });
}

cppcoro::task<dynamic>
perform_local_calc(
    service_core& service,
    thinknode_session const& session,
    string const& context_id,
    std::map<string, dynamic> const& environment,
    calculation_request const& request)
{
    auto recursive_call
        = [&](calculation_request const& request) -> cppcoro::task<dynamic> {
        return perform_local_calc(
            service, session, context_id, environment, request);
    };
    auto coercive_call
        = [&](thinknode_type_info const& schema, dynamic const& value) {
              return coerce_value(
                  [&](api_named_type_reference const& ref) {
                      auto type_info = resolve_named_type_reference(
                          service, session, context_id, ref);
                      return cppcoro::sync_wait(type_info);
                  },
                  as_api_type(schema),
                  value);
          };

    switch (get_tag(request))
    {
        case calculation_request_tag::REFERENCE:
            co_return co_await get_iss_object(
                service, session, context_id, as_reference(request));
        case calculation_request_tag::VALUE:
            co_return as_value(request);
        case calculation_request_tag::FUNCTION: {
            std::vector<dynamic> arg_values;
            for (auto const& arg : as_function(request).args)
                arg_values.push_back(co_await recursive_call(arg));
            co_return co_await perform_local_function_calc(
                service,
                session,
                context_id,
                as_function(request).account,
                as_function(request).app,
                as_function(request).name,
                arg_values);
        }
        case calculation_request_tag::ARRAY: {
            std::vector<dynamic> values;
            for (auto const& item : as_array(request).items)
            {
                values.push_back(coercive_call(
                    as_array(request).item_schema,
                    co_await recursive_call(item)));
            }
            co_return dynamic(values);
        }
        case calculation_request_tag::ITEM:
            co_return coercive_call(
                as_item(request).schema,
                cast<dynamic_array>(
                    co_await recursive_call(as_item(request).array))
                    .at(boost::numeric_cast<size_t>(cast<integer>(
                        co_await recursive_call(as_item(request).index)))));
        case calculation_request_tag::OBJECT: {
            dynamic_map object;
            for (auto const& item : as_object(request).properties)
            {
                object[dynamic(item.first)]
                    = co_await recursive_call(item.second);
            }
            co_return coercive_call(as_object(request).schema, object);
        }
        case calculation_request_tag::PROPERTY:
            co_return coercive_call(
                as_property(request).schema,
                cast<dynamic_map>(
                    co_await recursive_call(as_property(request).object))
                    .at(cast<string>(
                        co_await recursive_call(as_property(request).field))));
        case calculation_request_tag::LET: {
            std::map<string, dynamic> extended_environment = environment;
            for (auto const& v : as_let(request).variables)
            {
                extended_environment[v.first]
                    = co_await recursive_call(v.second);
            }
            co_return co_await perform_local_calc(
                service,
                session,
                context_id,
                extended_environment,
                as_let(request).in);
        }
        case calculation_request_tag::VARIABLE:
            co_return environment.at(as_variable(request));
        case calculation_request_tag::META:
            co_return coercive_call(
                as_meta(request).schema,
                co_await recursive_call(from_dynamic<calculation_request>(
                    co_await recursive_call(as_meta(request).generator))));
        case calculation_request_tag::CAST:
            co_return coercive_call(
                as_cast(request).schema,
                co_await recursive_call(as_cast(request).object));
        default:
            CRADLE_THROW(
                invalid_enum_value()
                << enum_id_info("calculation_request_tag")
                << enum_value_info(static_cast<int>(get_tag(request))));
    }
}

cppcoro::task<dynamic>
perform_local_calc(
    service_core& service,
    thinknode_session const& session,
    string const& context_id,
    calculation_request const& request)
{
    co_return co_await perform_local_calc(
        service, session, context_id, {}, request);
}

} // namespace cradle
