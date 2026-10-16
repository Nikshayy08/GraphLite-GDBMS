
#pragma once

#include <cstdint>
#include <string>
#include <variant>

namespace graphlite {

using Value = std::variant<
    bool,
    int64_t,
    double,
    std::string
>;

} // namespace graphlite
