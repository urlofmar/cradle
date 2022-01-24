# `calculation_request`: retrieve calculation descriptor
A `calculation_request` request asks to retrieve the descriptor for a calculation.
Example message:

```
request_id: 261f52823f3f4414a56e4c8bdeff6668
content:
  calculation_request:
    calculation_id: 61e53771010054e512d49dce6f3ebd8b
    context_id: 5dadeb4a004073e81b5e096255e83652
```

The fields are:

* `request_id`: identifies this request; set by the client
* `context_id`: see [here](data.md)
* `calculation_id`: identifies the calculation; this is a reference id

Example response:

```
request_id: 261f52823f3f4414a56e4c8bdeff6668
calculation_request_response:
  calculation:
    function:
      account: mgh
      app: dosimetry
      args:
        - reference: 61e5377001001d0206ce037e602e97a5
        - value: 0.0276431860394
      name: addition
```

The fields are:

* `request_id`: copied from the request message
* `calculation`: describes the calculation: an addition of a variable `x` and a constant,
  where `x` refers to another calculation


## Cache key
CRADLE will cache the response it gets from Thinknode.
The cache key is a hash depending on:

* The key type (`retrieve_calculation_request`)
* The API URL (e.g. `https://mgh.thinknode.io/api/v1.0`)
* The context id
* The calculation id

## Interaction
Interaction with Thinknode and the cache is similar to other simple queries, like
[iss_object_metadata](msg_iss_object_metadata.md).
