# ============================================================
# GraphLite - CMake Build Configuration
# ============================================================
#
# PURPOSE
# -------
# This file tells CMake how to configure, compile, link, and test
# the GraphLite C++17 graph database project.
#
# WHAT THIS FILE BUILDS
# ---------------------
# 1. graphlite_core:
#    A reusable static library containing the database engine:
#    Node, Edge, Graph, StorageManager, and GraphManager.
#
# 2. graphlite:
#    The application executable. Its entry point is src/main.cpp.
#    It links against graphlite_core instead of compiling the core
#    implementation files a second time.
#
# 3. graphlite_tests:
#    An automated test executable built from tests/smoke_test.cpp.
#    It links against graphlite_core and GoogleTest.
#
# BUILD FLOW
# ----------
# CMake configures the build system. The compiler builds the source
# files, the linker combines the required objects/libraries, and
# CTest runs the registered automated tests.
#
# TYPICAL COMMANDS (run from the project root)
# ---------------------------------------------
# Configure: cmake -S . -B build
# Build:     cmake --build build
# Test:      ctest --test-dir build --output-on-failure
#
# NOTE
# ----
# The first configure may download GoogleTest and therefore may
# require an internet connection. The build also requires all source
# files listed below to exist.
# ============================================================


# ============================================================
# 1. PROJECT CONFIGURATION
# ============================================================

# Require CMake version 3.20 or newer.
cmake_minimum_required(VERSION 3.20)

# Declare the project name and enable the C++ language.
project(GraphLite LANGUAGES CXX)


# ============================================================
# 2. C++ STANDARD
# ============================================================

# Compile the project using standard C++17.
set(CMAKE_CXX_STANDARD 17)

# Fail configuration if the compiler cannot provide C++17.
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Disable compiler-specific language extensions for portability.
set(CMAKE_CXX_EXTENSIONS OFF)


# ============================================================
# 3. TESTING AND DEPENDENCY SUPPORT
# ============================================================

# Enable CTest and define the BUILD_TESTING option.
# BUILD_TESTING is normally ON unless explicitly disabled.
include(CTest)

# Provide CMake's mechanism for downloading external dependencies.
include(FetchContent)


# ============================================================
# 4. GOOGLETEST DEPENDENCY
# ============================================================

# Declare the GoogleTest release used by GraphLite's test suite.
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)

# Download/configure GoogleTest and make its CMake targets available.
# The first configuration may need internet access.
FetchContent_MakeAvailable(googletest)


# ============================================================
# 5. GRAPH DATABASE CORE LIBRARY
# ============================================================

# Compile the database implementation once into a reusable static
# library. Both the application and the tests link against this
# target, avoiding duplicate source-file lists in each executable.
add_library(graphlite_core STATIC
    src/node.cpp
    src/edge.cpp
    src/graph.cpp
    src/storage_manager.cpp
    src/graph_manager.cpp
)

# Make the project's include/ directory available to this library
# and to every target that links against it.
target_include_directories(graphlite_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Propagate the C++17 requirement to targets linking this library.
target_compile_features(graphlite_core PUBLIC cxx_std_17)

# Enable useful compiler warnings.
if(MSVC)
    target_compile_options(graphlite_core PRIVATE /W4)
else()
    target_compile_options(graphlite_core PRIVATE -Wall -Wextra -Wpedantic)
endif()


# ============================================================
# 6. GRAPH LITE APPLICATION
# ============================================================

# Build the application executable from its entry point.
# Database implementation files are already compiled into
# graphlite_core, so they do not need to be listed again here.
add_executable(graphlite
    src/main.cpp
)

# Link the reusable database library into the application.
target_link_libraries(graphlite
    PRIVATE graphlite_core
)


# ============================================================
# 7. AUTOMATED TESTS
# ============================================================

# Build and register automated tests only when testing is enabled.
if(BUILD_TESTING)

    # Compile the test cases. The database implementation is supplied
    # by graphlite_core, so do not list its .cpp files a second time.
    add_executable(graphlite_tests
        tests/smoke_test.cpp
    )

    # Link the test executable to the database library and GoogleTest's
    # main function, which starts the test runner.
    target_link_libraries(graphlite_tests
        PRIVATE
            graphlite_core
            GTest::gtest_main
    )

    # Load CMake helpers for discovering GoogleTest test cases.
    include(GoogleTest)

    # Discover individual tests and register them with CTest.
    gtest_discover_tests(graphlite_tests)

endif()