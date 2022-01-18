# Messages
This section aims to give an overview of the messages exchanged between CRADLE and its clients.

Request                    | Response                        | Status     | Synopsis
-------------------------- | ------------------------------- | ---------- | --------
`registration`             | `registration_acknowledgement`  | CRADLE     | Register a client
`iss_object`               | `iss_object_response`           | CRADLE     | Retrieve data for an immutable object; more [here](msg_iss_object.md)
`resolve_iss_object`       | `resolve_iss_object_response`   | CRADLE     |
`iss_object_metadata`      | `iss_object_metadata_response`  | CRADLE     |
`post_iss_object`          | `post_iss_object_response`      | CRADLE     | Store an immutable object; more [here](msg_post_iss_object.md)
`post_calculation`         | `post_calculation_response`     | CRADLE     |
`calculation_request`      | `calculation_request_response`  | CRADLE     |
`calculation_diff`         | `calculation_diff_response`     | CRADLE     |
`calculation_search`       | `calculation_search_response`   | CRADLE     |
`iss_diff`                 | `iss_diff_response`             | CRADLE     |
`perform_local_calc`       | `local_calc_result`             | CRADLE     |
`kill`                     | —                               | ?          | Kill the CRADLE server
`test`                     | `test`                          | Testing    |
`cache_insert`             | `cache_insert_acknowledgement`  | Testing    |
`cache_query`              | `cache_response`                | Testing    |
`resolve_meta_chain`       | `resolve_meta_chain_response`   | Astroid    |
`results_api_query`        | `results_api_response`          | Astroid    |
`local_results_api_query`  | `local_results_api_response`    | Astroid    |
`copy_iss_object`          | `copy_iss_object_response`      | Thinknode  |
`copy_calculation`         | `copy_calculation_response`     | Thinknode  |

There's also the `error` response that is sent instead of the normal one, if an error
occurs interpreting or processing the request.

Statuses:

* CRADLE: will stay useful
* Testing: used for testing, not appropriate for normal CRADLE clients
* Astroid: Astroid-specific, candidate for removal
* Thinknode: Thinknode-specific, candidate for removal
