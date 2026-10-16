
#include <gtest/gtest.h>

#include "graphlite/node.hpp"
#include "graphlite/edge.hpp"
#include "graphlite/graph.hpp"
#include "graphlite/graph_snapshot.hpp"
#include "graphlite/storage_manager.hpp"
#include "graphlite/graph_manager.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <ios>
#include <variant>
#include <system_error>
#include <utility>

using namespace graphlite;

// ---------------- Node Tests ----------------

TEST(NodeTest, CreatesNodeWithCorrectId) {
    Node node(1);
    EXPECT_EQ(node.getId(), 1);
}

TEST(NodeTest, AddsAndReadsProperty) {
    Node node(1);
    node.setProperty("name", std::string("Alice"));

    const Value* value = node.getProperty("name");
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(std::get<std::string>(*value), "Alice");
}

TEST(NodeTest, UpdatesExistingProperty) {
    Node node(1);
    node.setProperty("age", int64_t(20));
    node.setProperty("age", int64_t(21));

    EXPECT_EQ(std::get<int64_t>(*node.getProperty("age")), 21);
}

TEST(NodeTest, RemovesProperty) {
    Node node(1);
    node.setProperty("age", int64_t(20));

    EXPECT_TRUE(node.removeProperty("age"));
    EXPECT_EQ(node.getProperty("age"), nullptr);
}

TEST(NodeTest, HandlesMissingProperty) {
    Node node(1);

    EXPECT_EQ(node.getProperty("missing"), nullptr);
    EXPECT_FALSE(node.removeProperty("missing"));
}

// ---------------- Edge Tests ----------------

TEST(EdgeTest, CreatesEdgeWithCorrectFields) {
    Edge edge(1, 2, "FOLLOWS");

    EXPECT_EQ(edge.getSourceId(), 1);
    EXPECT_EQ(edge.getTargetId(), 2);
    EXPECT_EQ(edge.getLabel(), "FOLLOWS");
}

TEST(EdgeTest, AddsAndReadsProperty) {
    Edge edge(1, 2, "FOLLOWS");
    edge.setProperty("weight", 2.5);

    const Value* value = edge.getProperty("weight");
    ASSERT_NE(value, nullptr);
    EXPECT_DOUBLE_EQ(std::get<double>(*value), 2.5);
}

TEST(EdgeTest, UpdatesExistingProperty) {
    Edge edge(1, 2, "FOLLOWS");
    edge.setProperty("weight", 1.0);
    edge.setProperty("weight", 3.0);

    const Value* value = edge.getProperty("weight");
    ASSERT_NE(value, nullptr);
    EXPECT_DOUBLE_EQ(std::get<double>(*value), 3.0);
}

TEST(EdgeTest, RemovesProperty) {
    Edge edge(1, 2, "FOLLOWS");
    edge.setProperty("weight", 2.5);

    EXPECT_TRUE(edge.removeProperty("weight"));
    EXPECT_EQ(edge.getProperty("weight"), nullptr);
}

TEST(EdgeTest, HandlesMissingProperty) {
    Edge edge(1, 2, "FOLLOWS");

    EXPECT_EQ(edge.getProperty("weight"), nullptr);
    EXPECT_FALSE(edge.removeProperty("weight"));
}

// ---------------- Graph Tests ----------------

TEST(GraphTest, AddsAndRetrievesNode) {
    Graph graph;

    ASSERT_TRUE(graph.addNode(1));
    ASSERT_NE(graph.getNode(1), nullptr);
    EXPECT_EQ(graph.getNode(1)->getId(), 1);
}

TEST(GraphTest, RejectsDuplicateNode) {
    Graph graph;

    EXPECT_TRUE(graph.addNode(1));
    EXPECT_FALSE(graph.addNode(1));
    EXPECT_EQ(graph.nodeCount(), 1);
}

TEST(GraphTest, AddsAndRetrievesEdge) {
    Graph graph;
    graph.addNode(1);
    graph.addNode(2);

    ASSERT_TRUE(graph.addEdge(1, 2, "FOLLOWS"));

    const Edge* edge = graph.getEdge(1, 2, "FOLLOWS");
    ASSERT_NE(edge, nullptr);

    EXPECT_EQ(edge->getSourceId(), 1);
    EXPECT_EQ(edge->getTargetId(), 2);
    EXPECT_EQ(edge->getLabel(), "FOLLOWS");
}

TEST(GraphTest, AllowsDifferentLabelsBetweenSameNodes) {
    Graph graph;
    graph.addNode(1);
    graph.addNode(2);

    EXPECT_TRUE(graph.addEdge(1, 2, "FOLLOWS"));
    EXPECT_TRUE(graph.addEdge(1, 2, "FRIENDS"));
    EXPECT_EQ(graph.edgeCount(), 2);
}

TEST(GraphTest, RejectsDuplicateEdgeIdentity) {
    Graph graph;
    graph.addNode(1);
    graph.addNode(2);

    EXPECT_TRUE(graph.addEdge(1, 2, "FOLLOWS"));
    EXPECT_FALSE(graph.addEdge(1, 2, "FOLLOWS"));
    EXPECT_EQ(graph.edgeCount(), 1);
}

TEST(GraphTest, RejectsEdgeWithMissingNode) {
    Graph graph;
    graph.addNode(1);

    EXPECT_FALSE(graph.addEdge(1, 99, "FOLLOWS"));
    EXPECT_EQ(graph.edgeCount(), 0);
}

TEST(GraphTest, RejectsSelfLoop) {
    Graph graph;
    graph.addNode(1);

    EXPECT_FALSE(graph.addEdge(1, 1, "FOLLOWS"));
    EXPECT_EQ(graph.edgeCount(), 0);
}

