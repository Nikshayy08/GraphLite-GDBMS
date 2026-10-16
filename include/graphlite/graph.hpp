
#pragma once

#include "graphlite/graph_snapshot.hpp"
#include "graphlite/node.hpp"
#include "graphlite/edge.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace graphlite {

struct EdgeKey {
    uint64_t sourceId;
    uint64_t targetId;
    std::string label;

    bool operator==(const EdgeKey& other) const {
        return sourceId == other.sourceId &&
               targetId == other.targetId &&
               label == other.label;
    }
};

struct EdgeKeyHash {
    size_t operator()(const EdgeKey& key) const;
};

class Graph {
private:
    std::unordered_map<uint64_t, Node> nodes_;
    std::unordered_map<EdgeKey, Edge, EdgeKeyHash> edges_;

    // Track relationships in both directions.
    std::unordered_map<uint64_t, std::vector<EdgeKey>> outgoing_;
    std::unordered_map<uint64_t, std::vector<EdgeKey>> incoming_;

public:
    bool addNode(uint64_t id);

    bool addEdge(
        uint64_t sourceId,
        uint64_t targetId,
        const std::string& label
    );

    Node* getNode(uint64_t id);
    const Node* getNode(uint64_t id) const;

    Edge* getEdge(
        uint64_t sourceId,
        uint64_t targetId,
        const std::string& label
    );

    const Edge* getEdge(
        uint64_t sourceId,
        uint64_t targetId,
        const std::string& label
    ) const;

    bool removeEdge(
        uint64_t sourceId,
        uint64_t targetId,
        const std::string& label
    );

    bool removeNode(uint64_t id);

    std::vector<Edge> getOutgoingEdges(
        uint64_t nodeId,
        std::optional<std::string> label = std::nullopt
    ) const;

    std::vector<Edge> getIncomingEdges(
        uint64_t nodeId,
        std::optional<std::string> label = std::nullopt
    ) const;

    size_t nodeCount() const;
    size_t edgeCount() const;

    std::vector<uint64_t> bfs(
        uint64_t startNodeId,
        size_t maxDepth = static_cast<size_t>(-1),
        std::optional<std::string> label = std::nullopt
    ) const;

    GraphSnapshot exportSnapshot() const;
    bool importSnapshot(const GraphSnapshot& snapshot);
    
};

} // namespace graphlite
