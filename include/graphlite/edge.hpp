
#pragma once

#include "graphlite/value.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace graphlite {

class Edge {
private:
    uint64_t sourceId_;
    uint64_t targetId_;
    std::string label_;

    // Properties belonging to this relationship.
    std::unordered_map<std::string, Value> properties_;

public:
    Edge(uint64_t sourceId, uint64_t targetId, std::string label);

    uint64_t getSourceId() const;
    uint64_t getTargetId() const;
    const std::string& getLabel() const;

    const Value* getProperty(const std::string& key) const;
    void setProperty(const std::string& key, Value value);
    bool removeProperty(const std::string& key);

    // Read-only access for snapshot serialization.
    const std::unordered_map<std::string, Value>& getProperties() const;
};

} // namespace graphlite