TEST(GraphTest, RemovesEdge) {
    Graph graph;
    graph.addNode(1);
    graph.addNode(2);
    graph.addEdge(1, 2, "FOLLOWS");

    EXPECT_TRUE(graph.removeEdge(1, 2, "FOLLOWS"));
    EXPECT_EQ(graph.getEdge(1, 2, "FOLLOWS"), nullptr);
    EXPECT_EQ(graph.edgeCount(), 0);
}

TEST(GraphTest, RetrievesOutgoingEdges) {
    Graph graph;
    graph.addNode(1);
    graph.addNode(2);
    graph.addNode(3);

    graph.addEdge(1, 2, "FOLLOWS");
    graph.addEdge(1, 3, "FRIENDS");

    EXPECT_EQ(graph.getOutgoingEdges(1).size(), 2);
    EXPECT_EQ(graph.getOutgoingEdges(1, "FOLLOWS").size(), 1);
}

TEST(GraphTest, RetrievesIncomingEdges) {
    Graph graph;
    graph.addNode(1);
    graph.addNode(2);
    graph.addNode(3);

    graph.addEdge(1, 3, "FOLLOWS");
    graph.addEdge(2, 3, "FRIENDS");

    EXPECT_EQ(graph.getIncomingEdges(3).size(), 2);
    EXPECT_EQ(graph.getIncomingEdges(3, "FRIENDS").size(), 1);
}

TEST(GraphTest, RemovingNodeRemovesConnectedEdges) {
    Graph graph;

    graph.addNode(1);
    graph.addNode(2);
    graph.addNode(3);

    graph.addEdge(1, 2, "A");
    graph.addEdge(3, 1, "B");
    graph.addEdge(2, 3, "C");

    EXPECT_TRUE(graph.removeNode(1));
    EXPECT_EQ(graph.getNode(1), nullptr);
    EXPECT_EQ(graph.edgeCount(), 1);
    EXPECT_NE(graph.getEdge(2, 3, "C"), nullptr);
}

// ---------------- BFS Tests ----------------

TEST(BfsTest, VisitsNodesInBreadthFirstOrder) {
    Graph graph;

    for (uint64_t id = 1; id <= 4; ++id) {
        graph.addNode(id);
    }

    graph.addEdge(1, 2, "LINK");
    graph.addEdge(1, 3, "LINK");
    graph.addEdge(2, 4, "LINK");
    graph.addEdge(3, 4, "LINK");

    EXPECT_EQ(graph.bfs(1),
              (std::vector<std::pair<uint64_t, size_t>>{{1, 0}, {2, 1}, {3, 1}, {4, 2}}));
}

TEST(BfsTest, HandlesCyclesWithoutRepeatingNodes) {
    Graph graph;

    for (uint64_t id = 1; id <= 3; ++id) {
        graph.addNode(id);
    }

    graph.addEdge(1, 2, "LINK");
    graph.addEdge(2, 3, "LINK");
    graph.addEdge(3, 1, "LINK");

    EXPECT_EQ(graph.bfs(1),
              (std::vector<std::pair<uint64_t, size_t>>{{1, 0}, {2, 1}, {3, 2}}));
}

TEST(BfsTest, ReturnsEmptyForMissingStartNode) {
    Graph graph;

    EXPECT_TRUE(graph.bfs(99).empty());
}

TEST(BfsTest, VisitsOnlyReachableNodes) {
    Graph graph;

    for (uint64_t id = 1; id <= 4; ++id) {
        graph.addNode(id);
    }

    graph.addEdge(1, 2, "LINK");
    graph.addEdge(3, 4, "LINK");

    EXPECT_EQ(graph.bfs(1),
              (std::vector<std::pair<uint64_t, size_t>>{{1, 0}, {2, 1}}));
}

TEST(BfsTest, RespectsMaximumDepth) {
    Graph graph;

    for (uint64_t id = 1; id <= 3; ++id) {
        graph.addNode(id);
    }

    graph.addEdge(1, 2, "LINK");
    graph.addEdge(2, 3, "LINK");

    EXPECT_EQ(graph.bfs(1, 1),
              (std::vector<std::pair<uint64_t, size_t>>{{1, 0}, {2, 1}}));
}

TEST(BfsTest, FiltersEdgesByLabel) {
    Graph graph;

    for (uint64_t id = 1; id <= 3; ++id) {
        graph.addNode(id);
    }

    graph.addEdge(1, 2, "FOLLOWS");
    graph.addEdge(1, 3, "FRIENDS");

    EXPECT_EQ(
        graph.bfs(1, static_cast<size_t>(-1), "FOLLOWS"),
        (std::vector<std::pair<uint64_t, size_t>>{{1, 0}, {2, 1}})
    );
}


TEST(BfsTest, ReportsMinimumHopDepthWhenMultipleRoutesExist) {
    Graph graph;

    for (uint64_t id = 1; id <= 5; ++id) {
        graph.addNode(id);
    }

    graph.addEdge(1, 2, "LINK");
    graph.addEdge(1, 3, "LINK");
    graph.addEdge(2, 4, "LINK");
    graph.addEdge(4, 5, "LINK");
    graph.addEdge(3, 5, "LINK");

    EXPECT_EQ(
        graph.bfs(1),
        (std::vector<std::pair<uint64_t, size_t>>{
            {1, 0}, {2, 1}, {3, 1}, {4, 2}, {5, 2}
        })
    );
}

