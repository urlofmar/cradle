# Messages
This section aims to give an overview of the messages exchanged between CRADLE and its clients.

Request                    | Response                        | Status     | Link                               | Synopsis
-------------------------- | ------------------------------- | ---------- | ----                               | --------
`registration`             | `registration_acknowledgement`  | CRADLE     |                                    | Register a client
`iss_object`               | `iss_object_response`           | CRADLE     | [Link](msg_iss_object.md)          | Retrieve data for an immutable object
`resolve_iss_object`       | `resolve_iss_object_response`   | CRADLE     |                                    | Convert a reference id to an immutable id; Testing?
`iss_object_metadata`      | `iss_object_metadata_response`  | CRADLE     | [Link](msg_iss_object_metadata.md) | Retrieve metadata for an immutable object
`post_iss_object`          | `post_iss_object_response`      | CRADLE     | [Link](msg_post_iss_object.md)     | Store an immutable object
`post_calculation`         | `post_calculation_response`     | CRADLE     | [Link](msg_post_calculation.md)    | Perform a calculation on Thinknode
`calculation_request`      | `calculation_request_response`  | CRADLE     | [Link](msg_calculation_request.md) | Convert a calculation id to a calculation descriptor
`calculation_diff`         | `calculation_diff_response`     | CRADLE     |                                    | Find the difference between two calculations
`calculation_search`       | `calculation_search_response`   | CRADLE     |                                    | Find the subcalculations calling a specified function
`iss_diff`                 | `iss_diff_response`             | CRADLE     |                                    | Find the difference between two immutable objects
`perform_local_calc`       | `local_calc_result`             | CRADLE     | [Link](msg_perform_local_calc.md)  | Perform a local calculation
`kill`                     | —                               | ?          |                                    | Kill the CRADLE server
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

It should be noted that all these messages are synchronous: CRADLE will send
the response only when the data is available. Thinknode exposes an asynchronous interface,
but where necessary CRADLE will enter a polling loop, querying Thinknode until
the operation has finished.
