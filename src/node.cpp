#include <utility> // Provides std::move
#include "graphlite/node.hpp"

namespace graphlite
{

    // Initialize the node with the ID provided by the caller
    Node::Node(uint64_t id)
        : id_(id)
    {
    }

    // Return the node's unique ID without modifying it
    uint64_t Node::getId() const
    {
        return id_;
    }

    const Value *Node::getProperty(const std::string &key) const
    {
        auto it = properties_.find(key);
        if (it != properties_.end())
        {
            return &(it->second);
        }
        return nullptr;
    }

    void Node::setProperty(const std::string &key, Value value)
    {
        properties_[key] = std::move(value);
    }

    bool Node::removeProperty(const std::string &key)
    {
        return properties_.erase(key) > 0;
    }

    const std::unordered_map<std::string, Value> &
    Node::getProperties() const
    {
        return properties_;
    }

} // namespace graphlite
