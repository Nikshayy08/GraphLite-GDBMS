
#include "graphlite/storage_manager.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace graphlite {
namespace {

// ============================================================
// 1. STRING SERIALIZATION
// ============================================================

// Writes a string inside quotes and escapes special characters.
// Example: hello "world" becomes "hello \"world\"".
void writeString(std::ostream& out, const std::string& value) {
    out << '"';

    for (char ch : value) {
        switch (ch) {
            case '\\': out << "\\\\"; break;
            case '"':  out << "\\\""; break;
            case '\n': out << "\\n";  break;
            case '\r': out << "\\r";  break;
            case '\t': out << "\\t";  break;
            default:   out << ch;     break;
        }
    }

    out << '"';
}

// Reads a quoted string and reverses the escaping performed above.
// Returns false for missing quotes, unknown escapes or incomplete strings.
bool readQuotedString(std::istream& in, std::string& value) {
    in >> std::ws;

    char ch;

    if (!in.get(ch) || ch != '"') {
        return false;
    }

    value.clear();

    while (in.get(ch)) {
        if (ch == '"') {
            return true;
        }

        if (ch != '\\') {
            value.push_back(ch);
            continue;
        }

        if (!in.get(ch)) {
            return false;
        }

        switch (ch) {
            case '\\': value.push_back('\\'); break;
            case '"':  value.push_back('"');  break;
            case 'n':  value.push_back('\n'); break;
            case 'r':  value.push_back('\r'); break;
            case 't':  value.push_back('\t'); break;
            default: return false;
        }
    }

    return false;
}

// Ensures a parsed record contains only whitespace after its valid content.
bool hasNoExtraContent(std::istream& in) {
    in >> std::ws;
    return in.eof();
}

// ============================================================
// 2. STRICT NODE AND EDGE ID PARSING
// ============================================================

// Parses an unsigned 64-bit decimal integer.
// Rejects negative IDs, non-numeric text, overflow and trailing characters.
bool parseUint64(const std::string& token, uint64_t& out) {
    if (token.empty()) {
        return false;
    }

    const char* first = token.data();
    const char* last = first + token.size();

    auto [ptr, error] = std::from_chars(first, last, out);

    return error == std::errc{} && ptr == last;
}

// ============================================================
// 3. VALUE SERIALIZATION
// ============================================================

// Writes a Value along with an explicit type tag.
// Non-finite doubles are rejected because they aren't valid database values.
bool writeValue(std::ostream& out, const Value& value) {
    if (const auto* v = std::get_if<bool>(&value)) {
        out << "BOOL " << (*v ? "true" : "false");
    } else if (const auto* v = std::get_if<int64_t>(&value)) {
        out << "INT " << *v;
    } else if (const auto* v = std::get_if<double>(&value)) {
        if (!std::isfinite(*v)) {
            return false;
        }

        // 17 significant digits preserve double precision during round trips.
        out << "DOUBLE " << std::setprecision(17) << *v;
    } else if (const auto* v = std::get_if<std::string>(&value)) {
        out << "STRING ";
        writeString(out, *v);
    } else {
        return false;
    }

    return static_cast<bool>(out);
}

// Reads a typed value and restores its original variant type.
bool readValue(std::istream& in, Value& value) {
    std::string type;

    if (!(in >> type)) {
        return false;
    }

    if (type == "BOOL") {
        std::string token;

        if (!(in >> token)) {
            return false;
        }

        if (token == "true") {
            value = true;
        } else if (token == "false") {
            value = false;
        } else {
            return false;
        }
    } else if (type == "INT") {
        int64_t number;

        if (!(in >> number)) {
            return false;
        }

        value = number;
    } else if (type == "DOUBLE") {
        double number;

        if (!(in >> number) || !std::isfinite(number)) {
            return false;
        }

        value = number;
    } else if (type == "STRING") {
        std::string text;

        if (!readQuotedString(in, text)) {
            return false;
        }

        value = std::move(text);
    } else {
        // Unknown type tag.
        return false;
    }

    return true;
}

// ============================================================
// 4. PROPERTY SERIALIZATION
// ============================================================

// Writes properties in sorted key order.
// This makes saved files deterministic even though properties use unordered_map.
bool writeProperties(
    std::ostream& out,
    const std::unordered_map<std::string, Value>& properties
) {
    std::vector<std::string> keys;
    keys.reserve(properties.size());

    for (const auto& entry : properties) {
        keys.push_back(entry.first);
    }

    std::sort(keys.begin(), keys.end());

    for (const auto& key : keys) {
        out << "PROP ";
        writeString(out, key);
        out << ' ';

        if (!writeValue(out, properties.at(key))) {
            return false;
        }

        out << '\n';

        if (!out) {
            return false;
        }
    }

    return true;
}

// Reads properties until the expected terminator is encountered.
// Duplicate property keys and incorrect terminators are rejected.
bool readProperties(
    std::istream& in,
    std::vector<std::pair<std::string, Value>>& properties,
    const std::string& expectedEnd
) {
    std::string line;
    std::unordered_set<std::string> keys;

    while (std::getline(in, line)) {
        // Support Windows CRLF files.
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::istringstream record(line);
        record.imbue(std::locale::classic());

        std::string keyword;

        if (!(record >> keyword)) {
            return false;
        }

        // A node must end with END_NODE; an edge with END_EDGE.
        if (keyword == "END_NODE" || keyword == "END_EDGE") {
            return keyword == expectedEnd &&
                   hasNoExtraContent(record);
        }

        if (keyword != "PROP") {
            return false;
        }

        std::string key;

        if (!readQuotedString(record, key)) {
            return false;
        }

        Value value;

        if (!readValue(record, value) ||
            !hasNoExtraContent(record)) {
            return false;
        }

        if (!keys.insert(key).second) {
            return false;
        }

        properties.emplace_back(std::move(key), std::move(value));
    }

    // EOF before END_NODE or END_EDGE means the record is incomplete.
    return false;
}

// ============================================================
// 5. SAFE FILE REPLACEMENT
// ============================================================

// Replaces the destination only after the temporary file is fully written.
// On Windows, MoveFileExW supports replacing an existing destination.
bool replaceFile(
    const std::filesystem::path& temporaryPath,
    const std::filesystem::path& destinationPath
) {
#ifdef _WIN32
    return MoveFileExW(
        temporaryPath.c_str(),
        destinationPath.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
    ) != 0;
#else
    std::error_code error;

    std::filesystem::rename(
        temporaryPath,
        destinationPath,
        error
    );

    return !error;
#endif
}

} // namespace

