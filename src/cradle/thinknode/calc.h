#ifndef CRADLE_THINKNODE_CALC_H
#define CRADLE_THINKNODE_CALC_H

#include <cppcoro/async_generator.hpp>

#include <cradle/core.h>
#include <cradle/service/core.h>
#include <cradle/thinknode/types.hpp>

namespace cradle {

struct http_connection_interface;
struct check_in_interface;

// Post a calculation to Thinknode.
cppcoro::shared_task<string>
post_calculation(
    service_core& service,
    thinknode_session session,
    string context_id,
    calculation_request request);

// Given a calculation status, get the next status that would represent
// meaningful progress. If the result is none, no further progress is possible.
optional<calculation_status>
get_next_calculation_status(calculation_status current);

// Get the query string representation of a calculation status.
string
calc_status_as_query_string(calculation_status status);

// Query the status of a calculation.
cppcoro::task<calculation_status>
query_calculation_status(
    service_core& service,
    thinknode_session session,
    string context_id,
    string calc_id);

// Long poll the status of a calculation.
//
// This will continuously long poll the calculation, passing the most recent
// status to :process_status, until no further progress is possible or an
// error occurs.
//
cppcoro::async_generator<calculation_status>
long_poll_calculation_status(
    service_core& service,
    thinknode_session session,
    string context_id,
    string calc_id);

// Retrieve a calculation request from Thinknode.
cppcoro::shared_task<calculation_request>
retrieve_calculation_request(
    service_core& service,
    thinknode_session session,
    string context_id,
    string calc_id);

// Substitute the variables in a Thinknode request for new requests.
calculation_request
substitute_variables(
    std::map<string, calculation_request> const& substitutions,
    calculation_request const& request);

struct calculation_submission_interface
{
    // Submit a calculation to Thinknode and return its ID.
    //
    // If :dry_run is true, then no new calculations will be submitted and the
    // result is only valid if the calculation already exists (hence the
    // optional return type).
    //
    // (The implementation of this can involve one or more levels of caching.)
    //
    virtual cppcoro::task<optional<string>>
    submit(
        thinknode_session session,
        string context_id,
        calculation_request request,
        bool dry_run)
        = 0;
};

// This is an alternative to Thinknode's meta request functionality that uses
// locally generated requests but tries to be as efficient as possible about
// submitting them to Thinknode. It's more responsive than other methods in
// cases where the client is repeatedly submitting many similar requests to
// Thinknode.
//
// In this method, the caller supplies a Thinknode request containing 'let'
// variables that represent repeated subrequests, and rather than submitting
// the entire request, these subrequests are submitted individually and their
// calculation IDs are substituted into higher-level requests in place of the
// 'variable' requests used to reference them. This method has the advantage
// that it can leverage memory and disk caching to avoid resubmitting
// subrequests that have previously been submitted.
//
// The return value is a structure that includes not only the ID of the
// calculation but also information that may be useful for tracking the
// progress of the calculation tree.
//
// If :dry_run is true, then no new calculations will be submitted and the
// result is only valid if the calculation already exists (hence the
// optional return type).
//
cppcoro::task<optional<let_calculation_submission_info>>
submit_let_calculation_request(
    calculation_submission_interface& submitter,
    thinknode_session session,
    string context_id,
    augmented_calculation_request augmented_request,
    bool dry_run = false);

// Search within a calculation request and return a list of subcalculation IDs
// that match :search_string.
// Note that currently the search is limited to matching function names.
cppcoro::task<std::vector<string>>
search_calculation(
    service_core& service,
    thinknode_session session,
    string context_id,
    string calculation_id,
    string search_string);

} // namespace cradle

#endif