TEST(BfsTest, DepthZeroReturnsOnlyTheStartNodeAtDepthZero) {
    Graph graph;
    graph.addNode(7);
    graph.addNode(8);
    graph.addEdge(7, 8, "LINK");

    EXPECT_EQ(
        graph.bfs(7, 0),
        (std::vector<std::pair<uint64_t, size_t>>{{7, 0}})
    );
}
// ---------------- Graph Snapshot Tests ----------------

TEST(GraphSnapshotTest, ExportPreservesNodesEdgesAndProperties) {
    Graph graph;
    graph.addNode(1);
    graph.addNode(2);

    graph.getNode(1)->setProperty("name", std::string("Alice"));
    graph.addEdge(1, 2, "FOLLOWS");
    graph.getEdge(1, 2, "FOLLOWS")
        ->setProperty("weight", 2.5);

    GraphSnapshot snapshot = graph.exportSnapshot();

    ASSERT_EQ(snapshot.nodes.size(), 2);
    ASSERT_EQ(snapshot.edges.size(), 1);

    // Find node by ID; snapshot order is unspecified.
    const Node* savedNode = nullptr;

    for (const auto& node : snapshot.nodes) {
        if (node.getId() == 1) {
            savedNode = &node;
            break;
        }
    }

    ASSERT_NE(savedNode, nullptr);

    const Value* name = savedNode->getProperty("name");
    ASSERT_NE(name, nullptr);
    EXPECT_EQ(std::get<std::string>(*name), "Alice");

    // Find edge by its identity, not its position.
    const Edge* savedEdge = nullptr;

    for (const auto& edge : snapshot.edges) {
        if (edge.getSourceId() == 1 &&
            edge.getTargetId() == 2 &&
            edge.getLabel() == "FOLLOWS") {
            savedEdge = &edge;
            break;
        }
    }

    ASSERT_NE(savedEdge, nullptr);

    const Value* weight = savedEdge->getProperty("weight");
    ASSERT_NE(weight, nullptr);
    EXPECT_DOUBLE_EQ(std::get<double>(*weight), 2.5);
}

TEST(GraphSnapshotTest, ImportRebuildsGraphAndAdjacency) {
    Graph original;
    original.addNode(1);
    original.addNode(2);
    original.addEdge(1, 2, "FOLLOWS");

    GraphSnapshot snapshot = original.exportSnapshot();

    Graph restored;
    ASSERT_TRUE(restored.importSnapshot(snapshot));

    EXPECT_EQ(restored.nodeCount(), 2);
    EXPECT_EQ(restored.edgeCount(), 1);
    EXPECT_NE(restored.getEdge(1, 2, "FOLLOWS"), nullptr);
    EXPECT_EQ(restored.getOutgoingEdges(1).size(), 1);
    EXPECT_EQ(restored.getIncomingEdges(2).size(), 1);
}

TEST(GraphSnapshotTest, RejectsInvalidSnapshotWithoutChangingGraph) {
    Graph graph;
    graph.addNode(99);

    GraphSnapshot invalid;
    invalid.nodes.emplace_back(1);
    invalid.edges.emplace_back(1, 2, "FOLLOWS");

    EXPECT_FALSE(graph.importSnapshot(invalid));

    EXPECT_EQ(graph.nodeCount(), 1);
    EXPECT_NE(graph.getNode(99), nullptr);
    EXPECT_EQ(graph.edgeCount(), 0);
}



// ---------------- Storage Manager Test Fixture ----------------

// Each storage test gets its own database file. SetUp and TearDown
// remove both the database file and its temporary replacement file.
class StorageManagerTest : public ::testing::Test {
protected:
    std::filesystem::path path_;

    void SetUp() override {
        const auto* info =
            ::testing::UnitTest::GetInstance()->current_test_info();

        path_ = std::filesystem::temp_directory_path() /
            (std::string("graphlite_") +
             info->test_suite_name() + "_" +
             info->name() + ".db");

        cleanStorageTestFiles();
    }

    void TearDown() override {
        cleanStorageTestFiles();
    }

    void cleanStorageTestFiles() {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);

        auto temporaryPath = path_;
        temporaryPath += ".tmp";

        ignored.clear();
        std::filesystem::remove(temporaryPath, ignored);
    }

    // Writes invalid database text and checks for the expected ParseError.
    bool rejectsInvalidStorageText(const std::string& text) {
        cleanStorageTestFiles();

        {
            std::ofstream file(path_);
            if (!file.is_open()) {
                return false;
            }

            file << "GRAPHLITE 1\n";
            file << text;
            if (!file.good()) {
                return false;
            }
        }

        graphlite::StorageManager storage(path_);
        const auto result = storage.load();
        return !result &&
               result.error() == graphlite::StorageError::ParseError;
    }
};

