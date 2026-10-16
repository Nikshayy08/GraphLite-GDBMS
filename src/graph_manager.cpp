#include "graphlite/graph_manager.hpp"
#include "graphlite/name_validation.hpp"

#include <utility>

namespace graphlite {

GraphManager::GraphManager(std::filesystem::path filePath)
    : storage_(std::move(filePath)) {
}

Result<void, StorageError> GraphManager::persistAndCommit(Graph&& candidate) {
    auto saveResult = storage_.save(candidate.exportSnapshot());
    if (!saveResult.hasValue()) {
        return Result<void, StorageError>::failure(saveResult.error());
    }

    graph_ = std::move(candidate);
    return Result<void, StorageError>::success();
}

Result<void, StorageError> GraphManager::open() {
    auto loadResult = storage_.load();
    if (!loadResult.hasValue()) {
        ready_ = false;
        graph_ = Graph{};
        return Result<void, StorageError>::failure(loadResult.error());
    }

    Graph candidate;
    if (!candidate.importSnapshot(loadResult.value())) {
        ready_ = false;
        graph_ = Graph{};
        return Result<void, StorageError>::failure(StorageError::InvalidFormat);
    }

    graph_ = std::move(candidate);
    ready_ = true;
    return Result<void, StorageError>::success();
}

Result<void, StorageError> GraphManager::createNode(std::uint64_t id) {
    if (!ready_) {
        return Result<void, StorageError>::failure(StorageError::NotReady);
    }

    Graph candidate = graph_;
    if (!candidate.addNode(id)) {
        return Result<void, StorageError>::failure(StorageError::DuplicateNode);
    }
    return persistAndCommit(std::move(candidate));
}

Result<void, StorageError> GraphManager::deleteNode(std::uint64_t id) {
    if (!ready_) {
        return Result<void, StorageError>::failure(StorageError::NotReady);
    }

    Graph candidate = graph_;
    if (!candidate.removeNode(id)) {
        return Result<void, StorageError>::failure(StorageError::MissingEndpoint);
    }
    return persistAndCommit(std::move(candidate));
}

Result<void, StorageError> GraphManager::createEdge(
    std::uint64_t sourceId, std::uint64_t targetId, const std::string& label
) {
    if (!ready_) {
        return Result<void, StorageError>::failure(StorageError::NotReady);
    }
    if (!isValidName(label)) {
        return Result<void, StorageError>::failure(StorageError::InvalidName);
    }
    if (graph_.getNode(sourceId) == nullptr || graph_.getNode(targetId) == nullptr) {
        return Result<void, StorageError>::failure(StorageError::MissingEndpoint);
    }
    if (sourceId == targetId) {
        return Result<void, StorageError>::failure(StorageError::SelfLoop);
    }

    Graph candidate = graph_;
    if (!candidate.addEdge(sourceId, targetId, label)) {
        return Result<void, StorageError>::failure(StorageError::DuplicateEdge);
    }
    return persistAndCommit(std::move(candidate));
}

Result<void, StorageError> GraphManager::deleteEdge(
    std::uint64_t sourceId, std::uint64_t targetId, const std::string& label
) {
    if (!ready_) {
        return Result<void, StorageError>::failure(StorageError::NotReady);
    }
    if (!isValidName(label)) {
        return Result<void, StorageError>::failure(StorageError::InvalidName);
    }

    Graph candidate = graph_;
    if (!candidate.removeEdge(sourceId, targetId, label)) {
        return Result<void, StorageError>::failure(StorageError::MissingEdge);
    }
    return persistAndCommit(std::move(candidate));
}

Result<void, StorageError> GraphManager::setNodeProperty(
    std::uint64_t nodeId, const std::string& key, const Value& value
) {
    if (!ready_) {
        return Result<void, StorageError>::failure(StorageError::NotReady);
    }
    if (!isValidName(key)) {
        return Result<void, StorageError>::failure(StorageError::InvalidName);
    }

    Graph candidate = graph_;
    Node* node = candidate.getNode(nodeId);
    if (node == nullptr) {
        return Result<void, StorageError>::failure(StorageError::MissingEndpoint);
    }

    node->setProperty(key, value);
    return persistAndCommit(std::move(candidate));
}

Result<void, StorageError> GraphManager::removeNodeProperty(
    std::uint64_t nodeId, const std::string& key
) {
    if (!ready_) {
        return Result<void, StorageError>::failure(StorageError::NotReady);
    }
    if (!isValidName(key)) {
        return Result<void, StorageError>::failure(StorageError::InvalidName);
    }

    Graph candidate = graph_;
    Node* node = candidate.getNode(nodeId);
    if (node == nullptr) {
        return Result<void, StorageError>::failure(StorageError::MissingEndpoint);
    }
    if (!node->removeProperty(key)) {
        return Result<void, StorageError>::failure(StorageError::MissingProperty);
    }
    return persistAndCommit(std::move(candidate));
}

Result<void, StorageError> GraphManager::setEdgeProperty(
    std::uint64_t sourceId, std::uint64_t targetId, const std::string& label,
    const std::string& key, const Value& value
) {
    if (!ready_) {
        return Result<void, StorageError>::failure(StorageError::NotReady);
    }
    if (!isValidName(label) || !isValidName(key)) {
        return Result<void, StorageError>::failure(StorageError::InvalidName);
    }

    Graph candidate = graph_;
    Edge* edge = candidate.getEdge(sourceId, targetId, label);
    if (edge == nullptr) {
        return Result<void, StorageError>::failure(StorageError::MissingEdge);
    }

    edge->setProperty(key, value);
    return persistAndCommit(std::move(candidate));
}

Result<void, StorageError> GraphManager::removeEdgeProperty(
    std::uint64_t sourceId, std::uint64_t targetId, const std::string& label,
    const std::string& key
) {
    if (!ready_) {
        return Result<void, StorageError>::failure(StorageError::NotReady);
    }
    if (!isValidName(label) || !isValidName(key)) {
        return Result<void, StorageError>::failure(StorageError::InvalidName);
    }

    Graph candidate = graph_;
    Edge* edge = candidate.getEdge(sourceId, targetId, label);
    if (edge == nullptr) {
        return Result<void, StorageError>::failure(StorageError::MissingEdge);
    }
    if (!edge->removeProperty(key)) {
        return Result<void, StorageError>::failure(StorageError::MissingProperty);
    }
    return persistAndCommit(std::move(candidate));
}

const Graph& GraphManager::graph() const {
    return graph_;
}

bool GraphManager::isReady() const {
    return ready_;
}

} // namespace graphlite
