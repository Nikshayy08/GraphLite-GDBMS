#pragma once

#include "graphlite/graph.hpp"
#include "graphlite/storage_manager.hpp"
#include "graphlite/result.hpp"
#include "graphlite/storage_error.hpp"
#include "graphlite/value.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace graphlite {

class GraphManager {
private:
    // The active graph held in memory.
    Graph graph_;

    // Handles saving and loading graph snapshots.
    StorageManager storage_;

    // Mutations are allowed only after open() succeeds.
    bool ready_ = false;

    // Persist a candidate graph before replacing the active graph.
    Result<void, StorageError> persistAndCommit(Graph&& candidate);

public:
    // Construct a manager using the database file path.
    explicit GraphManager(std::filesystem::path filePath);

    // Load the database, or initialize an empty graph on first run.
    Result<void, StorageError> open();

    // Create or delete nodes.
    Result<void, StorageError> createNode(std::uint64_t id);
    Result<void, StorageError> deleteNode(std::uint64_t id);

    // Create or delete directed, labelled edges.
    Result<void, StorageError> createEdge(
        std::uint64_t sourceId,
        std::uint64_t targetId,
        const std::string& label
    );

    Result<void, StorageError> deleteEdge(
        std::uint64_t sourceId,
        std::uint64_t targetId,
        const std::string& label
    );

    // Add or update a property on a node.
    Result<void, StorageError> setNodeProperty(
        std::uint64_t nodeId,
        const std::string& key,
        const Value& value
    );

    // Remove a property from a node.
    Result<void, StorageError> removeNodeProperty(
        std::uint64_t nodeId,
        const std::string& key
    );

    // Add or update a property on an edge.
    Result<void, StorageError> setEdgeProperty(
        std::uint64_t sourceId,
        std::uint64_t targetId,
        const std::string& label,
        const std::string& key,
        const Value& value
    );

    // Remove a property from an edge.
    Result<void, StorageError> removeEdgeProperty(
        std::uint64_t sourceId,
        std::uint64_t targetId,
        const std::string& label,
        const std::string& key
    );

    // Read-only access to the active graph.
    const Graph& graph() const;

    // Indicates whether the database was opened successfully.
    bool isReady() const;
};

} // namespace graphlite
