
#pragma once

#include "graphlite/node.hpp"
#include "graphlite/edge.hpp"

#include <vector>

namespace graphlite {

struct GraphSnapshot {
    std::vector<Node> nodes;
    std::vector<Edge> edges;
};

} // namespace graphlite
