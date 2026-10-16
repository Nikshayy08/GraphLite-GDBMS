# GraphLite Implementation Plan

## Milestones and rough schedule

Estimates are relative to the project start; keep the scope small and reassess at each milestone.

| Milestone | Target | Learn first | Scope | Done means |
|---|---:|---|---|---|
| **M0 — Project setup** | Week 1 | CMake basics, GoogleTest setup | Configure/build the C++17 project; wire in GoogleTest; add one dummy test. | Build and test pass with one documented command; README and `.gitignore` are present. |
| **M1 — Data model** | Week 1 | `std::variant`, implementing a simple `Result<T>` including `Result<void>` | Define `NodeId`, `PropertyValue`, `Node`, `Edge`, `EdgeKey`, `Result`, and `Command` types. | Types compile; focused unit tests cover value/result behavior; `CommandParser` can develop against `Command` without depending on `Graph`. |
| **M2 — In-memory graph** | Weeks 2–3 | `unordered_map` with a custom hash (`EdgeKey`), BFS with a visited set | Implement graph CRUD, validation, adjacency structures, bounded BFS, and graph invariants. | CRUD, invalid operations, duplicate identity, cascade deletion, and BFS depth/filter tests pass. |
| **M3a — StorageManager** | Weeks 3–4 | File I/O, `std::filesystem` (temporary file + rename), string escaping and parsing | Define the storage interface early. Implement versioned snapshot format, save/load, format and semantic validation, and corrupt-file errors. Keep storage independent of `GraphManager`. | Round-trip, malformed/truncated file, invalid record, and filesystem-error tests pass. Save uses a temporary file and rename where supported. Tests can inject write failures through a small `IStorage`-style interface and a failing fake; do not rely only on permissions or invalid paths. |
| **M3b — GraphManager integration** | Week 4 | C++ copy semantics (`Graph` must be copyable), move vs. copy | Wire `Graph` and `StorageManager` together. Implement copy-and-swap mutation: mutate a working copy, persist its snapshot, and activate it only after a successful save. Define first-run and failed-load startup behavior. | Missing database file starts empty; an existing corrupt/unreadable file fails startup and is never silently overwritten; injected save failure leaves the active graph unchanged. |
| **M4 — CLI integration** | Week 5 | Tokenizing and parsing command strings | Implement `CommandParser` and connect parsed commands to `GraphManager`. `CommandParser` depends on `Command`, not `Graph`. | Parser tests cover valid, malformed, quoted-string, and invalid-identifier inputs; CLI smoke tests exercise core operations. |
| **M5 — Phase 1 acceptance** | Week 6 | Test fixtures, fault injection | Run the end-to-end persistence, failure, and restart suite; fix real gaps and update documentation. | All tests pass. **Alice/Bob restart test:** Alice creates and persists nodes and an edge; the process exits; Bob starts a fresh process, loads the same database, and verifies the nodes, edge, labels, and properties. Corrupt-file startup and injected write-failure tests also pass. |

## Cross-milestone working rules

- **Test each layer independently** before integrating it with the next layer.
- **Agree on the `StorageManager` interface in M3a** so other contributors can plan against it.
- `CommandParser` only needs the `Command` type. It can be implemented and tested in parallel after M1 defines that type, while M2–M3 work proceeds.
- Keep milestone scope bounded. If a milestone exceeds its estimate, record the blocker and cut optional work before expanding scope.

## Milestone completion record

For each milestone, require all three:
1. Build and automated tests pass.
2. Relevant docs are updated.
3. A Git commit is created; add a milestone tag when the milestone establishes a meaningful checkpoint.

Add a short note to the milestone's completion record (or commit notes) with exactly these prompts:
- **Assumed:** What did I assume?
- **Broke:** What failed or surprised me?
- **Design change:** What, if anything, did I change in `design.md`?

Do not rewrite the design just to document ordinary implementation details; update it when implementation reveals a genuine design gap.

## Planned folder layout

```text
GraphLite/
├── CMakeLists.txt
├── .gitignore
├── include/graphlite/
├── src/
├── tests/
├── docs/
│   ├── design.md
│   ├── plan.md
│   ├── graphlite-uml.mmd
│   └── assets/
└── README.md
```