TEST_F(StorageManagerTest, RoundTripsGraphAndTypedProperties) {
    cleanStorageTestFiles();

    graphlite::StorageManager storage(path_);

    graphlite::GraphSnapshot original;

    graphlite::Node alice(1);
    alice.setProperty("name", std::string("Alice"));
    alice.setProperty("age", int64_t{21});
    alice.setProperty("active", true);
    alice.setProperty("score", 9.5);
    alice.setProperty("message", std::string("hello\n\"GraphLite\""));

    graphlite::Node bob(2);
    bob.setProperty("name", std::string("Bob"));

    graphlite::Edge follows(1, 2, "FOLLOWS");
    follows.setProperty("weight", 2.5);
    follows.setProperty("verified", true);

    original.nodes.push_back(alice);
    original.nodes.push_back(bob);
    original.edges.push_back(follows);

    auto saveResult = storage.save(original);
    ASSERT_TRUE(saveResult.hasValue());

    auto loadResult = storage.load();
    ASSERT_TRUE(loadResult.hasValue());

    const auto& loaded = loadResult.value();

    ASSERT_EQ(loaded.nodes.size(), 2);
    ASSERT_EQ(loaded.edges.size(), 1);

    const graphlite::Node* loadedAlice = nullptr;

    for (const auto& node : loaded.nodes) {
        if (node.getId() == 1) {
            loadedAlice = &node;
            break;
        }
    }

    ASSERT_NE(loadedAlice, nullptr);

    const auto* name = loadedAlice->getProperty("name");
    ASSERT_NE(name, nullptr);
    ASSERT_TRUE(std::holds_alternative<std::string>(*name));
    EXPECT_EQ(std::get<std::string>(*name), "Alice");

    const auto* age = loadedAlice->getProperty("age");
    ASSERT_NE(age, nullptr);
    ASSERT_TRUE(std::holds_alternative<int64_t>(*age));
    EXPECT_EQ(std::get<int64_t>(*age), 21);

    const auto* active = loadedAlice->getProperty("active");
    ASSERT_NE(active, nullptr);
    ASSERT_TRUE(std::holds_alternative<bool>(*active));
    EXPECT_TRUE(std::get<bool>(*active));

    const auto* score = loadedAlice->getProperty("score");
    ASSERT_NE(score, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(*score));
    EXPECT_DOUBLE_EQ(std::get<double>(*score), 9.5);

    const auto* message = loadedAlice->getProperty("message");
    ASSERT_NE(message, nullptr);
    EXPECT_EQ(
        std::get<std::string>(*message),
        "hello\n\"GraphLite\""
    );

    const auto& loadedEdge = loaded.edges.front();

    EXPECT_EQ(loadedEdge.getSourceId(), 1);
    EXPECT_EQ(loadedEdge.getTargetId(), 2);
    EXPECT_EQ(loadedEdge.getLabel(), "FOLLOWS");

    const auto* weight = loadedEdge.getProperty("weight");
    ASSERT_NE(weight, nullptr);
    EXPECT_DOUBLE_EQ(std::get<double>(*weight), 2.5);

    const auto* verified = loadedEdge.getProperty("verified");
    ASSERT_NE(verified, nullptr);
    EXPECT_TRUE(std::get<bool>(*verified));

    cleanStorageTestFiles();
}

TEST_F(StorageManagerTest, MissingFileLoadsEmptySnapshot) {
    cleanStorageTestFiles();

    graphlite::StorageManager storage(path_);

    auto result = storage.load();

    ASSERT_TRUE(result.hasValue());
    EXPECT_TRUE(result.value().nodes.empty());
    EXPECT_TRUE(result.value().edges.empty());

    cleanStorageTestFiles();
}

TEST_F(StorageManagerTest, RejectsMalformedFile) {
    cleanStorageTestFiles();

    {
        std::ofstream out(path_);

        ASSERT_TRUE(out.is_open());

        // The NODE record is incomplete: END_NODE is missing.
        out << "GRAPHLITE 1\n";
        out << "NODE 1\n";
        out << "PROP \"name\" STRING \"Alice\"\n";
    }

    graphlite::StorageManager storage(path_);

    auto result = storage.load();

    EXPECT_FALSE(result.hasValue());
    EXPECT_EQ(result.error(), graphlite::StorageError::ParseError);

    cleanStorageTestFiles();
}

TEST_F(StorageManagerTest, ReplacesExistingDatabaseOnSave) {
    cleanStorageTestFiles();

    graphlite::StorageManager storage(path_);

    graphlite::GraphSnapshot first;
    first.nodes.emplace_back(1);

    auto firstSave = storage.save(first);
    ASSERT_TRUE(firstSave.hasValue());

    graphlite::GraphSnapshot second;
    second.nodes.emplace_back(2);

    auto secondSave = storage.save(second);
    ASSERT_TRUE(secondSave.hasValue());

    auto loaded = storage.load();

    ASSERT_TRUE(loaded.hasValue());
    ASSERT_EQ(loaded.value().nodes.size(), 1);
    EXPECT_EQ(loaded.value().nodes.front().getId(), 2);

    cleanStorageTestFiles();
}

// Negative node IDs must be rejected.
TEST_F(StorageManagerTest, RejectsNegativeNodeId) {
    EXPECT_TRUE(rejectsInvalidStorageText(
        "NODE -1\nEND_NODE\nEND_GRAPH\n"
    ));
}

// Non-numeric node IDs must be rejected.
TEST_F(StorageManagerTest, RejectsNonNumericNodeId) {
    EXPECT_TRUE(rejectsInvalidStorageText(
        "NODE abc\nEND_NODE\nEND_GRAPH\n"
    ));
}

// IDs exceeding uint64_t's maximum must be rejected.
TEST_F(StorageManagerTest, RejectsOverflowingNodeId) {
    EXPECT_TRUE(rejectsInvalidStorageText(
        "NODE 18446744073709551616\nEND_NODE\nEND_GRAPH\n"
    ));
}

// Unexpected tokens after an ID must be rejected.
TEST_F(StorageManagerTest, RejectsExtraTokensAfterNodeId) {
    EXPECT_TRUE(rejectsInvalidStorageText(
        "NODE 1 unexpected\nEND_NODE\nEND_GRAPH\n"
    ));
}

// Negative edge endpoints must also be rejected.
TEST_F(StorageManagerTest, RejectsNegativeEdgeEndpoint) {
    EXPECT_TRUE(rejectsInvalidStorageText(
        "NODE 1\nEND_NODE\n"
        "NODE 2\nEND_NODE\n"
        "EDGE -1 2 \"LINK\"\nEND_EDGE\n"
        "END_GRAPH\n"
    ));
}


