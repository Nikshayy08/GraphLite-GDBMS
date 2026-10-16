#include "graphlite/graph_manager.hpp"
#include "graphlite/graph_snapshot.hpp"
#include "graphlite/value.hpp"

#include <charconv>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

using namespace graphlite;

namespace {

std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string token;
    bool quoted = false;
    bool escaping = false;

    for (char ch : line) {
        if (escaping) {
            switch (ch) {
                case 'n': token.push_back('\n'); break;
                case 'r': token.push_back('\r'); break;
                case 't': token.push_back('\t'); break;
                default: token.push_back(ch); break;
            }
            escaping = false;
        } else if (quoted && ch == '\\') {
            escaping = true;
        } else if (ch == '"') {
            quoted = !quoted;
        } else if (std::isspace(static_cast<unsigned char>(ch)) && !quoted) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        } else {
            token.push_back(ch);
        }
    }

    if (escaping) token.push_back('\\');
    if (!token.empty()) tokens.push_back(token);
    if (quoted) {
        std::cerr << "Error: unmatched quote.\n";
        return {};
    }
    return tokens;
}

bool parseId(const std::string& text, std::uint64_t& value) {
    if (text.empty()) return false;
    const char* first = text.data();
    const char* last = first + text.size();
    auto result = std::from_chars(first, last, value);
    return result.ec == std::errc{} && result.ptr == last;
}

Value parseValue(const std::string& text) {
    if (text == "true") return Value{true};
    if (text == "false") return Value{false};

    std::int64_t integer = 0;
    auto intResult = std::from_chars(text.data(), text.data() + text.size(), integer);
    if (!text.empty() && intResult.ec == std::errc{} &&
        intResult.ptr == text.data() + text.size()) {
        return Value{integer};
    }

    try {
        std::size_t used = 0;
        double number = std::stod(text, &used);
        if (!text.empty() && used == text.size()) return Value{number};
    } catch (...) {
    }

    return Value{text};
}

void printValue(const Value& value) {
    std::visit([](const auto& item) {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, std::string>) {
            std::cout << '"' << item << '"';
        } else if constexpr (std::is_same_v<T, bool>) {
            std::cout << (item ? "true" : "false");
        } else {
            std::cout << item;
        }
    }, value);
}

template <typename Properties>
void printProperties(const Properties& properties) {
    for (const auto& [key, value] : properties) {
        std::cout << "  " << key << " = ";
        printValue(value);
        std::cout << '\n';
    }
}

void printHelp() {
    std::cout
        << "\nGraphLite Phase 1 CLI\n"
        << "  HELP\n"
        << "  EXIT\n"
        << "  CREATE NODE <id>\n"
        << "  GET NODE <id>\n"
        << "  DELETE NODE <id>\n"
        << "  CREATE EDGE <source> <target> <label>\n"
        << "  GET EDGE <source> <target> <label>\n"
        << "  DELETE EDGE <source> <target> <label>\n"
        << "  SET NODE <id> <property> <value>\n"
        << "  REMOVE NODE PROPERTY <id> <property>\n"
        << "  SET EDGE <source> <target> <label> <property> <value>\n"
        << "  REMOVE EDGE PROPERTY <source> <target> <label> <property>\n"
        << "  OUT <id>\n"
        << "  IN <id>\n"
        << "  BFS <start-id>\n"
        << "\nValues: true/false, integers, decimals, or strings. Quote strings containing spaces.\n"
        << "Example: SET NODE 1 name \"Alice Smith\"\n\n";
}

