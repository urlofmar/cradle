#include <cradle/thinknode/calc.h>

#include <fmt/format.h>

#include <cradle/core/monitoring.h>
#include <cradle/encodings/json.h>
#include <cradle/encodings/sha256_hash_id.h>
#include <cradle/io/http_requests.hpp>
#include <cradle/thinknode/iss.h>
#include <cradle/thinknode/utilities.h>
#include <cradle/utilities/errors.h>
#include <cradle/utilities/functional.h>
#include <cradle/utilities/logging.h>

namespace cradle {

static calculation_request
sanitize_request(calculation_request const& request)
{
    auto recursive_call = [](calculation_request const& request) {
        return sanitize_request(request);
    };
    switch (get_tag(request))
    {
        case calculation_request_tag::REFERENCE:
        case calculation_request_tag::VALUE:
            return request;
        case calculation_request_tag::FUNCTION:
            return make_calculation_request_with_function(
                make_function_application(
                    as_function(request).account,
                    as_function(request).app,
                    as_function(request).name,
                    some(
                        as_function(request).level
                            ? *as_function(request).level
                            : integer(4)),
                    map(recursive_call, as_function(request).args)));
        case calculation_request_tag::ARRAY:
            return make_calculation_request_with_array(
                make_calculation_array_request(
                    map(recursive_call, as_array(request).items),
                    as_array(request).item_schema));
        case calculation_request_tag::ITEM:
            return make_calculation_request_with_item(
                make_calculation_item_request(
                    recursive_call(as_item(request).array),
                    as_item(request).index,
                    as_item(request).schema));
        case calculation_request_tag::OBJECT:
            return make_calculation_request_with_object(
                make_calculation_object_request(
                    map(recursive_call, as_object(request).properties),
                    as_object(request).schema));
        case calculation_request_tag::PROPERTY:
            return make_calculation_request_with_property(
                make_calculation_property_request(
                    recursive_call(as_property(request).object),
                    as_property(request).field,
                    as_property(request).schema));
        case calculation_request_tag::LET:
            return make_calculation_request_with_let(
                make_let_calculation_request(
                    map(recursive_call, as_let(request).variables),
                    recursive_call(as_let(request).in)));
        case calculation_request_tag::VARIABLE:
            return request;
        case calculation_request_tag::META:
            return make_calculation_request_with_meta(
                make_meta_calculation_request(
                    recursive_call(as_meta(request).generator),
                    as_meta(request).schema));
        case calculation_request_tag::CAST:
            return make_calculation_request_with_cast(
                make_calculation_cast_request(
                    as_cast(request).schema,
                    recursive_call(as_cast(request).object)));
        default:
            CRADLE_THROW(
                invalid_enum_value()
                << enum_id_info("calculation_request_tag")
                << enum_value_info(static_cast<int>(get_tag(request))));
    }
}

namespace uncached {

cppcoro::task<string>
post_calculation(
    service_core& service,
    thinknode_session session,
    string context_id,
    calculation_request request)
{
    if (is_reference(request))
        co_return as_reference(request);

    auto request_iss_id = co_await post_iss_object(
        service,
        session,
        context_id,
        make_thinknode_type_info_with_dynamic_type(thinknode_dynamic_type()),
        to_dynamic(sanitize_request(request)));

    auto query = make_http_request(
        http_request_method::POST,
        session.api_url + "/calc/" + request_iss_id + "?context=" + context_id,
        {{"Authorization", "Bearer " + session.access_token}},
        blob());

    auto response = co_await async_http_request(service, query);

    co_return from_dynamic<id_response>(parse_json_response(response)).id;
}

} // namespace uncached

cppcoro::shared_task<string>
post_calculation(
    service_core& service,
    thinknode_session session,
    string context_id,
    calculation_request request)
{
    auto cache_key = make_sha256_hashed_id(
        "post_calculation", session.api_url, context_id, request);

    return fully_cached<string>(service, cache_key, [=, &service] {
        return uncached::post_calculation(
            service, session, context_id, request);
    });
}

optional<calculation_status>
get_next_calculation_status(calculation_status current)
{
    switch (get_tag(current))
    {
        case calculation_status_tag::WAITING:
            return make_calculation_status_with_queued(
                calculation_queue_type::PENDING);
        case calculation_status_tag::GENERATING:
            return make_calculation_status_with_queued(
                calculation_queue_type::READY);
        case calculation_status_tag::QUEUED:
            switch (as_queued(current))
            {
                case calculation_queue_type::PENDING:
                    return make_calculation_status_with_queued(
                        calculation_queue_type::READY);
                case calculation_queue_type::READY:
                    return make_calculation_status_with_calculating(
                        calculation_calculating_status{0});
                default:
                    CRADLE_THROW(
                        invalid_enum_value()
                        << enum_id_info("calculation_queue_type")
                        << enum_value_info(
                               static_cast<int>(as_queued(current))));
            }
        case calculation_status_tag::CALCULATING: {
            // Wait for progress in increments of 1%.
            // The extra .0001 is just to make sure that we don't get rounded
            // back down.
            auto next_progress
                = std::floor(as_calculating(current).progress * 100 + 1.0001)
                  / 100;
            // Once we get to the end of the calculating phase, we want to wait
            // for the upload.
            return next_progress < 1
                       ? make_calculation_status_with_calculating(
                           calculation_calculating_status{next_progress})
                       : make_calculation_status_with_uploading(
                           calculation_uploading_status{0});
        }
        case calculation_status_tag::UPLOADING: {
            // Wait for progress in increments of 1%.
            // The extra .0001 is just to make sure that we don't get rounded
            // back down.
            auto next_progress
                = std::floor(as_uploading(current).progress * 100 + 1.0001)
                  / 100;
            // Once we get to the end of the calculating phase, we want to wait
            // for the completed status.
            return next_progress < 1
                       ? make_calculation_status_with_uploading(
                           calculation_uploading_status{next_progress})
                       : make_calculation_status_with_completed(nil);
        }
        case calculation_status_tag::COMPLETED:
        case calculation_status_tag::FAILED:
        case calculation_status_tag::CANCELED:
            return none;
        default:
            CRADLE_THROW(
                invalid_enum_value()
                << enum_id_info("calculation_status_tag")
                << enum_value_info(static_cast<int>(get_tag(current))));
    }
}

string
calc_status_as_query_string(calculation_status status)
{
    switch (get_tag(status))
    {
        case calculation_status_tag::WAITING:
            return "status=waiting";
        case calculation_status_tag::GENERATING:
            return "status=generating";
        case calculation_status_tag::QUEUED:
            switch (as_queued(status))
            {
                case calculation_queue_type::PENDING:
                    return "status=queued&queued=pending";
                case calculation_queue_type::READY:
                    return "status=queued&queued=ready";
                default:
                    CRADLE_THROW(
                        invalid_enum_value()
                        << enum_id_info("calculation_queue_type")
                        << enum_value_info(
                               static_cast<int>(as_queued(status))));
            }
        case calculation_status_tag::CALCULATING:
            return fmt::format(
                "status=calculating&progress={:4.2f}",
                as_calculating(status).progress);
        case calculation_status_tag::UPLOADING:
            return fmt::format(
                "status=uploading&progress={:4.2f}",
                as_uploading(status).progress);
        case calculation_status_tag::COMPLETED:
            return "status=completed";
        case calculation_status_tag::FAILED:
            return "status=failed";
        case calculation_status_tag::CANCELED:
            return "status=canceled";
        default:
            CRADLE_THROW(
                invalid_enum_value()
                << enum_id_info("calculation_status_tag")
                << enum_value_info(static_cast<int>(get_tag(status))));
    }
}

cppcoro::task<calculation_status>
query_calculation_status(
    service_core& service,
    thinknode_session session,
    string context_id,
    string calc_id)
{
    auto query = make_get_request(
        session.api_url + "/calc/" + calc_id + "/status?context=" + context_id,
        {{"Authorization", "Bearer " + session.access_token},
         {"Accept", "application/json"}});

    auto response = co_await async_http_request(service, query);

    co_return from_dynamic<calculation_status>(parse_json_response(response));
}

cppcoro::async_generator<calculation_status>
long_poll_calculation_status(
    service_core& service,
    thinknode_session session,
    string context_id,
    string calc_id)
{
    // Query the initial status.
    auto status = co_await query_calculation_status(
        service, session, context_id, calc_id);

    while (true)
    {
        co_yield status;

        // Determine the next meaningful calculation status.
        auto next_status = get_next_calculation_status(status);
        // If there is none, we're done here.
        if (!next_status)
            co_return;

        // Long poll for that status and update :status with whatever Thinknode
        // reports back.
        auto long_poll_request = make_get_request(
            session.api_url + "/calc/" + calc_id + "/status?"
                + calc_status_as_query_string(*next_status) + "&timeout=120"
                + "&context=" + context_id,
            {{"Authorization", "Bearer " + session.access_token},
             {"Accept", "application/json"}});

        auto response
            = co_await async_http_request(service, long_poll_request);

        status
            = from_dynamic<calculation_status>(parse_json_response(response));
    }
}

namespace uncached {

cppcoro::task<calculation_request>
retrieve_calculation_request(
    service_core& service,
    thinknode_session session,
    string context_id,
    string calc_id)
{
    auto query = make_get_request(
        session.api_url + "/calc/" + calc_id + "?context=" + context_id,
        {{"Authorization", "Bearer " + session.access_token},
         {"Accept", "application/json"}});

    auto response = co_await async_http_request(service, query);

    co_return from_dynamic<calculation_request>(parse_json_response(response));
}

} // namespace uncached

cppcoro::shared_task<calculation_request>
retrieve_calculation_request(
    service_core& service,
    thinknode_session session,
    string context_id,
    string calc_id)
{
    auto cache_key = make_sha256_hashed_id(
        "retrieve_calculation_request", session.api_url, context_id, calc_id);

    return fully_cached<calculation_request>(
        service, cache_key, [=, &service] {
            return uncached::retrieve_calculation_request(
                service, session, context_id, calc_id);
        });
}

// Substitute the variables in a Thinknode request for new requests.
calculation_request
substitute_variables(
    std::map<string, calculation_request> const& substitutions,
    calculation_request const& request)
{
    auto recursive_call
        = [&substitutions](calculation_request const& request) {
              return substitute_variables(substitutions, request);
          };
    switch (get_tag(request))
    {
        case calculation_request_tag::REFERENCE:
        case calculation_request_tag::VALUE:
            return request;
        case calculation_request_tag::FUNCTION:
            return make_calculation_request_with_function(
                make_function_application(
                    as_function(request).account,
                    as_function(request).app,
                    as_function(request).name,
                    as_function(request).level,
                    map(recursive_call, as_function(request).args)));
        case calculation_request_tag::ARRAY:
            return make_calculation_request_with_array(
                make_calculation_array_request(
                    map(recursive_call, as_array(request).items),
                    as_array(request).item_schema));
        case calculation_request_tag::ITEM:
            return make_calculation_request_with_item(
                make_calculation_item_request(
                    recursive_call(as_item(request).array),
                    as_item(request).index,
                    as_item(request).schema));
        case calculation_request_tag::OBJECT:
            return make_calculation_request_with_object(
                make_calculation_object_request(
                    map(recursive_call, as_object(request).properties),
                    as_object(request).schema));
        case calculation_request_tag::PROPERTY:
            return make_calculation_request_with_property(
                make_calculation_property_request(
                    recursive_call(as_property(request).object),
                    as_property(request).field,
                    as_property(request).schema));
        case calculation_request_tag::LET:
            CRADLE_THROW(
                internal_check_failed() << internal_error_message_info(
                    "encountered let request during variable substitution"));
        case calculation_request_tag::VARIABLE: {
            auto substitution = substitutions.find(as_variable(request));
            if (substitution == substitutions.end())
            {
                CRADLE_THROW(
                    internal_check_failed() << internal_error_message_info(
                        "missing variable substitution"));
            }
            return substitution->second;
        }
        case calculation_request_tag::META:
            return make_calculation_request_with_meta(
                make_meta_calculation_request(
                    recursive_call(as_meta(request).generator),
                    as_meta(request).schema));
        case calculation_request_tag::CAST:
            return make_calculation_request_with_cast(
                make_calculation_cast_request(
                    as_cast(request).schema,
                    recursive_call(as_cast(request).object)));
        default:
            CRADLE_THROW(
                invalid_enum_value()
                << enum_id_info("calculation_request_tag")
                << enum_value_info(static_cast<int>(get_tag(request))));
    }
}

cppcoro::task<optional<let_calculation_submission_info>>
submit_let_calculation_request(
    calculation_submission_interface& submitter,
    thinknode_session session,
    string context_id,
    augmented_calculation_request augmented_request,
    bool dry_run)
{
    let_calculation_submission_info result;

    // We expect this request to be a series of nested let requests, so we'll
    // deconstruct that one-by-one, submitting the requests and recording the
    // substitutions...

    std::map<string, calculation_request> substitutions;

    // :current_request stores a pointer into the full request that indicates
    // how far we've unwrapped it.
    calculation_request const* current_request = &augmented_request.request;

    while (is_let(*current_request))
    {
        auto const& let = as_let(*current_request);

        // Loop through all the variables in :let.
        for (auto const& var : let.variables)
        {
            // Apply the existing substitutions and submit the request.
            auto calculation_id = co_await submitter.submit(
                session,
                context_id,
                substitute_variables(substitutions, var.second),
                dry_run);

            // If there's no calculation ID, then this must be a dry run that
            // hasn't been done yet, so the whole result is none.
            if (!calculation_id)
                co_return none;

            // We got a calculation ID, so record the new substitution.
            substitutions[var.first]
                = make_calculation_request_with_reference(*calculation_id);

            // If this is a reported variable, record it.
            auto const& reported = augmented_request.reported_variables;
            if (std::find(reported.begin(), reported.end(), var.first)
                != reported.end())
            {
                result.reported_subcalcs.push_back(
                    make_reported_calculation_info(
                        *calculation_id,
                        // We assume that all reported calculations are
                        // function calls.
                        is_function(var.second)
                            ? as_function(var.second).name
                            : "internal error: unrecognized reported calc"));
            }
            // Otherwise, just record its ID.
            else
            {
                result.other_subcalc_ids.push_back(*calculation_id);
            }
        }

        // Proceed to the next level of nesting.
        current_request = &let.in;
    }

    // Now we've made it to the actual request, so again apply the
    // substitutions and submit it.
    auto main_calc_id = co_await submitter.submit(
        session,
        context_id,
        substitute_variables(substitutions, *current_request),
        dry_run);
    if (!main_calc_id)
        co_return none;

    result.main_calc_id = *main_calc_id;

    co_return result;
}

static cppcoro::task<void>
search_calculation(
    service_core& service,
    std::map<string, bool>& is_matching,
    thinknode_session session,
    string context_id,
    string calculation_id,
    string search_string)
{
    // If this calculation has already been searched, don't redo the work.
    if (is_matching.find(calculation_id) != is_matching.end())
        co_return;

    // Get the calculation request;
    calculation_request request;
    try
    {
        request = co_await retrieve_calculation_request(
            service, session, context_id, calculation_id);
    }
    catch (bad_http_status_code& e)
    {
        // When calculation results are copied, their inputs aren't guaranteed
        // to be accessible, and we don't want that to cause an error when
        // trying to search inside such calculations. Instead, we simply log a
        // warning and treat the calculation as if it doesn't contain any
        // matches.
        if (get_error_info<http_response_info>(e)->status_code == 404)
        {
            is_matching[calculation_id] = false;
            spdlog::get("cradle")->warn(
                "failed to search {} due to 404; results may be incomplete",
                calculation_id);
            co_return;
        }
    }

    auto recurse
        = [&](calculation_request const& request) -> cppcoro::task<void> {
        if (is_reference(request))
        {
            auto ref = as_reference(request);
            if (get_thinknode_service_id(ref) == thinknode_service_id::CALC)
            {
                co_await search_calculation(
                    service,
                    is_matching,
                    session,
                    context_id,
                    ref,
                    search_string);
            }
        }
    };

    switch (get_tag(request))
    {
        case calculation_request_tag::REFERENCE:
        case calculation_request_tag::VALUE:
            is_matching[calculation_id] = false;
            break;
        case calculation_request_tag::FUNCTION:
            is_matching[calculation_id]
                = as_function(request).name.find(search_string)
                  != string::npos;
            for (auto const& arg : as_function(request).args)
                co_await recurse(arg);
            break;
        case calculation_request_tag::ARRAY:
            is_matching[calculation_id] = false;
            for (auto const& item : as_array(request).items)
                co_await recurse(item);
            break;
        case calculation_request_tag::ITEM:
            is_matching[calculation_id] = false;
            co_await recurse(as_item(request).array);
            break;
        case calculation_request_tag::OBJECT:
            is_matching[calculation_id] = false;
            for (auto const& property : as_object(request).properties)
                co_await recurse(property.second);
            break;
        case calculation_request_tag::PROPERTY:
            is_matching[calculation_id] = false;
            co_await recurse(as_property(request).object);
            break;
        case calculation_request_tag::LET:
            CRADLE_THROW(
                internal_check_failed() << internal_error_message_info(
                    "resolved calculation request contains 'let'"));
        case calculation_request_tag::VARIABLE:
            CRADLE_THROW(
                internal_check_failed() << internal_error_message_info(
                    "resolved calculation request contains 'variable'"));
        case calculation_request_tag::META:
            is_matching[calculation_id] = false;
            co_await recurse(as_meta(request).generator);
            break;
        case calculation_request_tag::CAST:
            is_matching[calculation_id] = false;
            co_await recurse(as_cast(request).object);
            break;
        default:
            CRADLE_THROW(
                invalid_enum_value()
                << enum_id_info("calculation_request_tag")
                << enum_value_info(static_cast<int>(get_tag(request))));
    }
}

cppcoro::task<std::vector<string>>
search_calculation(
    service_core& service,
    thinknode_session session,
    string context_id,
    string calculation_id,
    string search_string)
{
    // mapping from calculation IDs to whether or not the corresponding
    // calculation matches the search criteria
    std::map<string, bool> is_matching;

    co_await search_calculation(
        service,
        is_matching,
        session,
        context_id,
        calculation_id,
        search_string);

    // Extract the matching calculation IDs.
    std::vector<string> matches;
    for (auto const& i : is_matching)
    {
        if (i.second)
            matches.push_back(i.first);
    }
    co_return matches;
}

} // namespace cradle
