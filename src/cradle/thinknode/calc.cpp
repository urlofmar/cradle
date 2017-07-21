#include <cradle/thinknode/calc.hpp>

#include <boost/format.hpp>

#include <cradle/io/http_requests.hpp>
#include <cradle/core/monitoring.hpp>

namespace cradle {

optional<calculation_status>
get_next_calculation_status(calculation_status current)
{
    switch (get_tag(current))
    {
     case calculation_status_tag::WAITING:
        return
            make_calculation_status_with_queued(
                calculation_queue_type::PENDING);
     case calculation_status_tag::GENERATING:
        return
            make_calculation_status_with_queued(
                calculation_queue_type::READY);
     case calculation_status_tag::QUEUED:
        switch (as_queued(current))
        {
         case calculation_queue_type::PENDING:
            return
                make_calculation_status_with_queued(
                    calculation_queue_type::READY);
         case calculation_queue_type::READY:
            return
                make_calculation_status_with_calculating(
                    calculation_calculating_status{0});
         default:
            CRADLE_THROW(
                invalid_enum_value() <<
                    enum_id_info("calculation_queue_type") <<
                    enum_value_info(static_cast<int>(as_queued(current))));
        }
     case calculation_status_tag::CALCULATING:
      {
        // Wait for progress in increments of 1%.
        // The extra .0001 is just to make sure that we don't get rounded back down.
        auto next_progress =
            std::floor(as_calculating(current).progress * 100 + 1.0001) / 100;
        // Once we get to the end of the calculating phase, we want to wait
        // for the upload.
        return
            next_progress < 1
          ? make_calculation_status_with_calculating(
                calculation_calculating_status{next_progress})
          : make_calculation_status_with_uploading(
                calculation_uploading_status());
      }
     case calculation_status_tag::UPLOADING:
      {
        // Wait for progress in increments of 1%.
        // The extra .0001 is just to make sure that we don't get rounded back down.
        auto next_progress =
            std::floor(as_uploading(current).progress * 100 + 1.0001) / 100;
        // Once we get to the end of the calculating phase, we want to wait
        // for the completed status.
        return
            next_progress < 1
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
            invalid_enum_value() <<
                enum_id_info("calculation_status_tag") <<
                enum_value_info(static_cast<int>(get_tag(current))));
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
                invalid_enum_value() <<
                    enum_id_info("calculation_queue_type") <<
                    enum_value_info(static_cast<int>(as_queued(status))));
        }
     case calculation_status_tag::CALCULATING:
        return
            str(boost::format("status=calculating&progress=%4.2f") %
                as_calculating(status).progress);
     case calculation_status_tag::UPLOADING:
        return
            str(boost::format("status=uploading&progress=%4.2f") %
                as_uploading(status).progress);
     case calculation_status_tag::COMPLETED:
        return "status=completed";
     case calculation_status_tag::FAILED:
        return "status=failed";
     case calculation_status_tag::CANCELED:
         return "status=canceled";
     default:
        CRADLE_THROW(
            invalid_enum_value() <<
                enum_id_info("calculation_status_tag") <<
                enum_value_info(static_cast<int>(get_tag(status))));
    }
}

calculation_status
query_calculation_status(
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& calc_id)
{
    auto query =
        make_get_request(
            session.api_url + "/calc/" + calc_id + "/status?context=" + context_id,
            {
                { "Authorization", "Bearer '" + session.access_token + "'" },
                { "Accept", "application/json" }
            });
    null_check_in check_in;
    null_progress_reporter reporter;
    auto response = connection.perform_request(check_in, reporter, query);
    return from_value<calculation_status>(parse_json_response(response));
}

void
long_poll_calculation_status(
    check_in_interface& check_in,
    std::function<void (calculation_status const&)> const& process_status,
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& calc_id)
{
    // Query the initial status.
    auto status = query_calculation_status(connection, session, context_id, calc_id);

    while (true)
    {
        process_status(status);
        check_in();

        // Determine the next meaningful calculation status.
        auto next_status = get_next_calculation_status(status);
        // If there is none, we're done here.
        if (!next_status)
            return;

        // Long poll for that status and update the actual status
        // with whatever Thinknode reports back.
        null_progress_reporter reporter;
        auto long_poll_request =
            make_get_request(
                session.api_url +
                    "/calc/" + calc_id + "/status?" +
                    calc_status_as_query_string(*next_status) +
                    "&timeout=120" +
                    "&context=" + context_id,
                {
                    { "Authorization", "Bearer '" + session.access_token + "'" },
                    { "Accept", "application/json" }
                });
        status =
            cradle::from_value<calculation_status>(
                parse_json_response(
                    connection.perform_request(check_in, reporter, long_poll_request)));
    }
}

}