void printError(StorageError error, const std::string& context = {}) {
    std::string message;

    switch (error) {
        case StorageError::None:
            message = "No error.";
            break;
        case StorageError::FileOpenError:
            message = "Could not open the database file.";
            break;
        case StorageError::FileWriteError:
            message = "Could not save changes to the database file.";
            break;
        case StorageError::FileReadError:
            message = "Could not read the database file.";
            break;
        case StorageError::InvalidFormat:
            message = "The database file has an invalid format.";
            break;
        case StorageError::ParseError:
            message = "The database file contains invalid data.";
            break;
        case StorageError::DuplicateNode:
            message = "A node with this ID already exists.";
            break;
        case StorageError::DuplicateEdge:
            message = "This edge already exists.";
            break;
        case StorageError::MissingEndpoint:
            message = context == "delete-node"
                ? "The specified node does not exist."
                : "One or both endpoint nodes do not exist.";
            break;
        case StorageError::SelfLoop:
            message = "Self-loops are not allowed.";
            break;
        case StorageError::UnsupportedVersion:
            message = "This database file version is not supported.";
            break;
        case StorageError::NotReady:
            message = "The database is not ready.";
            break;
        case StorageError::MissingEdge:
            message = "The specified edge does not exist.";
            break;
        case StorageError::InvalidName:
            message = "Names must start with an ASCII letter or underscore and contain only ASCII letters, digits, or underscores.";
            break;
        case StorageError::MissingProperty:
            message = "The specified property does not exist.";
            break;
        default:
            message = "An unknown database error occurred.";
            break;
    }

    std::cerr << "Error: " << message << '\n';
}