// ------------------------------------------------------------
// GraphManager Test Fixture
// ------------------------------------------------------------

class GraphManagerTest : public ::testing::Test {
protected:
    std::filesystem::path path_;
    std::filesystem::path backupPath_;

    void SetUp() override {
        const auto* info =
            ::testing::UnitTest::GetInstance()->current_test_info();

        path_ = std::filesystem::temp_directory_path() /
            (std::string("graphlite_manager_") +
             info->test_suite_name() + "_" +
             info->name() + ".db");

        backupPath_ = path_;
        backupPath_ += ".backup";

        cleanFiles();
    }

    void TearDown() override {
        cleanFiles();
    }

    void cleanFiles() {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);

        auto temporaryPath = path_;
        temporaryPath += ".tmp";
        ignored.clear();
        std::filesystem::remove(temporaryPath, ignored);

        ignored.clear();
        std::filesystem::remove(backupPath_, ignored);
    }

    // Move the existing database aside and create a directory at its old path.
    // Writing the .tmp file can succeed, but replacing the directory must fail.
    bool forceReplacementFailure() {
        std::error_code error;
        std::filesystem::rename(path_, backupPath_, error);
        if (error) {
            return false;
        }

        error.clear();
        std::filesystem::create_directory(path_, error);
        if (error) {
            std::error_code restoreError;
            std::filesystem::rename(backupPath_, path_, restoreError);
            return false;
        }

        return true;
    }

    bool backupContains(std::size_t expectedNodes, std::size_t expectedEdges) {
        StorageManager storage(backupPath_);
        auto result = storage.load();
        return result.hasValue() &&
               result.value().nodes.size() == expectedNodes &&
               result.value().edges.size() == expectedEdges;
    }
};

// ------------------------------------------------------------
// Test 1: Manager must be opened before mutations.
// ------------------------------------------------------------

TEST_F(GraphManagerTest, RejectsMutationBeforeOpen) {
    GraphManager manager(path_);

    EXPECT_FALSE(manager.isReady());

    auto result = manager.createNode(1);

    EXPECT_FALSE(result.hasValue());
    EXPECT_EQ(manager.graph().nodeCount(), 0);
}

// ------------------------------------------------------------
// Test 2: Open a missing database as an empty graph.
// ------------------------------------------------------------

TEST_F(GraphManagerTest, OpensMissingDatabaseAsEmptyGraph) {
    GraphManager manager(path_);

    auto result = manager.open();

    ASSERT_TRUE(result.hasValue());
    EXPECT_TRUE(manager.isReady());
    EXPECT_EQ(manager.graph().nodeCount(), 0);
    EXPECT_EQ(manager.graph().edgeCount(), 0);
}

// ------------------------------------------------------------
// Test 3: Save a node and restore it after reopening.
// ------------------------------------------------------------

TEST_F(GraphManagerTest, PersistsNodeAcrossRestart) {
    {
        GraphManager manager(path_);

        ASSERT_TRUE(manager.open().hasValue());
        ASSERT_TRUE(manager.createNode(10).hasValue());

        EXPECT_EQ(manager.graph().nodeCount(), 1);
    }

    // A new manager simulates restarting the application.
    GraphManager restarted(path_);

    ASSERT_TRUE(restarted.open().hasValue());

    EXPECT_EQ(restarted.graph().nodeCount(), 1);
    EXPECT_NE(restarted.graph().getNode(10), nullptr);
}

// ------------------------------------------------------------
// Test 4: Persist nodes and edges across restart.
// ------------------------------------------------------------

TEST_F(GraphManagerTest, PersistsEdgesAcrossRestart) {
    {
        GraphManager manager(path_);

        ASSERT_TRUE(manager.open().hasValue());
        ASSERT_TRUE(manager.createNode(1).hasValue());
        ASSERT_TRUE(manager.createNode(2).hasValue());

        ASSERT_TRUE(
            manager.createEdge(1, 2, "FOLLOWS").hasValue()
        );

        EXPECT_EQ(manager.graph().edgeCount(), 1);
    }

    GraphManager restarted(path_);

    ASSERT_TRUE(restarted.open().hasValue());

    EXPECT_NE(
        restarted.graph().getEdge(1, 2, "FOLLOWS"),
        nullptr
    );

    EXPECT_EQ(restarted.graph().getOutgoingEdges(1).size(), 1);
}

// ------------------------------------------------------------
// Test 5: A rejected duplicate must not change the graph.
// ------------------------------------------------------------

TEST_F(GraphManagerTest, DuplicateNodeDoesNotChangeGraph) {
    GraphManager manager(path_);

    ASSERT_TRUE(manager.open().hasValue());
    ASSERT_TRUE(manager.createNode(1).hasValue());

    auto result = manager.createNode(1);

    EXPECT_FALSE(result.hasValue());
    EXPECT_EQ(manager.graph().nodeCount(), 1);
}

// ------------------------------------------------------------
// Test 6: Deleting a node also deletes its connected edges.
// ------------------------------------------------------------

TEST_F(GraphManagerTest, DeletingNodePersistsCascadeDeletion) {
    {
        GraphManager manager(path_);

        ASSERT_TRUE(manager.open().hasValue());
        ASSERT_TRUE(manager.createNode(1).hasValue());
        ASSERT_TRUE(manager.createNode(2).hasValue());
        ASSERT_TRUE(
            manager.createEdge(1, 2, "FOLLOWS").hasValue()
        );

        ASSERT_TRUE(manager.deleteNode(1).hasValue());

        EXPECT_EQ(manager.graph().nodeCount(), 1);
        EXPECT_EQ(manager.graph().edgeCount(), 0);
    }

    GraphManager restarted(path_);

    ASSERT_TRUE(restarted.open().hasValue());

    EXPECT_EQ(restarted.graph().nodeCount(), 1);
    EXPECT_EQ(restarted.graph().edgeCount(), 0);
    EXPECT_NE(restarted.graph().getNode(2), nullptr);
}


