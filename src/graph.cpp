#include "graphlite/graph.hpp"
#include "graphlite/name_validation.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

namespace graphlite {

// Combines the hashes of all three parts of an edge's identity.
// Edge identity = (source ID, target ID, label).
std::size_t EdgeKeyHash::operator()(const EdgeKey& key) const {
    std::size_t seed = std::hash<std::uint64_t>{}(key.sourceId);

    // Mix each additional hash into the current seed.
    const auto combine = [&seed](std::size_t valueHash) {
        seed ^= valueHash
              + static_cast<std::size_t>(0x9e3779b97f4a7c15ULL)
              + (seed << 6)
              + (seed >> 2);
    };

    combine(std::hash<std::uint64_t>{}(key.targetId));
    combine(std::hash<std::string>{}(key.label));

    return seed;
}

bool Graph::addNode(std::uint64_t id) {
    return nodes_.try_emplace(id, id).second;
}

bool Graph::addEdge(
    std::uint64_t sourceId,
    std::uint64_t targetId,
    const std::string& label
) {
    // Reject invalid labels, self-loops and edges with missing endpoints.
    if (!isValidName(label) ||
        sourceId == targetId ||
        nodes_.find(sourceId) == nodes_.end() ||
        nodes_.find(targetId) == nodes_.end()) {
        return false;
    }

    EdgeKey key{sourceId, targetId, label};

    // Reject duplicate edges with the same source, target and label.
    if (edges_.find(key) != edges_.end()) {
        return false;
    }

    auto inserted = edges_.try_emplace(
        key, sourceId, targetId, label
    );

    if (!inserted.second) {
        return false;
    }

    // Maintain both adjacency indexes.
    outgoing_[sourceId].push_back(key);
    incoming_[targetId].push_back(key);

    return true;
}

Node* Graph::getNode(std::uint64_t id) {
    auto it = nodes_.find(id);
    return it == nodes_.end() ? nullptr : &it->second;
}

const Node* Graph::getNode(std::uint64_t id) const {
    auto it = nodes_.find(id);
    return it == nodes_.end() ? nullptr : &it->second;
}

Edge* Graph::getEdge(
    std::uint64_t sourceId,
    std::uint64_t targetId,
    const std::string& label
) {
    auto it = edges_.find(EdgeKey{sourceId, targetId, label});
    return it == edges_.end() ? nullptr : &it->second;
}

const Edge* Graph::getEdge(
    std::uint64_t sourceId,
    std::uint64_t targetId,
    const std::string& label
) const {
    auto it = edges_.find(EdgeKey{sourceId, targetId, label});
    return it == edges_.end() ? nullptr : &it->second;
}

bool Graph::removeEdge(
    std::uint64_t sourceId,
    std::uint64_t targetId,
    const std::string& label
) {
    EdgeKey key{sourceId, targetId, label};

    auto it = edges_.find(key);
    if (it == edges_.end()) {
        return false;
    }

    // Remove the edge key from an adjacency list.
    const auto removeKey = [&key](std::vector<EdgeKey>& keys) {
        keys.erase(
            std::remove(keys.begin(), keys.end(), key),
            keys.end()
        );
    };

    // Remove the edge from the source's outgoing index.
    auto outIt = outgoing_.find(sourceId);
    if (outIt != outgoing_.end()) {
        removeKey(outIt->second);

        // Avoid retaining empty adjacency entries.
        if (outIt->second.empty()) {
            outgoing_.erase(outIt);
        }
    }

    // Remove the edge from the target's incoming index.
    auto inIt = incoming_.find(targetId);
    if (inIt != incoming_.end()) {
        removeKey(inIt->second);

        // Avoid retaining empty adjacency entries.
        if (inIt->second.empty()) {
            incoming_.erase(inIt);
        }
    }

    edges_.erase(it);
    return true;
}

bool Graph::removeNode(std::uint64_t id) {
    if (nodes_.find(id) == nodes_.end()) {
        return false;
    }

    // Copy connected edge keys because removeEdge modifies
    // the adjacency indexes being traversed.
    std::vector<EdgeKey> connected;

    auto outIt = outgoing_.find(id);
    if (outIt != outgoing_.end()) {
        connected.insert(
            connected.end(),
            outIt->second.begin(),
            outIt->second.end()
        );
    }

    auto inIt = incoming_.find(id);
    if (inIt != incoming_.end()) {
        connected.insert(
            connected.end(),
            inIt->second.begin(),
            inIt->second.end()
        );
    }

    // Removing an edge also updates the opposite endpoint's index.
    for (const auto& key : connected) {
        removeEdge(key.sourceId, key.targetId, key.label);
    }

    // Remove the node and any remaining adjacency entries.
    outgoing_.erase(id);
    incoming_.erase(id);
    nodes_.erase(id);

    return true;
}

std::vector<Edge> Graph::getOutgoingEdges(
    std::uint64_t nodeId,
    std::optional<std::string> label
) const {
    std::vector<Edge> result;

    auto it = outgoing_.find(nodeId);
    if (it == outgoing_.end()) {
        return result;
    }

    for (const auto& key : it->second) {
        auto edgeIt = edges_.find(key);

        if (edgeIt != edges_.end() &&
            (!label || edgeIt->second.getLabel() == *label)) {
            result.push_back(edgeIt->second);
        }
    }

    return result;
}

std::vector<Edge> Graph::getIncomingEdges(
    std::uint64_t nodeId,
    std::optional<std::string> label
) const {
    std::vector<Edge> result;

    auto it = incoming_.find(nodeId);
    if (it == incoming_.end()) {
        return result;
    }

    for (const auto& key : it->second) {
        auto edgeIt = edges_.find(key);

        if (edgeIt != edges_.end() &&
            (!label || edgeIt->second.getLabel() == *label)) {
            result.push_back(edgeIt->second);
        }
    }

    return result;
}

std::size_t Graph::nodeCount() const {
    return nodes_.size();
}

std::size_t Graph::edgeCount() const {
    return edges_.size();
}

std::vector<std::pair<std::uint64_t, std::size_t>> Graph::bfs(
    std::uint64_t startNodeId,
    std::size_t maxDepth,
    std::optional<std::string> label
) const {
    std::vector<std::pair<std::uint64_t, std::size_t>> visitedOrder;

    if (nodes_.find(startNodeId) == nodes_.end()) {
        return visitedOrder;
    }

    std::queue<std::pair<std::uint64_t, std::size_t>> pending;
    std::unordered_map<std::uint64_t, bool> visited;

    pending.push({startNodeId, 0});
    visited[startNodeId] = true;

    while (!pending.empty()) {
        auto [current, depth] = pending.front();
        pending.pop();

        visitedOrder.emplace_back(current, depth);

        // Do not explore beyond the requested depth.
        if (depth >= maxDepth) {
            continue;
        }

        auto it = outgoing_.find(current);
        if (it == outgoing_.end()) {
            continue;
        }

        for (const auto& key : it->second) {
            if (label && key.label != *label) {
                continue;
            }

            if (!visited[key.targetId]) {
                visited[key.targetId] = true;
                pending.push({key.targetId, depth + 1});
            }
        }
    }

    return visitedOrder;
}

GraphSnapshot Graph::exportSnapshot() const {
    GraphSnapshot snapshot;

    snapshot.nodes.reserve(nodes_.size());
    snapshot.edges.reserve(edges_.size());

    for (const auto& [id, node] : nodes_) {
        snapshot.nodes.push_back(node);
    }

    for (const auto& [key, edge] : edges_) {
        snapshot.edges.push_back(edge);
    }

    // Sort nodes by ID so repeated exports have the same order.
    std::sort(
        snapshot.nodes.begin(),
        snapshot.nodes.end(),
        [](const Node& a, const Node& b) {
            return a.getId() < b.getId();
        }
    );

    // Sort edges by their composite identity.
    std::sort(
        snapshot.edges.begin(),
        snapshot.edges.end(),
        [](const Edge& a, const Edge& b) {
            if (a.getSourceId() != b.getSourceId()) {
                return a.getSourceId() < b.getSourceId();
            }

            if (a.getTargetId() != b.getTargetId()) {
                return a.getTargetId() < b.getTargetId();
            }

            return a.getLabel() < b.getLabel();
        }
    );

    return snapshot;
}

bool Graph::importSnapshot(const GraphSnapshot& snapshot) {
    // Build a temporary graph so failure cannot partially replace
    // the current graph.
    Graph candidate;

    // Rebuild nodes and reject duplicate IDs or invalid property names.
    for (const auto& node : snapshot.nodes) {
        for (const auto& [propertyName, value] : node.getProperties()) {
            (void)value;
            if (!isValidName(propertyName)) {
                return false;
            }
        }

        auto result = candidate.nodes_.emplace(
            node.getId(), node
        );

        if (!result.second) {
            return false;
        }
    }

    // Rebuild edges and both adjacency indexes.
    for (const auto& edge : snapshot.edges) {
        const std::uint64_t source = edge.getSourceId();
        const std::uint64_t target = edge.getTargetId();

        // Reject invalid labels, invalid edge property names,
        // self-loops and missing endpoints.
        if (!isValidName(edge.getLabel()) ||
            source == target ||
            candidate.nodes_.find(source) == candidate.nodes_.end() ||
            candidate.nodes_.find(target) == candidate.nodes_.end()) {
            return false;
        }

        for (const auto& [propertyName, value] : edge.getProperties()) {
            (void)value;
            if (!isValidName(propertyName)) {
                return false;
            }
        }

        EdgeKey key{source, target, edge.getLabel()};

        auto result = candidate.edges_.emplace(key, edge);

        if (!result.second) {
            return false;
        }

        candidate.outgoing_[source].push_back(key);
        candidate.incoming_[target].push_back(key);
    }

    // Commit only after the entire snapshot is validated.
    nodes_.swap(candidate.nodes_);
    edges_.swap(candidate.edges_);
    outgoing_.swap(candidate.outgoing_);
    incoming_.swap(candidate.incoming_);

    return true;
}

} // namespace graphlite
