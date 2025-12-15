/**
 * Battle Simulator - OPR Grimdark Future Combat Analysis
 *
 * A high-performance Monte Carlo simulation engine for analyzing
 * unit matchups in One Page Rules tabletop wargames.
 *
 * Designed to handle 100 billion+ matchup simulations.
 */

#include "core/types.hpp"
#include "core/unit.hpp"
#include "core/weapon.hpp"
#include "core/faction_rules.hpp"
#include "engine/game_runner.hpp"
#include "engine/dice.hpp"
#include "parser/unit_parser.hpp"

#include <iostream>
#include <iomanip>
#include <chrono>
#include <string>
#include <vector>

using namespace battle;

// ==============================================================================
// Progress Display
// ==============================================================================

class ProgressDisplay {
public:
    void update(u64 completed, u64 total, f64 rate) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - start_time_).count();

        if (elapsed - last_update_ < 100 && completed < total) return;
        last_update_ = elapsed;

        f64 pct = 100.0 * completed / total;
        f64 eta_sec = (total - completed) / rate;

        std::cout << "\r[";

        // Progress bar
        int bar_width = 40;
        int filled = static_cast<int>(bar_width * pct / 100.0);
        for (int i = 0; i < bar_width; ++i) {
            std::cout << (i < filled ? '=' : (i == filled ? '>' : ' '));
        }

        std::cout << "] " << std::fixed << std::setprecision(1) << pct << "% ";
        std::cout << "(" << format_number(completed) << "/" << format_number(total) << ") ";
        std::cout << format_number(static_cast<u64>(rate)) << "/s ";
        std::cout << "ETA: " << format_time(eta_sec) << "   " << std::flush;
    }

    void finish() {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time_).count();
        std::cout << "\nCompleted in " << format_time(static_cast<f64>(elapsed)) << std::endl;
    }

private:
    std::chrono::steady_clock::time_point start_time_ = std::chrono::steady_clock::now();
    i64 last_update_ = 0;

    static std::string format_number(u64 n) {
        if (n >= 1'000'000'000) return std::to_string(n / 1'000'000'000) + "B";
        if (n >= 1'000'000) return std::to_string(n / 1'000'000) + "M";
        if (n >= 1'000) return std::to_string(n / 1'000) + "K";
        return std::to_string(n);
    }

    static std::string format_time(f64 seconds) {
        if (seconds < 60) return std::to_string(static_cast<int>(seconds)) + "s";
        if (seconds < 3600) {
            int m = static_cast<int>(seconds / 60);
            int s = static_cast<int>(seconds) % 60;
            return std::to_string(m) + "m " + std::to_string(s) + "s";
        }
        int h = static_cast<int>(seconds / 3600);
        int m = (static_cast<int>(seconds) % 3600) / 60;
        return std::to_string(h) + "h " + std::to_string(m) + "m";
    }
};

// ==============================================================================
// Demo Unit Definitions (parsed from text format)
// ==============================================================================

const char* DEMO_UNITS = R"(
Assault Squad [5] Q3+ D4+ | 150pts | Furious
5x CCW (A2), 5x 12" Pistol (A1)

Terminator Squad [5] Q3+ D2+ | 300pts | Tough(3)
5x 24" Storm Bolter (A2), 5x Power Fist (A2, AP(3), Deadly(3))
)";

// ==============================================================================
// Main Entry Point
// ==============================================================================

void print_banner() {
    std::cout << R"(
  ____        _   _   _        ____  _
 | __ )  __ _| |_| |_| | ___  / ___|(_)_ __ ___
 |  _ \ / _` | __| __| |/ _ \ \___ \| | '_ ` _ \
 | |_) | (_| | |_| |_| |  __/  ___) | | | | | | |
 |____/ \__,_|\__|\__|_|\___| |____/|_|_| |_| |_|

 OPR Grimdark Future Combat Simulator v1.0
 Optimized for 100 billion matchups
)" << std::endl;
}

void print_system_info() {
    std::cout << "System Configuration:" << std::endl;
    std::cout << "  Threads: " << std::thread::hardware_concurrency() << std::endl;
    std::cout << "  Game Result Size: " << sizeof(GameResult) << " bytes" << std::endl;
    std::cout << "  Game State Size: " << sizeof(GameState) << " bytes" << std::endl;
    std::cout << "  Unit Size: " << sizeof(Unit) << " bytes" << std::endl;
    std::cout << "  Model Size: " << sizeof(Model) << " bytes" << std::endl;
    std::cout << std::endl;
}