// ------------------------------------------------------------
// Additional GraphManager Regression Tests
// ------------------------------------------------------------

// Test 7: Every mutation rejects calls before open() with NotReady.
TEST_F(GraphManagerTest, AllMutationsRejectCallsBeforeOpen) {
    GraphManager manager(path_);

    auto createNodeResult = manager.createNode(1);
    auto deleteNodeResult = manager.deleteNode(1);
    auto createEdgeResult = manager.createEdge(1, 2, "FOLLOWS");
    auto deleteEdgeResult = manager.deleteEdge(1, 2, "FOLLOWS");

    ASSERT_FALSE(createNodeResult.hasValue());
    EXPECT_EQ(createNodeResult.error(), StorageError::NotReady);

    ASSERT_FALSE(deleteNodeResult.hasValue());
    EXPECT_EQ(deleteNodeResult.error(), StorageError::NotReady);

    ASSERT_FALSE(createEdgeResult.hasValue());
    EXPECT_EQ(createEdgeResult.error(), StorageError::NotReady);

    ASSERT_FALSE(deleteEdgeResult.hasValue());
    EXPECT_EQ(deleteEdgeResult.error(), StorageError::NotReady);

    EXPECT_FALSE(manager.isReady());
    EXPECT_EQ(manager.graph().nodeCount(), 0);
    EXPECT_EQ(manager.graph().edgeCount(), 0);
}

// Test 8: Deleting an edge that does not exist returns MissingEdge.
TEST_F(GraphManagerTest, DeletingMissingEdgeReturnsMissingEdge) {
    GraphManager manager(path_);

    ASSERT_TRUE(manager.open().hasValue());
    ASSERT_TRUE(manager.createNode(1).hasValue());
    ASSERT_TRUE(manager.createNode(2).hasValue());

    auto result = manager.deleteEdge(1, 2, "FOLLOWS");

    ASSERT_FALSE(result.hasValue());
    EXPECT_EQ(result.error(), StorageError::MissingEdge);

    // A rejected operation must leave the graph unchanged.
    EXPECT_EQ(manager.graph().nodeCount(), 2);
    EXPECT_EQ(manager.graph().edgeCount(), 0);
}

// Test 9: Failed open clears previously loaded in-memory state.
TEST_F(GraphManagerTest, FailedReopenClearsGraphAndReadiness) {
    {
        GraphManager initial(path_);

        ASSERT_TRUE(initial.open().hasValue());
        ASSERT_TRUE(initial.createNode(42).hasValue());
    }

    GraphManager manager(path_);

    ASSERT_TRUE(manager.open().hasValue());
    ASSERT_NE(manager.graph().getNode(42), nullptr);

    // Replace the database file with malformed contents.
    {
        std::ofstream output(path_, std::ios::trunc);
        ASSERT_TRUE(output.is_open());
        output << "THIS_IS_NOT_A_GRAPHLITE_DATABASE\\n";
        ASSERT_TRUE(output.good());
    }

    auto result = manager.open();

    ASSERT_FALSE(result.hasValue());
    EXPECT_FALSE(manager.isReady());
    EXPECT_EQ(manager.graph().nodeCount(), 0);
    EXPECT_EQ(manager.graph().edgeCount(), 0);
}

// ------------------------------------------------------------
// Persistence failure safety tests
// ------------------------------------------------------------

// A failed save must not add a node to memory, and the previous database
// must remain readable.
TEST_F(GraphManagerTest, FailedNodeCreateDoesNotCommitToMemory) {
    GraphManager manager(path_);
    ASSERT_TRUE(manager.open().hasValue());
    ASSERT_TRUE(manager.createNode(1).hasValue());
    ASSERT_TRUE(forceReplacementFailure());

    auto result = manager.createNode(2);
    ASSERT_FALSE(result.hasValue());
    EXPECT_EQ(result.error(), StorageError::FileWriteError);

    EXPECT_EQ(manager.graph().nodeCount(), 1);
    EXPECT_NE(manager.graph().getNode(1), nullptr);
    EXPECT_EQ(manager.graph().getNode(2), nullptr);
    EXPECT_TRUE(backupContains(1, 0));
}

TEST_F(GraphManagerTest, FailedEdgeCreateDoesNotCommitToMemory) {
    GraphManager manager(path_);
    ASSERT_TRUE(manager.open().hasValue());
    ASSERT_TRUE(manager.createNode(1).hasValue());
    ASSERT_TRUE(manager.createNode(2).hasValue());
    ASSERT_TRUE(forceReplacementFailure());

    auto result = manager.createEdge(1, 2, "FOLLOWS");
    ASSERT_FALSE(result.hasValue());
    EXPECT_EQ(result.error(), StorageError::FileWriteError);

    EXPECT_EQ(manager.graph().nodeCount(), 2);
    EXPECT_EQ(manager.graph().edgeCount(), 0);
    EXPECT_EQ(manager.graph().getEdge(1, 2, "FOLLOWS"), nullptr);
    EXPECT_TRUE(backupContains(2, 0));
}

