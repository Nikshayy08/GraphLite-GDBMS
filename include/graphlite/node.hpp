
#pragma once

#include "graphlite/value.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace graphlite {

class Node {
private:
    uint64_t id_;

    // Maps each property name to its typed value.
    std::unordered_map<std::string, Value> properties_;

public:
    explicit Node(uint64_t id);

    uint64_t getId() const;

    // Returns nullptr if the property does not exist.
    const Value* getProperty(const std::string& key) const;

    // Adds a property or updates an existing one.
    void setProperty(const std::string& key, Value value);

    // Returns true if the property existed and was removed.
    bool removeProperty(const std::string& key);

    // Read-only access for snapshot serialization.
    const std::unordered_map<std::string, Value>& getProperties() const;
};

} // namespace graphlite
