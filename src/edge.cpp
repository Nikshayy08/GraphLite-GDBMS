
#include "graphlite/edge.hpp"

#include <utility>

namespace graphlite {

Edge::Edge(uint64_t sourceId, uint64_t targetId, std::string label)
    : sourceId_(sourceId),
      targetId_(targetId),
      label_(std::move(label)) {
}

uint64_t Edge::getSourceId() const {
    return sourceId_;
}

uint64_t Edge::getTargetId() const {
    return targetId_;
}

const std::string& Edge::getLabel() const {
    return label_;
}

const Value* Edge::getProperty(const std::string& key) const {
    auto it = properties_.find(key);

    if (it == properties_.end()) {
        return nullptr;
    }

    return &it->second;
}

void Edge::setProperty(const std::string& key, Value value) {
    properties_[key] = std::move(value);
}

bool Edge::removeProperty(const std::string& key) {
    return properties_.erase(key) > 0;
}

const std::unordered_map<std::string, Value>&
Edge::getProperties() const {
    return properties_;
}

} // namespace graphlite
