# GraphLite: Requirements

## 1. Problem Statement

Relationship-heavy queries in relational databases may require multiple
JOIN operations, which can become increasingly expensive as relationship
depth and dataset size increase.

GraphLite explores a graph-based approach where entities are represented
as nodes and relationships as edges, enabling direct graph traversal.

## 2. Goal and Scope

### 2.1 Goal

Build a graph database engine from scratch in C++ and progressively
extend it toward the capabilities of production graph databases such as
JetBrains YouTrackDB.

GraphLite is a learning-oriented implementation: the first milestone
will be deliberately small, while later milestones will introduce more
advanced database internals.

-   Entities -\> Nodes
-   Relationships -\> Edges
-   Information about entities and relationships -\> Properties

### 2.2 Reference System

GraphLite is inspired by JetBrains YouTrackDB, particularly its graph
data model, relationship traversal, indexing, storage engine,
transactions, and recovery capabilities. GraphLite will implement these
ideas incrementally; it will not attempt to reproduce the entire
production system in its first milestone.

### 2.3 Delivery Phases

**Phase 1 --- Working Core (must deliver first)**

-   C++17 project built with CMake.
-   CLI for interacting with the graph engine.
-   Create, read, update, and delete nodes.
-   Create, read, update, and delete edges.
-   Typed properties on nodes and edges.
-   Directed, labelled edges.
-   In-memory lookup of nodes and edges.
-   File-based persistence: successful mutations are saved before
    success is reported.
-   Load saved data when GraphLite starts.
-   Clear errors for invalid operations and storage failures.
-   Basic tests for core behavior and persistence.

**Phase 2 --- Efficient Graph Storage**

-   Efficient outgoing and incoming adjacency lookup.
-   A node-ID index and appropriate edge lookup structures.
-   A defined on-disk format with fixed-size pages.
-   Buffer pool with a replacement policy such as LRU.
-   Performance measurements as graph size increases.

**Phase 3 --- Database Reliability and Concurrency**

-   Transactions with begin, commit, and rollback.
-   Write-ahead logging (WAL) and crash recovery.
-   Locking for concurrent access.
-   Tests for transaction correctness and recovery behavior.

**Phase 4 --- Querying and Demonstration**

-   BFS and DFS graph traversal.
-   A small query language.
-   Secondary indexes on properties, if time permits.
-   Benchmark harness and comparison with MySQL using equivalent
    operations and datasets.
-   Optional web demo and results page.

**Stretch goals**

-   Snapshot isolation / MVCC.
-   Client-server mode.
-   Simple schema or constraints.
-   Backup and export.

### 2.4 Project-Level Out of Scope

For the planned project scope: authentication and roles, encryption at
rest, Gremlin/TinkerPop compatibility, multiple databases per instance,
and a cost-based query planner.

## 3. Data Model Decisions

### 3.1 Nodes

-   A node has a unique Node ID and a set of properties.
-   Node ID is an unsigned 64-bit integer supplied by the caller.
-   Nodes have no built-in type or label in the initial version. A type
    can be stored as an ordinary property, for example `type = "User"`.

### 3.2 Edges

-   Edges are directed and labelled. An edge has a source node ID, a
    destination node ID, a label (string, e.g. `FOLLOWS`), and
    properties.
-   Creating A -\> B does not create B -\> A. A bidirectional
    relationship requires two separate edges.
-   Multiple edges between the same pair of nodes are allowed when their
    labels differ.
-   In the initial version, an edge is identified by
    `(source ID, destination ID, label)`. There is no separate edge ID.
-   Because these values define edge identity, they are immutable. Only
    an edge's properties can be updated.

### 3.3 Properties

-   Nodes and edges both support properties: a property name plus a
    typed value.
-   Supported types: String, Integer (64-bit signed), Floating-point
    (`double`), and Boolean.
-   A property name appears at most once per node or edge.
-   Property types are preserved through save and load.

### 3.4 Edge Endpoint Validation

An edge can only be created if both its source and destination nodes
exist. GraphLite must not create dangling edges.

### 3.5 Duplicate Policy