TEST_F(GraphManagerTest, FailedNodeDeleteDoesNotCommitToMemory) {
    GraphManager manager(path_);
    ASSERT_TRUE(manager.open().hasValue());
    ASSERT_TRUE(manager.createNode(1).hasValue());
    ASSERT_TRUE(manager.createNode(2).hasValue());
    ASSERT_TRUE(manager.createEdge(1, 2, "FOLLOWS").hasValue());
    ASSERT_TRUE(forceReplacementFailure());

    auto result = manager.deleteNode(1);
    ASSERT_FALSE(result.hasValue());
    EXPECT_EQ(result.error(), StorageError::FileWriteError);

    EXPECT_EQ(manager.graph().nodeCount(), 2);
    EXPECT_EQ(manager.graph().edgeCount(), 1);
    EXPECT_NE(manager.graph().getNode(1), nullptr);
    EXPECT_NE(manager.graph().getEdge(1, 2, "FOLLOWS"), nullptr);
    EXPECT_TRUE(backupContains(2, 1));
}

TEST_F(GraphManagerTest, FailedEdgeDeleteDoesNotCommitToMemory) {
    GraphManager manager(path_);
    ASSERT_TRUE(manager.open().hasValue());
    ASSERT_TRUE(manager.createNode(1).hasValue());
    ASSERT_TRUE(manager.createNode(2).hasValue());
    ASSERT_TRUE(manager.createEdge(1, 2, "FOLLOWS").hasValue());
    ASSERT_TRUE(forceReplacementFailure());

    auto result = manager.deleteEdge(1, 2, "FOLLOWS");
    ASSERT_FALSE(result.hasValue());
    EXPECT_EQ(result.error(), StorageError::FileWriteError);

    EXPECT_EQ(manager.graph().nodeCount(), 2);
    EXPECT_EQ(manager.graph().edgeCount(), 1);
    EXPECT_NE(manager.graph().getEdge(1, 2, "FOLLOWS"), nullptr);
    EXPECT_TRUE(backupContains(2, 1));
}


// ------------------------------------------------------------
// GraphManager Property Operation Tests
// ------------------------------------------------------------

// Setting a node property persists it across a restart.
TEST_F(GraphManagerTest, PersistsNodePropertyAcrossRestart) {
    {
        GraphManager manager(path_);
        ASSERT_TRUE(manager.open().hasValue());
        ASSERT_TRUE(manager.createNode(1).hasValue());

        ASSERT_TRUE(
            manager.setNodeProperty(1, "name", Value{std::string("Alice")})
                .hasValue()
        );
        ASSERT_TRUE(
            manager.setNodeProperty(1, "age", Value{int64_t{21}})
                .hasValue()
        );
    }

    GraphManager restarted(path_);
    ASSERT_TRUE(restarted.open().hasValue());

    const Node* node = restarted.graph().getNode(1);
    ASSERT_NE(node, nullptr);

    const Value* name = node->getProperty("name");
    ASSERT_NE(name, nullptr);
    ASSERT_TRUE(std::holds_alternative<std::string>(*name));
    EXPECT_EQ(std::get<std::string>(*name), "Alice");

    const Value* age = node->getProperty("age");
    ASSERT_NE(age, nullptr);
    ASSERT_TRUE(std::holds_alternative<int64_t>(*age));
    EXPECT_EQ(std::get<int64_t>(*age), 21);
}

// Setting the same property key updates its value.
TEST_F(GraphManagerTest, UpdatesExistingNodeProperty) {
    GraphManager manager(path_);
    ASSERT_TRUE(manager.open().hasValue());
    ASSERT_TRUE(manager.createNode(1).hasValue());

    ASSERT_TRUE(
        manager.setNodeProperty(1, "name", Value{std::string("Alice")})
            .hasValue()
    );
    ASSERT_TRUE(
        manager.setNodeProperty(1, "name", Value{std::string("Bob")})
            .hasValue()
    );

    const Node* node = manager.graph().getNode(1);
    ASSERT_NE(node, nullptr);
    ASSERT_NE(node->getProperty("name"), nullptr);
    EXPECT_EQ(
        std::get<std::string>(*node->getProperty("name")),
        "Bob"
    );
}

// Removing a node property persists the removal.
TEST_F(GraphManagerTest, RemovesNodeProperty) {
    GraphManager manager(path_);
    ASSERT_TRUE(manager.open().hasValue());
    ASSERT_TRUE(manager.createNode(1).hasValue());
    ASSERT_TRUE(
        manager.setNodeProperty(1, "name", Value{std::string("Alice")})
            .hasValue()
    );

    ASSERT_TRUE(manager.removeNodeProperty(1, "name").hasValue());

    const Node* node = manager.graph().getNode(1);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->getProperty("name"), nullptr);
}

// Setting an edge property persists it across a restart.
TEST_F(GraphManagerTest, PersistsEdgePropertyAcrossRestart) {
    {
        GraphManager manager(path_);
        ASSERT_TRUE(manager.open().hasValue());
        ASSERT_TRUE(manager.createNode(1).hasValue());
        ASSERT_TRUE(manager.createNode(2).hasValue());
        ASSERT_TRUE(manager.createEdge(1, 2, "FOLLOWS").hasValue());

        ASSERT_TRUE(
            manager.setEdgeProperty(
                1, 2, "FOLLOWS", "weight", Value{2.5}
            ).hasValue()
        );
        ASSERT_TRUE(
            manager.setEdgeProperty(
                1, 2, "FOLLOWS", "verified", Value{true}
            ).hasValue()
        );
    }

    GraphManager restarted(path_);
    ASSERT_TRUE(restarted.open().hasValue());

    const Edge* edge = restarted.graph().getEdge(1, 2, "FOLLOWS");
    ASSERT_NE(edge, nullptr);

    const Value* weight = edge->getProperty("weight");
    ASSERT_NE(weight, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(*weight));
    EXPECT_DOUBLE_EQ(std::get<double>(*weight), 2.5);

    const Value* verified = edge->getProperty("verified");
    ASSERT_NE(verified, nullptr);
    ASSERT_TRUE(std::holds_alternative<bool>(*verified));
    EXPECT_TRUE(std::get<bool>(*verified));
}

