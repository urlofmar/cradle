#ifndef CRADLE_THINKNODE_CALC_HPP
#define CRADLE_THINKNODE_CALC_HPP

#include <cradle/common.hpp>

namespace cradle {

struct http_connection_interface;
struct check_in_interface;

// Given a calculation status, get the next status that would represent
// meaningful progress. If the result is none, no further progress is possible.
optional<calculation_status>
get_next_calculation_status(calculation_status current);

// Get the query string repesentation of a calculation status.
string
calc_status_as_query_string(calculation_status status);

// Query the status of a calculation.
calculation_status
query_calculation_status(
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& calc_id);

// Long poll the status of a calculation.
//
// This will continuously long poll the calculation, passing the most recent
// status to :process_status, until no further progress is possible or an
// error occurs.
//
void
long_poll_calculation_status(
    check_in_interface& check_in,
    boost::function<void (calculation_status const&)> const& process_status,
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& calc_id);

}

#endif