// ============================================================
// 6. CONSTRUCTOR
// ============================================================

StorageManager::StorageManager(std::filesystem::path filePath)
    : filePath_(std::move(filePath)) {
}

// ============================================================
// 7. SAVE SNAPSHOT
// ============================================================

Result<void, StorageError> StorageManager::save(
    const GraphSnapshot& snapshot
) const {
    // Write to a temporary file first so a failed write doesn't truncate
    // the existing database file.
    std::filesystem::path temporaryPath = filePath_;
    temporaryPath += ".tmp";

    std::ofstream out;

    // Use a locale-independent representation for numbers.
    out.imbue(std::locale::classic());

    out.open(
        temporaryPath,
        std::ios::out | std::ios::trunc
    );

    if (!out.is_open()) {
        return Result<void, StorageError>::failure(
            StorageError::FileOpenError
        );
    }

    out << "GRAPHLITE 1\n";

    bool success = static_cast<bool>(out);

    // Sort nodes by ID so identical graph states produce identical files.
    std::vector<const Node*> sortedNodes;
    sortedNodes.reserve(snapshot.nodes.size());

    for (const auto& node : snapshot.nodes) {
        sortedNodes.push_back(&node);
    }

    std::sort(
        sortedNodes.begin(),
        sortedNodes.end(),
        [](const Node* a, const Node* b) {
            return a->getId() < b->getId();
        }
    );

    for (const Node* node : sortedNodes) {
        if (!success) {
            break;
        }

        out << "NODE " << node->getId() << '\n';

        success = static_cast<bool>(out) &&
                  writeProperties(out, node->getProperties());

        if (success) {
            out << "END_NODE\n";
            success = static_cast<bool>(out);
        }
    }

    // Sort edges by source, target and label for deterministic output.
    std::vector<const Edge*> sortedEdges;
    sortedEdges.reserve(snapshot.edges.size());

    for (const auto& edge : snapshot.edges) {
        sortedEdges.push_back(&edge);
    }

    std::sort(
        sortedEdges.begin(),
        sortedEdges.end(),
        [](const Edge* a, const Edge* b) {
            if (a->getSourceId() != b->getSourceId()) {
                return a->getSourceId() < b->getSourceId();
            }

            if (a->getTargetId() != b->getTargetId()) {
                return a->getTargetId() < b->getTargetId();
            }

            return a->getLabel() < b->getLabel();
        }
    );

    for (const Edge* edge : sortedEdges) {
        if (!success) {
            break;
        }

        out << "EDGE "
            << edge->getSourceId() << ' '
            << edge->getTargetId() << ' ';

        writeString(out, edge->getLabel());
        out << '\n';

        success = static_cast<bool>(out) &&
                  writeProperties(out, edge->getProperties());

        if (success) {
            out << "END_EDGE\n";
            success = static_cast<bool>(out);
        }
    }

    // Footer lets the loader detect truncation between complete records.
    if (success) {
        out << "END_GRAPH\n";
        success = static_cast<bool>(out);
    }

    out.flush();
    success = success && static_cast<bool>(out);

    out.close();
    success = success && !out.fail();

    if (!success) {
        std::error_code ignored;
        std::filesystem::remove(temporaryPath, ignored);

        return Result<void, StorageError>::failure(
            StorageError::FileWriteError
        );
    }

    // The original file is replaced only after the temporary file is complete.
    if (!replaceFile(temporaryPath, filePath_)) {
        std::error_code ignored;
        std::filesystem::remove(temporaryPath, ignored);

        return Result<void, StorageError>::failure(
            StorageError::FileWriteError
        );
    }

    return Result<void, StorageError>::success();
}