-   Creating a node with an existing ID is rejected.
-   Creating an edge with the same `(source, destination, label)` as an
    existing edge is rejected.

### 3.6 Self-Loops

For the initial version, edges where source equals destination are
rejected. This restriction may be reconsidered if required by later use
cases.

### 3.7 Node Deletion

Deleting a node also deletes all incoming and outgoing edges. No edge
may reference a deleted node.

### 3.8 Error Codes

| Error code | Meaning |
|---|---|
| `NodeNotFound` | Requested node does not exist. |
| `EdgeNotFound` | Requested edge does not exist. |
| `PropertyNotFound` | Requested property does not exist. |
| `DuplicateNode` | Node ID already exists. |
| `DuplicateEdge` | Edge identity `(sourceId, destinationId, label)` already exists. |
| `InvalidEndpoint` | An edge endpoint does not exist. |
| `SelfLoopNotAllowed` | An edge's source and destination are identical. |
| `StorageError` | File-system read/write failure. |
| `InvalidFormat` | File header, version, or record format is unsupported. |
| `ParseError` | Malformed persisted data or syntactically invalid CLI input in Phase 1. |

A missing database file on first run is not an error. If an existing database
file fails to load, startup fails and mutations are not allowed, so the file
cannot be silently overwritten. Error messages must include context to
distinguish a file-loading parse error from invalid CLI input.

### 3.9 Edge Label and Property-Name Rules

Edge labels and property names must be non-empty and contain only ASCII
letters, digits, and underscores. The first character must be a letter or
underscore. Spaces, quotes, punctuation, and empty names are rejected.
Quoted strings remain allowed as property values.

## 4. Functional Requirements and Acceptance Criteria

### FR-1: Create Node

The system shall create a node with a unique ID and properties.

-   Creating a node with a unique ID succeeds.
-   Creating a node with an existing ID fails with `DuplicateNode`.
-   The created node is retrievable by its ID.

### FR-2: Create Edge

The system shall create a directed, labelled edge between two nodes.

-   Creation succeeds when both endpoint nodes exist.
-   Creation fails with `InvalidEndpoint` if either endpoint is missing.
-   Creation fails with `DuplicateEdge` if the same
    `(source, destination, label)` already exists.
-   Creation fails with `SelfLoopNotAllowed` if source equals
    destination.
-   The same pair of nodes may have edges with different labels.
-   No reverse edge is created automatically.
-   A failed creation leaves the graph unchanged.

### FR-3: Read Node

The system shall retrieve a node by ID.

-   Returns the node's properties with their correct types.
-   A nonexistent node returns `NodeNotFound`.

### FR-4: Read Relationships

The system shall retrieve edges connected to a node.

-   Returns matching edges with their labels.
-   Incoming and outgoing edges are distinguishable.
-   Results can be filtered by direction and optionally by label.
-   Never returns edges that do not exist.
-   A nonexistent node returns `NodeNotFound`.

### FR-5: Update Node

The system shall update a node's properties.

-   Updating stores the new value and preserves its type.
-   Updating an existing property changes that property rather than
    creating a duplicate.
-   A nonexistent node returns `NodeNotFound`.

### FR-6: Update Edge

The system shall update an edge's properties, identified by
`(source, destination, label)`.

-   Updating stores the new value and preserves its type.
-   Updating an existing property changes that property rather than
    creating a duplicate.
-   A nonexistent edge returns `EdgeNotFound`.
-   Source, destination, and label cannot be changed.

### FR-7: Delete Node

The system shall delete a node and all its connected edges.

-   The node and all incoming and outgoing edges are removed.
-   Unrelated nodes and edges remain unchanged.
-   Reading the deleted node returns `NodeNotFound`.
-   Deleting a nonexistent node returns `NodeNotFound`.

### FR-8: Delete Edge

The system shall delete an edge identified by
`(source, destination, label)`.

-   Neither endpoint node is deleted.
-   Other edges between the same nodes with different labels remain.
-   A nonexistent edge returns `EdgeNotFound`.

### FR-9: Properties

-   Nodes and edges support string, integer, floating-point, and boolean
    values.