void run_demo_simulation() {
    std::cout << "Running full game simulation (with movement & objectives)..." << std::endl;
    std::cout << std::endl;

    // Initialize faction rules
    initialize_faction_rules();

    // Parse demo units
    auto parse_result = UnitParser::parse_string(DEMO_UNITS, "Demo");
    if (parse_result.units.size() < 2) {
        std::cerr << "Failed to parse demo units!" << std::endl;
        return;
    }

    Unit& assault = parse_result.units[0];
    Unit& terminators = parse_result.units[1];

    // Display AI types
    auto ai_type_str = [](AIType t) -> const char* {
        switch (t) {
            case AIType::Melee: return "Melee";
            case AIType::Shooting: return "Shooting";
            case AIType::Hybrid: return "Hybrid";
            default: return "Unknown";
        }
    };

    std::cout << "Matchup: " << assault.name.c_str() << " vs " << terminators.name.c_str() << std::endl;
    std::cout << "  " << assault.name.c_str() << ": " << (int)assault.model_count << " models, "
              << assault.points_cost << " pts, AI: " << ai_type_str(assault.ai_type) << std::endl;
    std::cout << "  " << terminators.name.c_str() << ": " << (int)terminators.model_count << " models, "
              << terminators.points_cost << " pts, AI: " << ai_type_str(terminators.ai_type) << std::endl;
    std::cout << std::endl;

    std::cout << "Game Rules:" << std::endl;
    std::cout << "  - Units start 24\" apart (12\" from center)" << std::endl;
    std::cout << "  - Objective at center, control within 3\"" << std::endl;
    std::cout << "  - 4 rounds maximum" << std::endl;
    std::cout << "  - Winner: unit controlling objective at end" << std::endl;
    std::cout << std::endl;

    // Run simulation
    constexpr u64 NUM_GAMES = 100000;
    ProgressDisplay progress;

    // Statistics accumulators
    u64 unit_a_wins = 0;
    u64 unit_b_wins = 0;
    u64 draws = 0;
    u64 total_rounds = 0;
    u64 total_wounds_a = 0;
    u64 total_wounds_b = 0;
    u64 total_kills_a = 0;
    u64 total_kills_b = 0;
    u64 total_obj_rounds_a = 0;
    u64 total_obj_rounds_b = 0;

    DiceRoller dice;
    GameRunner runner(dice);

    auto start = std::chrono::high_resolution_clock::now();

    for (u64 i = 0; i < NUM_GAMES; ++i) {
        GameResult result = runner.run_game(assault, terminators);

        switch (result.winner) {
            case GameWinner::UnitA: unit_a_wins++; break;
            case GameWinner::UnitB: unit_b_wins++; break;
            case GameWinner::Draw: draws++; break;
        }

        total_rounds += result.rounds_played;
        total_wounds_a += result.stats.wounds_dealt_a;
        total_wounds_b += result.stats.wounds_dealt_b;
        total_kills_a += result.stats.models_killed_a;
        total_kills_b += result.stats.models_killed_b;
        total_obj_rounds_a += result.stats.rounds_holding_a;
        total_obj_rounds_b += result.stats.rounds_holding_b;

        // Update progress every 1000 games
        if ((i + 1) % 1000 == 0) {
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
            f64 rate = (i + 1) * 1000.0 / elapsed_ms;
            progress.update(i + 1, NUM_GAMES, rate);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    progress.finish();
    std::cout << std::endl;

    // Print results
    std::cout << "Results (" << NUM_GAMES << " games):" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  " << assault.name.c_str() << " Win Rate: " << (100.0 * unit_a_wins / NUM_GAMES) << "%" << std::endl;
    std::cout << "  " << terminators.name.c_str() << " Win Rate: " << (100.0 * unit_b_wins / NUM_GAMES) << "%" << std::endl;
    std::cout << "  Draw Rate: " << (100.0 * draws / NUM_GAMES) << "%" << std::endl;
    std::cout << std::endl;
    std::cout << "  Avg Rounds Played: " << (1.0 * total_rounds / NUM_GAMES) << std::endl;
    std::cout << std::endl;
    std::cout << "Combat Stats (per game average):" << std::endl;
    std::cout << "  " << assault.name.c_str() << ":" << std::endl;
    std::cout << "    Wounds Dealt: " << (1.0 * total_wounds_a / NUM_GAMES) << std::endl;
    std::cout << "    Models Killed: " << (1.0 * total_kills_a / NUM_GAMES) << std::endl;
    std::cout << "    Rounds Holding Objective: " << (1.0 * total_obj_rounds_a / NUM_GAMES) << std::endl;
    std::cout << "  " << terminators.name.c_str() << ":" << std::endl;
    std::cout << "    Wounds Dealt: " << (1.0 * total_wounds_b / NUM_GAMES) << std::endl;
    std::cout << "    Models Killed: " << (1.0 * total_kills_b / NUM_GAMES) << std::endl;
    std::cout << "    Rounds Holding Objective: " << (1.0 * total_obj_rounds_b / NUM_GAMES) << std::endl;
    std::cout << std::endl;

    f64 games_per_sec = NUM_GAMES * 1000.0 / duration.count();
    std::cout << "Performance: " << std::fixed << std::setprecision(0)
              << games_per_sec << " games/second" << std::endl;
}

int main(int argc, char* argv[]) {
    print_banner();
    print_system_info();

    // TODO: Parse command line arguments
    // TODO: Load units from file
    // TODO: Configure simulation parameters

    // For now, run demo
    run_demo_simulation();

    return 0;
}
