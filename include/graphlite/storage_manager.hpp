
#pragma once

#include "graphlite/graph_snapshot.hpp"
#include "graphlite/result.hpp"
#include "graphlite/storage_error.hpp"

#include <filesystem>

namespace graphlite {

class StorageManager {
private:
    std::filesystem::path filePath_;

public:
    explicit StorageManager(std::filesystem::path filePath);

    Result<void, StorageError> save(
        const GraphSnapshot& snapshot
    ) const;

    Result<GraphSnapshot, StorageError> load() const;
};

} // namespace graphlite