-   Property names are unique within a node or edge.
-   Reading a property returns its original value and type.

### FR-10: Persistence

-   Every successful mutation is saved to disk before the operation
    reports success.
-   After restarting GraphLite, previously saved nodes, edges, labels,
    and properties are retrievable.
-   Property names, values, and types remain unchanged after save and
    load.
-   Deleted data stays deleted after restart.
-   If saving fails, the operation returns `StorageError` and the
    in-memory graph remains consistent with the last successfully saved
    state.
-   The initial file-based persistence is not guaranteed to recover from
    every process or machine crash. WAL and crash recovery are planned
    for a later phase.
- If the database file does not exist on first run, start with an empty graph.
- If an existing file fails to load because of I/O, format, parse, or validation errors, fail startup and reject mutations; never silently replace the file with an empty graph.

## 5. Non-Functional Requirements

-   **NFR-1 Correctness:** Edges always reference existing nodes,
    duplicates are rejected, and property types are preserved.
-   **NFR-2 Persistence:** Successfully saved operations survive a
    normal restart.
-   **NFR-3 Performance:** Measure core operation and traversal
    performance. Set targets after baseline measurements.
-   **NFR-4 Memory Management:** Avoid unnecessary duplication of graph
    data and release resources properly.
-   **NFR-5 Maintainability:** Use modular C++ components with clear
    responsibilities.
-   **NFR-6 Reliability:** Invalid operations and storage failures must
    not corrupt previously saved graph data.
-   **NFR-7 Portability:** Use C++17 and CMake.
-   **NFR-8 Scalability Evaluation:** Test increasing graph sizes and
    observe memory usage and performance.
-   **NFR-9 Concurrency:** Add and evaluate concurrent access in a later
    phase, after the single-threaded core works.

## 6. Initial Architecture

``` text
CLI
 |
GraphManager  (coordinates operations)
 |          |
Graph       StorageManager
(in-memory  (file read/write)
 graph rules)     |
                 Disk
```

Initial responsibilities:

-   **CLI:** Parses user commands and displays results.
-   **GraphManager:** Coordinates operations across the graph and
    storage components.
-   **Graph:** Owns and validates the in-memory graph data.
-   **StorageManager:** Serializes, saves, and loads graph data; it does
    not implement graph business rules.
-   **Node and Edge:** Represent graph entities and relationships.

Later components may include an Index Manager, Buffer Pool, Query
Engine, Transaction Manager, Lock Manager, API/Server, and Web
Application.

## 7. Initial Milestone

Prove that GraphLite can create, manage, and persist graph data.

``` text
Node + Edge
    -> Graph CRUD
    -> File-based Storage
    -> Restart
    -> Data Available Again
```

The initial milestone focuses on the core engine with a CLI, not a web
application.

## 8. Later Capabilities

The following are planned extensions rather than requirements for the
first working core:

-   Fixed-size page-based storage.
-   Node-ID and edge lookup indexes.
-   Buffer pool and LRU replacement.
-   BFS/DFS traversal and a small query language.
-   Transactions, WAL, crash recovery, and locking.
-   Secondary property indexes.
-   Benchmarking and MySQL comparison.
-   Optional web demo, API/server, MVCC, schema constraints, and
    backup/export.

The order may be adjusted based on available time, but each extension
should be added only after the preceding core behavior is tested.

## 9. Future Comparison with MySQL

GraphLite may be compared with MySQL using equivalent datasets and
operations, ideally modelled on an LDBC-style social network dataset.

Potential metrics include query execution time, insertion time,
retrieval time, relationship/traversal query time, memory usage, disk
I/O, throughput, performance as dataset size increases, and theoretical
time complexity.

A web-based benchmark and comparison page is optional and should only be
attempted after the core engine and measurements are working.


### FR-11 — Bounded Breadth-First Search (BFS)

The graph engine shall support BFS from a starting node with a maximum hop
depth and an optional edge-label filter. It returns each matching visited
node together with its minimum hop depth from the start node. If the start
node does not exist, the operation returns `NodeNotFound`. DFS remains a
later-phase feature.
