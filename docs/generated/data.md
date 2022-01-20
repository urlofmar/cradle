# Immutable data
An immutable object is characterized by:

* An _immutable id_ that uniquely identifies the object, and its data;
* A _schema_ that describes the structure of the data;
* The data itself.

A one-to-one relation exists between an immutable id and the corresponding immutable data:
if two immutable ids are different, then so will the underlying immutable data differ.

In contrast, an immutable object will also be identified by a number of _reference ids_.
Immutable ids are invisible to CRADLE clients; they use reference ids only.
(A client can retrieve an immutable id using a `resolve_iss_object` request,
but there is no request to put that value in.)

The term _reference id_ comes from the Thinknode documentation; the CRADLE interface
calls them _object ids_. The two terms will be used interchangeably.

All immutable and reference ids originate from Thinknode; CRADLE does not calculate any
of them.

The interpretation of a reference id depends on its _context_, which implies
a map from reference ids to corresponding immutable ids.
A context is identified by a _context id_.
A CRADLE client has to retrieve a context id directly from Thinknode (via a
`GET /iam/realms/:realm/context` request), thus bypassing the CRADLE interface.

There are two kinds of immutable object:

* _Posted objects_, originating from [`post_iss_object`](msg_post_iss_object.md) requests.

  (Thinknode also stores posted objects that do not come from CRADLE, of course.)
* _Calculated objects_, resulting from completed calculations; whether successful or failed.


## CRADLE cache
The CRADLE cache follows the "Implementing a Caching Protocol" proposal in the Thinknode
documentation. It stores

* A context-dependent translation of reference ids to immutable ids;
* A translation of immutable ids to the corresponding immutable data.

By using the two-step lookup, the cache needs to store each immutable object only once;
this is the only place where CRADLE uses immutable ids.