// Removing an edge property persists the removal.
TEST_F(GraphManagerTest, RemovesEdgeProperty) {
    GraphManager manager(path_);
    ASSERT_TRUE(manager.open().hasValue());
    ASSERT_TRUE(manager.createNode(1).hasValue());
    ASSERT_TRUE(manager.createNode(2).hasValue());
    ASSERT_TRUE(manager.createEdge(1, 2, "FOLLOWS").hasValue());
    ASSERT_TRUE(
        manager.setEdgeProperty(
            1, 2, "FOLLOWS", "weight", Value{2.5}
        ).hasValue()
    );

    ASSERT_TRUE(
        manager.removeEdgeProperty(1, 2, "FOLLOWS", "weight").hasValue()
    );

    const Edge* edge = manager.graph().getEdge(1, 2, "FOLLOWS");
    ASSERT_NE(edge, nullptr);
    EXPECT_EQ(edge->getProperty("weight"), nullptr);
}

// Property operations reject missing nodes and edges without changing state.
TEST_F(GraphManagerTest, PropertyOperationsRejectMissingEntities) {
    GraphManager manager(path_);
    ASSERT_TRUE(manager.open().hasValue());

    auto setNode = manager.setNodeProperty(
        99, "name", Value{std::string("Nobody")}
    );
    auto removeNode = manager.removeNodeProperty(99, "name");
    auto setEdge = manager.setEdgeProperty(
        1, 2, "FOLLOWS", "weight", Value{1.0}
    );
    auto removeEdge = manager.removeEdgeProperty(
        1, 2, "FOLLOWS", "weight"
    );

    EXPECT_FALSE(setNode.hasValue());
    EXPECT_FALSE(removeNode.hasValue());
    EXPECT_FALSE(setEdge.hasValue());
    EXPECT_FALSE(removeEdge.hasValue());

    EXPECT_EQ(manager.graph().nodeCount(), 0);
    EXPECT_EQ(manager.graph().edgeCount(), 0);
}

// A failed property save must not update the active in-memory graph.
TEST_F(GraphManagerTest, FailedNodePropertySaveDoesNotCommitToMemory) {
    GraphManager manager(path_);
    ASSERT_TRUE(manager.open().hasValue());
    ASSERT_TRUE(manager.createNode(1).hasValue());
    ASSERT_TRUE(manager.setNodeProperty(
        1, "name", Value{std::string("Before")}
    ).hasValue());

    ASSERT_TRUE(forceReplacementFailure());

    auto result = manager.setNodeProperty(
        1, "name", Value{std::string("After")}
    );

    ASSERT_FALSE(result.hasValue());
    EXPECT_EQ(result.error(), StorageError::FileWriteError);

    const Node* node = manager.graph().getNode(1);
    ASSERT_NE(node, nullptr);
    ASSERT_NE(node->getProperty("name"), nullptr);
    EXPECT_EQ(
        std::get<std::string>(*node->getProperty("name")),
        "Before"
    );

    EXPECT_TRUE(backupContains(1, 0));
}


// ------------------------------------------------------------
// Name validation and missing-property regression tests
// ------------------------------------------------------------

TEST_F(GraphManagerTest, RejectsInvalidNamesWithoutChangingGraph) {
    GraphManager manager(path_);
    ASSERT_TRUE(manager.open().hasValue());
    ASSERT_TRUE(manager.createNode(1).hasValue());
    ASSERT_TRUE(manager.createNode(2).hasValue());

    auto invalidEdgeLabel = manager.createEdge(1, 2, "INVALID-LABEL");
    auto invalidNodeProperty = manager.setNodeProperty(
        1, "invalid key", Value{std::string("value")}
    );

    ASSERT_FALSE(invalidEdgeLabel.hasValue());
    EXPECT_EQ(invalidEdgeLabel.error(), StorageError::InvalidName);
    ASSERT_FALSE(invalidNodeProperty.hasValue());
    EXPECT_EQ(invalidNodeProperty.error(), StorageError::InvalidName);

    EXPECT_EQ(manager.graph().edgeCount(), 0);
    EXPECT_EQ(manager.graph().getNode(1)->getProperty("invalid key"), nullptr);
}

TEST_F(GraphManagerTest, RemovingMissingPropertiesReturnsMissingProperty) {
    GraphManager manager(path_);
    ASSERT_TRUE(manager.open().hasValue());
    ASSERT_TRUE(manager.createNode(1).hasValue());
    ASSERT_TRUE(manager.createNode(2).hasValue());
    ASSERT_TRUE(manager.createEdge(1, 2, "FOLLOWS").hasValue());

    auto missingNodeProperty = manager.removeNodeProperty(1, "missing");
    auto missingEdgeProperty = manager.removeEdgeProperty(
        1, 2, "FOLLOWS", "missing"
    );

    ASSERT_FALSE(missingNodeProperty.hasValue());
    EXPECT_EQ(missingNodeProperty.error(), StorageError::MissingProperty);
    ASSERT_FALSE(missingEdgeProperty.hasValue());
    EXPECT_EQ(missingEdgeProperty.error(), StorageError::MissingProperty);

    EXPECT_EQ(manager.graph().nodeCount(), 2);
    EXPECT_EQ(manager.graph().edgeCount(), 1);
}
