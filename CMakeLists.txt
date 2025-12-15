cmake_minimum_required(VERSION 3.20)
project(BattleSimulator VERSION 1.0.0 LANGUAGES CXX)

# ==============================================================================
# Project Configuration
# ==============================================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type configuration
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

# ==============================================================================
# Compiler Flags for Maximum Performance
# ==============================================================================

# Common flags for all builds
set(COMMON_FLAGS -Wall -Wextra -Wpedantic)

# Release flags - optimized for 100 billion matchups
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -mtune=native -flto")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ffast-math -funroll-loops")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ftree-vectorize")

# Debug flags
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -DDEBUG -fsanitize=address,undefined")

# RelWithDebInfo for profiling
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG -march=native")

# ==============================================================================
# Dependencies
# ==============================================================================

# Find threading library
find_package(Threads REQUIRED)

# Optional: OpenMP for additional parallelization
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP found - parallel simulations enabled")
else()
    message(STATUS "OpenMP not found - using std::thread only")
endif()

# ==============================================================================
# Phase 1: Parser Test (header-only for now)
# ==============================================================================

# The parser is header-only, so we just need the test executable
# Libraries will be added as source files are implemented

# ==============================================================================
# Main Executable (placeholder until engine is complete)
# ==============================================================================

# add_executable(battle_sim src/main.cpp)
# target_include_directories(battle_sim PRIVATE ${CMAKE_SOURCE_DIR}/include)
# target_link_libraries(battle_sim PRIVATE Threads::Threads)
# target_compile_options(battle_sim PRIVATE ${COMMON_FLAGS})

# ==============================================================================
# Tests
# ==============================================================================

enable_testing()

# Parser test - header-only, no library dependencies
add_executable(test_parser src/test_parser.cpp)
target_include_directories(test_parser PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_options(test_parser PRIVATE ${COMMON_FLAGS})

# Game engine test - header-only
add_executable(test_game src/test_game.cpp)
target_include_directories(test_game PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_options(test_game PRIVATE ${COMMON_FLAGS})

# Benchmark - parallel simulation
add_executable(benchmark_sim src/benchmark_sim.cpp)
target_include_directories(benchmark_sim PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(benchmark_sim PRIVATE Threads::Threads)
target_compile_options(benchmark_sim PRIVATE ${COMMON_FLAGS})

# Commented out until source files are implemented:
# add_executable(test_dice tests/test_dice.cpp)
# target_link_libraries(test_dice PRIVATE battle_engine)
# add_test(NAME DiceTests COMMAND test_dice)

# add_executable(test_combat tests/test_combat.cpp)
# target_link_libraries(test_combat PRIVATE battle_engine)
# add_test(NAME CombatTests COMMAND test_combat)

# add_executable(test_scenarios tests/test_scenarios.cpp)
# target_link_libraries(test_scenarios PRIVATE battle_simulation)
# add_test(NAME ScenarioTests COMMAND test_scenarios)

# ==============================================================================
# Installation (enabled when battle_sim is ready)
# ==============================================================================

# install(TARGETS battle_sim DESTINATION bin)
# install(DIRECTORY data/ DESTINATION share/battle_sim/data)

# ==============================================================================
# Summary
# ==============================================================================

message(STATUS "")
message(STATUS "=== Battle Simulator Configuration ===")
message(STATUS "Version:        ${PROJECT_VERSION}")
message(STATUS "Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "OpenMP:         ${OpenMP_CXX_FOUND}")
message(STATUS "")