bool requireCount(const std::vector<std::string>& t, std::size_t count) {
    if (t.size() != count) {
        std::cerr << "Invalid command. Type HELP for usage.\n";
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    const std::string databasePath = (argc > 1) ? argv[1] : "graphlite.db";
    GraphManager manager(databasePath);

    auto opened = manager.open();
    if (!opened.hasValue()) {
        printError(opened.error());
        return 1;
    }

    std::cout << "GraphLite opened: " << databasePath
              << "\nType HELP for commands.\n";

    std::string line;
    while (std::cout << "graphlite> " && std::getline(std::cin, line)) {
        auto t = tokenize(line);
        if (t.empty()) continue;

        // Normalize command keywords only; labels and property names retain case.
        for (std::size_t i = 0; i < t.size() && i < 3; ++i) {
            if (i == 0 || (i == 1) ||
                (i == 2 && (t[0] == "REMOVE"))) {
                for (char& ch : t[i])
                    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            }
        }

        if (t[0] == "EXIT" || t[0] == "QUIT") break;
        if (t[0] == "HELP") { printHelp(); continue; }

        std::uint64_t a = 0, b = 0;
        if (t[0] == "CREATE" && t.size() >= 2 && t[1] == "NODE") {
            if (!requireCount(t, 3) || !parseId(t[2], a)) {
                std::cerr << "Invalid node ID.\n"; continue;
            }
            auto r = manager.createNode(a);
            if (r.hasValue()) std::cout << "Node created: " << a << '\n';
            else printError(r.error());
        } else if (t[0] == "GET" && t.size() == 3 && t[1] == "NODE") {
            if (!parseId(t[2], a)) { std::cerr << "Invalid node ID.\n"; continue; }
            const Node* node = manager.graph().getNode(a);
            if (!node) { std::cerr << "Node not found.\n"; continue; }
            std::cout << "Node " << node->getId() << '\n';
            printProperties(node->getProperties());
        } else if (t[0] == "DELETE" && t.size() == 3 && t[1] == "NODE") {
            if (!parseId(t[2], a)) { std::cerr << "Invalid node ID.\n"; continue; }
            auto r = manager.deleteNode(a);
            if (r.hasValue()) std::cout << "Node deleted: " << a << '\n';
            else printError(r.error(), "delete-node");
        } else if (t[0] == "CREATE" && t.size() == 5 && t[1] == "EDGE") {
            if (!parseId(t[2], a) || !parseId(t[3], b)) {
                std::cerr << "Invalid endpoint ID.\n"; continue;
            }
            auto r = manager.createEdge(a, b, t[4]);
            if (r.hasValue()) std::cout << "Edge created.\n";
            else printError(r.error());
        } else if (t[0] == "GET" && t.size() == 5 && t[1] == "EDGE") {
            if (!parseId(t[2], a) || !parseId(t[3], b)) {
                std::cerr << "Invalid endpoint ID.\n"; continue;
            }
            bool found = false;
            for (const auto& edge : manager.graph().exportSnapshot().edges) {
                if (edge.getSourceId() == a && edge.getTargetId() == b &&
                    edge.getLabel() == t[4]) {
                    std::cout << "Edge " << a << " -> " << b
                              << " [" << t[4] << "]\n";
                    printProperties(edge.getProperties());
                    found = true;
                    break;
                }
            }
            if (!found) std::cerr << "Edge not found.\n";
        } else if (t[0] == "DELETE" && t.size() == 5 && t[1] == "EDGE") {
            if (!parseId(t[2], a) || !parseId(t[3], b)) {
                std::cerr << "Invalid endpoint ID.\n"; continue;
            }
            auto r = manager.deleteEdge(a, b, t[4]);
            if (r.hasValue()) std::cout << "Edge deleted.\n";
            else printError(r.error());
        } else if (t[0] == "SET" && t.size() == 5 && t[1] == "NODE") {
            if (!parseId(t[2], a)) { std::cerr << "Invalid node ID.\n"; continue; }
            auto r = manager.setNodeProperty(a, t[3], parseValue(t[4]));
            if (r.hasValue()) std::cout << "Property saved.\n";
            else printError(r.error());
        } else if (t[0] == "REMOVE" && t.size() == 5 &&
                   t[1] == "NODE" && t[2] == "PROPERTY") {
            if (!parseId(t[3], a)) { std::cerr << "Invalid node ID.\n"; continue; }
            auto r = manager.removeNodeProperty(a, t[4]);
            if (r.hasValue()) std::cout << "Property removed.\n";
            else printError(r.error());
        } else if (t[0] == "SET" && t.size() == 7 && t[1] == "EDGE") {
            if (!parseId(t[2], a) || !parseId(t[3], b)) {
                std::cerr << "Invalid endpoint ID.\n"; continue;
            }
            auto r = manager.setEdgeProperty(a, b, t[4], t[5], parseValue(t[6]));
            if (r.hasValue()) std::cout << "Property saved.\n";
            else printError(r.error());
        } else if (t[0] == "REMOVE" && t.size() == 7 &&
                   t[1] == "EDGE" && t[2] == "PROPERTY") {
            // REMOVE EDGE PROPERTY <source> <target> <label> <property>
            if (!parseId(t[3], a) || !parseId(t[4], b)) {
                std::cerr << "Invalid endpoint ID.\n"; continue;
            }
            auto r = manager.removeEdgeProperty(a, b, t[5], t[6]);
            if (r.hasValue()) std::cout << "Property removed.\n";
            else printError(r.error());
        } else if ((t[0] == "OUT" || t[0] == "IN") && t.size() == 2) {
            if (!parseId(t[1], a)) { std::cerr << "Invalid node ID.\n"; continue; }
            const auto snapshot = manager.graph().exportSnapshot();
            bool foundNode = false;
            for (const auto& node : snapshot.nodes)
                if (node.getId() == a) foundNode = true;
            if (!foundNode) { std::cerr << "Node not found.\n"; continue; }
            for (const auto& edge : snapshot.edges) {
                if ((t[0] == "OUT" && edge.getSourceId() == a) ||
                    (t[0] == "IN" && edge.getTargetId() == a)) {
                    std::cout << edge.getSourceId() << " -> "
                              << edge.getTargetId() << " [" << edge.getLabel() << "]\n";
                }
            }
        } else if (t[0] == "BFS" && t.size() == 2) {
            if (!parseId(t[1], a)) { std::cerr << "Invalid node ID.\n"; continue; }
            const auto visits = manager.graph().bfs(a);
            if (visits.empty() && manager.graph().getNode(a) == nullptr) {
                std::cerr << "Start node not found.\n"; continue;
            }
            std::cout << "BFS (node:depth):";
            for (const auto& [id, depth] : visits) {
                std::cout << ' ' << id << ':' << depth;
            }
            std::cout << '\n';
        } else {
            std::cerr << "Invalid command. Type HELP for usage.\n";
        }
    }

    std::cout << "GraphLite closed.\n";
    return 0;
}
