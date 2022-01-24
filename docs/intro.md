# Introduction
Clients interact with CRADLE via a websocket interface, where data is encoded via MessagePack:

```plantuml
@startuml
actor "client A" as clientA
actor "client B" as clientB
actor "client C" as clientC
agent CRADLE

clientA -down-> CRADLE
clientB -down-> CRADLE
clientC -down-> CRADLE: websocket / MessagePack
@enduml
```

Requests are identified by an id that is set by the client.
Requests are asynchronous; CRADLE sends responses back to the originating client, where
the response includes the request id that was sent by the client:

```plantuml
@startuml
participant "client A" as clientA
participant "client B" as clientB

clientA -> CRADLE: request(id=0)
clientB -> CRADLE: request(id=0)
clientB -> CRADLE: request(id=1)
clientB <-- CRADLE: response(id=1)
clientA <-- CRADLE: response(id=0)
clientB <-- CRADLE: response(id=0)
@enduml
```

CRADLE will calculate the response itself when it can; otherwise, it will get it from the remote
Thinknode, and translate it into a response to the client.

Requests between CRADLE and Thinknode tend to be functionally similar to the ones between clients and CRADLE.
The protocols are different though: Thinknode is accessed via HTTP, and data is encoded in JSON or MessagePack.

Thinknode forbids messages with JSON data size exceeding 5MB; MessagePack is required for those situations.
Consequently, CRADLE uses MessagePack for storing and retrieving immutable objects (that could be too large), and
JSON otherwise.

```plantuml
@startuml
actor client
agent CRADLE
agent Thinknode

client -down-> CRADLE: websocket / MessagePack
CRADLE -right-> Thinknode: HTTP / JSON / MessagePack
@enduml
```

```plantuml
@startuml
client -> CRADLE: request(id=0)
CRADLE -> Thinknode: HTTP request
CRADLE <-- Thinknode: HTTP response
client <-- CRADLE: response(id=0)
@enduml
```

CRADLE and Thinknode have similar designs:

```plantuml
@startuml
actor client

package "CRADLE (local)" {
    agent server as local_server
    database cache as local_cache
    agent "calculation supervisor" as local_supervisor
    agent "calculation provider" as local_provider
    note bottom of local_provider: Docker container

    local_server -down-> local_cache
    local_server -right-> local_supervisor
    local_supervisor -down-> local_provider: IPC / MessagePack
}

package "Thinknode (remote)" {
    agent server as remote_server
    database storage as remote_cache
    agent "calculation supervisor" as remote_supervisor
    agent "calculation provider" as remote_provider
    note bottom of remote_provider: Docker container

    remote_server -down-> remote_cache
    remote_server -right-> remote_supervisor
    remote_supervisor -down-> remote_provider: IPC / MessagePack
}

client -down-> local_server
local_server -> remote_server
@enduml
```

The local CRADLE provides similar functionality as the remote Thinknode.
The components are:

* A server processing incoming requests, and sending back responses.
* Storage for immutable data. In case of Thinknode, storage is permanent; CRADLE uses a
  cache where items can be evicted on an LRU basis.
* A calculation provider, which is a Docker container performing calculations.
* A calculation supervisor, forming the interface between server and calculation provider.

Docker images are identical between CRADLE and Thinknode;
CRADLE retrieves a Docker image from Thinknode when it is not yet available.
As the same Docker image is used for remote and local calculation, the interface between
supervisor and provider must be identical as well; it is based on IPC, using MessagePack
for encoding structured data.

CRADLE locally caches objects; more [here](cache.md).

Several options exist for the request processing in CRADLE:

* The server needs no interaction with other blocks. This is the simple and uncommon case
  (registration, test, kill, ...).
* The requested data is already present in the cache.
* The requested data needs to be retrieved from Thinknode. If possible,
  the data is cached so that a following request for the same data need not go
  to the remote server again.
* The request is for a local calculation, and is performed by the local calculation provider.
  Again, results are stored in the cache wherever possible.