// ============================================================
// 8. LOAD SNAPSHOT
// ============================================================

Result<GraphSnapshot, StorageError> StorageManager::load() const {
    std::ifstream in;

    in.imbue(std::locale::classic());
    in.open(filePath_);

    if (!in.is_open()) {
        std::error_code error;
        const bool exists = std::filesystem::exists(filePath_, error);

        if (!error && !exists) {
            // A missing file is normal on the first run.
            return Result<GraphSnapshot, StorageError>::success(
                GraphSnapshot{}
            );
        }

        return Result<GraphSnapshot, StorageError>::failure(
            StorageError::FileOpenError
        );
    }

    // ---------------- Read and validate header ----------------

    std::string line;

    if (!std::getline(in, line)) {
        return Result<GraphSnapshot, StorageError>::failure(
            StorageError::InvalidFormat
        );
    }

    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }

    std::istringstream header(line);
    header.imbue(std::locale::classic());

    std::string magic;
    int version = 0;

    if (!(header >> magic >> version) ||
        !hasNoExtraContent(header) ||
        magic != "GRAPHLITE") {
        return Result<GraphSnapshot, StorageError>::failure(
            StorageError::InvalidFormat
        );
    }

    if (version != 1) {
        return Result<GraphSnapshot, StorageError>::failure(
            StorageError::UnsupportedVersion
        );
    }

    // Build a local snapshot. It is returned only after complete validation.
    GraphSnapshot snapshot;

    std::unordered_set<uint64_t> nodeIds;

    using EdgeIdentity =
        std::tuple<uint64_t, uint64_t, std::string>;

    std::set<EdgeIdentity> edgeIdentities;

    bool foundFooter = false;

    // ---------------- Read graph records ----------------

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) {
            return Result<GraphSnapshot, StorageError>::failure(
                StorageError::ParseError
            );
        }

        std::istringstream record(line);
        record.imbue(std::locale::classic());

        std::string keyword;

        if (!(record >> keyword)) {
            return Result<GraphSnapshot, StorageError>::failure(
                StorageError::ParseError
            );
        }

        // ---------------- Node record ----------------

        if (keyword == "NODE") {
            uint64_t id;
            std::string idToken;

            // Parse the ID as a token to reject negative values and overflow.
            if (!(record >> idToken) ||
                !parseUint64(idToken, id) ||
                !hasNoExtraContent(record)) {
                return Result<GraphSnapshot, StorageError>::failure(
                    StorageError::ParseError
                );
            }

            if (!nodeIds.insert(id).second) {
                return Result<GraphSnapshot, StorageError>::failure(
                    StorageError::DuplicateNode
                );
            }

            std::vector<std::pair<std::string, Value>> properties;

            if (!readProperties(in, properties, "END_NODE")) {
                return Result<GraphSnapshot, StorageError>::failure(
                    StorageError::ParseError
                );
            }

            Node node(id);

            for (auto& [key, value] : properties) {
                node.setProperty(key, std::move(value));
            }

            snapshot.nodes.push_back(std::move(node));
        }

        // ---------------- Edge record ----------------

        else if (keyword == "EDGE") {
            uint64_t source;
            uint64_t target;

            std::string sourceToken;
            std::string targetToken;
            std::string label;

            // Validate both endpoint IDs using strict unsigned parsing.
            if (!(record >> sourceToken >> targetToken) ||
                !parseUint64(sourceToken, source) ||
                !parseUint64(targetToken, target) ||
                !readQuotedString(record, label) ||
                !hasNoExtraContent(record)) {
                return Result<GraphSnapshot, StorageError>::failure(
                    StorageError::ParseError
                );
            }

            if (source == target) {
                return Result<GraphSnapshot, StorageError>::failure(
                    StorageError::SelfLoop
                );
            }

            EdgeIdentity identity{source, target, label};

            if (!edgeIdentities.insert(identity).second) {
                return Result<GraphSnapshot, StorageError>::failure(
                    StorageError::DuplicateEdge
                );
            }

            std::vector<std::pair<std::string, Value>> properties;

            if (!readProperties(in, properties, "END_EDGE")) {
                return Result<GraphSnapshot, StorageError>::failure(
                    StorageError::ParseError
                );
            }

            Edge edge(source, target, label);

            for (auto& [key, value] : properties) {
                edge.setProperty(key, std::move(value));
            }

            snapshot.edges.push_back(std::move(edge));
        }

        // ---------------- End-of-graph footer ----------------

        else if (keyword == "END_GRAPH") {
            if (!hasNoExtraContent(record)) {
                return Result<GraphSnapshot, StorageError>::failure(
                    StorageError::ParseError
                );
            }

            foundFooter = true;
            break;
        }

        // Any unknown record is invalid.
        else {
            return Result<GraphSnapshot, StorageError>::failure(
                StorageError::ParseError
            );
        }
    }

    // Detect underlying file read errors.
    if (in.bad()) {
        return Result<GraphSnapshot, StorageError>::failure(
            StorageError::FileReadError
        );
    }

    // Without the footer, the file may have been truncated.
    if (!foundFooter) {
        return Result<GraphSnapshot, StorageError>::failure(
            StorageError::ParseError
        );
    }

    // Nothing is permitted after END_GRAPH.
    if (std::getline(in, line)) {
        return Result<GraphSnapshot, StorageError>::failure(
            StorageError::ParseError
        );
    }

    if (in.bad()) {
        return Result<GraphSnapshot, StorageError>::failure(
            StorageError::FileReadError
        );
    }

    // ---------------- Validate edge endpoints ----------------

    // Edges may reference nodes defined earlier or later in the file.
    // Validate all endpoints after parsing the complete snapshot.
    for (const auto& edge : snapshot.edges) {
        if (nodeIds.find(edge.getSourceId()) == nodeIds.end() ||
            nodeIds.find(edge.getTargetId()) == nodeIds.end()) {
            return Result<GraphSnapshot, StorageError>::failure(
                StorageError::MissingEndpoint
            );
        }
    }

    // Only return the snapshot after every validation step succeeds.
    return Result<GraphSnapshot, StorageError>::success(
        std::move(snapshot)
    );
}

} // namespace graphlite
