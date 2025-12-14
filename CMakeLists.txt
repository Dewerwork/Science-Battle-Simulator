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
set(COMMON_FLAGS "-Wall -Wextra -Wpedantic")

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
# Source Files
# ==============================================================================

# Core library (data structures)
set(CORE_SOURCES
    src/core/unit.cpp
    src/core/model.cpp
    src/core/weapon.cpp
    src/core/special_rule.cpp
    src/core/army_list.cpp
)

# Engine library (combat mechanics)
set(ENGINE_SOURCES
    src/engine/dice.cpp
    src/engine/combat.cpp
    src/engine/wound_allocation.cpp
    src/engine/morale.cpp
    src/engine/scenario.cpp
    src/engine/multi_round.cpp
)

# Simulation library (Monte Carlo, statistics)
set(SIMULATION_SOURCES
    src/simulation/simulator.cpp
    src/simulation/statistics.cpp
    src/simulation/thread_pool.cpp
    src/simulation/batch_processor.cpp
)

# IO library (parsers, output)
set(IO_SOURCES
    src/io/army_parser.cpp
    src/io/config_loader.cpp
    src/io/results_exporter.cpp
)

# Utility library
set(UTILS_SOURCES
    src/utils/memory_pool.cpp
    src/utils/timer.cpp
    src/utils/progress.cpp
)

# ==============================================================================
# Libraries
# ==============================================================================

# Core library
add_library(battle_core STATIC ${CORE_SOURCES})
target_include_directories(battle_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_compile_options(battle_core PRIVATE ${COMMON_FLAGS})

# Engine library
add_library(battle_engine STATIC ${ENGINE_SOURCES})
target_include_directories(battle_engine PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(battle_engine PUBLIC battle_core)
target_compile_options(battle_engine PRIVATE ${COMMON_FLAGS})

# Simulation library
add_library(battle_simulation STATIC ${SIMULATION_SOURCES})
target_include_directories(battle_simulation PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(battle_simulation PUBLIC battle_engine Threads::Threads)
if(OpenMP_CXX_FOUND)
    target_link_libraries(battle_simulation PUBLIC OpenMP::OpenMP_CXX)
    target_compile_definitions(battle_simulation PUBLIC USE_OPENMP)
endif()
target_compile_options(battle_simulation PRIVATE ${COMMON_FLAGS})

# IO library
add_library(battle_io STATIC ${IO_SOURCES})
target_include_directories(battle_io PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(battle_io PUBLIC battle_core)
target_compile_options(battle_io PRIVATE ${COMMON_FLAGS})

# Utils library
add_library(battle_utils STATIC ${UTILS_SOURCES})
target_include_directories(battle_utils PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_compile_options(battle_utils PRIVATE ${COMMON_FLAGS})

# ==============================================================================
# Main Executable
# ==============================================================================

add_executable(battle_sim src/main.cpp)
target_link_libraries(battle_sim PRIVATE
    battle_simulation
    battle_io
    battle_utils
)
target_compile_options(battle_sim PRIVATE ${COMMON_FLAGS})

# ==============================================================================
# Benchmarks
# ==============================================================================

add_executable(benchmark_dice benchmarks/benchmark_dice.cpp)
target_link_libraries(benchmark_dice PRIVATE battle_engine battle_utils)

add_executable(benchmark_combat benchmarks/benchmark_combat.cpp)
target_link_libraries(benchmark_combat PRIVATE battle_simulation battle_utils)

# ==============================================================================
# Tests
# ==============================================================================

enable_testing()

add_executable(test_dice tests/test_dice.cpp)
target_link_libraries(test_dice PRIVATE battle_engine)
add_test(NAME DiceTests COMMAND test_dice)

add_executable(test_combat tests/test_combat.cpp)
target_link_libraries(test_combat PRIVATE battle_engine)
add_test(NAME CombatTests COMMAND test_combat)

add_executable(test_scenarios tests/test_scenarios.cpp)
target_link_libraries(test_scenarios PRIVATE battle_simulation)
add_test(NAME ScenarioTests COMMAND test_scenarios)

# ==============================================================================
# Installation
# ==============================================================================

install(TARGETS battle_sim DESTINATION bin)
install(DIRECTORY data/ DESTINATION share/battle_sim/data)

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
