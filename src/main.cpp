/**
 * Battle Simulator - OPR Grimdark Future Combat Analysis
 *
 * A high-performance Monte Carlo simulation engine for analyzing
 * unit matchups in One Page Rules tabletop wargames.
 *
 * This is the primary entry point for running full game simulations
 * with movement, AI decision-making, and objective control.
 *
 * For large-scale batch processing (100B+ matchups), use batch_sim instead.
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
#include <cstring>
#include <algorithm>

using namespace battle;

// ==============================================================================
// CLI Configuration
// ==============================================================================

struct SimConfig {
    std::string units_file;
    std::string unit_a_name;
    std::string unit_b_name;
    u64 num_games = 10000;
    bool run_all_matchups = false;
    bool verbose = false;
    bool show_help = false;
    bool demo_mode = false;
};

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
 Full Game Simulation with Movement & Objectives
)" << std::endl;
}

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -f <file>      Units file to load\n";
    std::cout << "  -a <name>      Unit A name (for single matchup)\n";
    std::cout << "  -b <name>      Unit B name (for single matchup)\n";
    std::cout << "  -n <count>     Number of games to simulate (default: 10000)\n";
    std::cout << "  -A             Run all matchups (all units vs all units)\n";
    std::cout << "  -v             Verbose output\n";
    std::cout << "  -d             Run demo with built-in units\n";
    std::cout << "  -h             Show this help\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << prog << " -d                           # Run demo simulation\n";
    std::cout << "  " << prog << " -f units.txt -A              # All matchups from file\n";
    std::cout << "  " << prog << " -f units.txt -a \"Marines\" -b \"Orks\" -n 50000\n";
    std::cout << "\nGame Rules:\n";
    std::cout << "  - Units start 24\" apart (12\" from center)\n";
    std::cout << "  - Objective at center, control within 3\"\n";
    std::cout << "  - 4 rounds maximum\n";
    std::cout << "  - Winner: unit controlling objective at end\n";
    std::cout << "\nFor large-scale batch processing (100B+ matchups), use batch_sim.\n";
}

SimConfig parse_args(int argc, char* argv[]) {
    SimConfig config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            config.show_help = true;
        } else if (arg == "-d" || arg == "--demo") {
            config.demo_mode = true;
        } else if (arg == "-v" || arg == "--verbose") {
            config.verbose = true;
        } else if (arg == "-A" || arg == "--all") {
            config.run_all_matchups = true;
        } else if (arg == "-f" && i + 1 < argc) {
            config.units_file = argv[++i];
        } else if (arg == "-a" && i + 1 < argc) {
            config.unit_a_name = argv[++i];
        } else if (arg == "-b" && i + 1 < argc) {
            config.unit_b_name = argv[++i];
        } else if (arg == "-n" && i + 1 < argc) {
            config.num_games = std::stoull(argv[++i]);
        }
    }

    // Default to demo mode if no file specified and no help requested
    if (config.units_file.empty() && !config.show_help) {
        config.demo_mode = true;
    }

    return config;
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

// ==============================================================================
// Simulation Functions
// ==============================================================================

const char* ai_type_str(AIType t) {
    switch (t) {
        case AIType::Melee: return "Melee";
        case AIType::Shooting: return "Shooting";
        case AIType::Hybrid: return "Hybrid";
        default: return "Unknown";
    }
}

void print_unit_info(const Unit& unit) {
    std::cout << "  " << unit.name.c_str() << ": " << (int)unit.model_count << " models, "
              << unit.points_cost << " pts, AI: " << ai_type_str(unit.ai_type) << std::endl;
}

void run_matchup(const Unit& unit_a, const Unit& unit_b, u64 num_games, bool verbose) {
    std::cout << "\nMatchup: " << unit_a.name.c_str() << " vs " << unit_b.name.c_str() << std::endl;
    print_unit_info(unit_a);
    print_unit_info(unit_b);
    std::cout << std::endl;

    if (verbose) {
        std::cout << "Game Rules:" << std::endl;
        std::cout << "  - Units start 24\" apart (12\" from center)" << std::endl;
        std::cout << "  - Objective at center, control within 3\"" << std::endl;
        std::cout << "  - 4 rounds maximum" << std::endl;
        std::cout << "  - Winner: unit controlling objective at end" << std::endl;
        std::cout << std::endl;
    }

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

    for (u64 i = 0; i < num_games; ++i) {
        GameResult result = runner.run_game(unit_a, unit_b);

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
            progress.update(i + 1, num_games, rate);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    progress.finish();
    std::cout << std::endl;

    // Print results
    std::cout << "Results (" << num_games << " games):" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  " << unit_a.name.c_str() << " Win Rate: " << (100.0 * unit_a_wins / num_games) << "%" << std::endl;
    std::cout << "  " << unit_b.name.c_str() << " Win Rate: " << (100.0 * unit_b_wins / num_games) << "%" << std::endl;
    std::cout << "  Draw Rate: " << (100.0 * draws / num_games) << "%" << std::endl;
    std::cout << std::endl;
    std::cout << "  Avg Rounds Played: " << (1.0 * total_rounds / num_games) << std::endl;

    if (verbose) {
        std::cout << std::endl;
        std::cout << "Combat Stats (per game average):" << std::endl;
        std::cout << "  " << unit_a.name.c_str() << ":" << std::endl;
        std::cout << "    Wounds Dealt: " << (1.0 * total_wounds_a / num_games) << std::endl;
        std::cout << "    Models Killed: " << (1.0 * total_kills_a / num_games) << std::endl;
        std::cout << "    Rounds Holding Objective: " << (1.0 * total_obj_rounds_a / num_games) << std::endl;
        std::cout << "  " << unit_b.name.c_str() << ":" << std::endl;
        std::cout << "    Wounds Dealt: " << (1.0 * total_wounds_b / num_games) << std::endl;
        std::cout << "    Models Killed: " << (1.0 * total_kills_b / num_games) << std::endl;
        std::cout << "    Rounds Holding Objective: " << (1.0 * total_obj_rounds_b / num_games) << std::endl;
    }

    f64 games_per_sec = num_games * 1000.0 / duration.count();
    std::cout << "\nPerformance: " << std::fixed << std::setprecision(0)
              << games_per_sec << " games/second" << std::endl;
}

void run_all_matchups(const std::vector<Unit>& units, u64 games_per_matchup, bool verbose) {
    std::cout << "\nRunning all matchups (" << units.size() << " units, "
              << (units.size() * units.size()) << " total matchups)..." << std::endl;

    // Results matrix
    struct MatchupResult {
        f64 win_rate_a;
        f64 avg_rounds;
    };
    std::vector<std::vector<MatchupResult>> results(units.size(),
        std::vector<MatchupResult>(units.size()));

    DiceRoller dice;
    GameRunner runner(dice);

    u64 total_matchups = units.size() * units.size();
    u64 completed = 0;
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < units.size(); ++i) {
        for (size_t j = 0; j < units.size(); ++j) {
            u64 a_wins = 0;
            u64 total_rounds = 0;

            for (u64 g = 0; g < games_per_matchup; ++g) {
                GameResult result = runner.run_game(units[i], units[j]);
                if (result.winner == GameWinner::UnitA) a_wins++;
                total_rounds += result.rounds_played;
            }

            results[i][j].win_rate_a = 100.0 * a_wins / games_per_matchup;
            results[i][j].avg_rounds = 1.0 * total_rounds / games_per_matchup;

            completed++;
            if (completed % 10 == 0 || completed == total_matchups) {
                auto now = std::chrono::high_resolution_clock::now();
                auto elapsed = std::chrono::duration<f64>(now - start).count();
                f64 rate = completed / elapsed;
                f64 eta = (total_matchups - completed) / rate;
                std::cout << "\r  Progress: " << completed << "/" << total_matchups
                          << " (" << std::fixed << std::setprecision(1)
                          << (100.0 * completed / total_matchups) << "%) "
                          << std::setprecision(0) << rate << " matchups/s"
                          << " ETA: " << std::setprecision(0) << eta << "s   " << std::flush;
            }
        }
    }

    std::cout << "\n\n";

    // Print results matrix
    std::cout << "Win Rate Matrix (row vs column):" << std::endl;
    std::cout << std::setw(20) << "";
    for (const auto& unit : units) {
        std::cout << std::setw(12) << unit.name.c_str();
    }
    std::cout << std::endl;

    for (size_t i = 0; i < units.size(); ++i) {
        std::cout << std::setw(20) << units[i].name.c_str();
        for (size_t j = 0; j < units.size(); ++j) {
            std::cout << std::setw(11) << std::fixed << std::setprecision(1)
                      << results[i][j].win_rate_a << "%";
        }
        std::cout << std::endl;
    }

    // Calculate overall win rates
    std::cout << "\nOverall Rankings:" << std::endl;
    std::vector<std::pair<size_t, f64>> rankings;
    for (size_t i = 0; i < units.size(); ++i) {
        f64 total_win_rate = 0;
        for (size_t j = 0; j < units.size(); ++j) {
            if (i != j) total_win_rate += results[i][j].win_rate_a;
        }
        rankings.emplace_back(i, total_win_rate / (units.size() - 1));
    }

    std::sort(rankings.begin(), rankings.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    for (size_t rank = 0; rank < rankings.size(); ++rank) {
        auto [idx, win_rate] = rankings[rank];
        std::cout << "  " << (rank + 1) << ". " << units[idx].name.c_str()
                  << " (" << units[idx].points_cost << " pts): "
                  << std::fixed << std::setprecision(1) << win_rate << "% avg win rate" << std::endl;
    }
}

void run_demo_simulation(const SimConfig& config) {
    std::cout << "Running demo simulation (with movement & objectives)..." << std::endl;

    // Parse demo units
    auto parse_result = UnitParser::parse_string(DEMO_UNITS, "Demo");
    if (parse_result.units.size() < 2) {
        std::cerr << "Failed to parse demo units!" << std::endl;
        return;
    }

    run_matchup(parse_result.units[0], parse_result.units[1], config.num_games, config.verbose);
}

const Unit* find_unit_by_name(const std::vector<Unit>& units, const std::string& name) {
    for (const auto& unit : units) {
        if (unit.name.view() == name) {
            return &unit;
        }
    }
    // Try partial match
    for (const auto& unit : units) {
        if (unit.name.view().find(name) != std::string_view::npos) {
            return &unit;
        }
    }
    return nullptr;
}

int main(int argc, char* argv[]) {
    SimConfig config = parse_args(argc, argv);

    print_banner();

    if (config.show_help) {
        print_usage(argv[0]);
        return 0;
    }

    print_system_info();

    // Initialize faction rules
    initialize_faction_rules();

    // Demo mode
    if (config.demo_mode) {
        run_demo_simulation(config);
        return 0;
    }

    // Load units from file
    std::cout << "Loading units from: " << config.units_file << std::endl;
    auto parse_result = UnitParser::parse_file(config.units_file);

    if (parse_result.units.empty()) {
        std::cerr << "Error: No units loaded from " << config.units_file << std::endl;
        return 1;
    }

    std::cout << "Loaded " << parse_result.units.size() << " units" << std::endl;

    if (!parse_result.errors.empty()) {
        std::cout << "Warnings: " << parse_result.errors.size() << " parse errors" << std::endl;
        if (config.verbose) {
            for (const auto& err : parse_result.errors) {
                std::cerr << "  - " << err << std::endl;
            }
        }
    }

    // List units if verbose
    if (config.verbose) {
        std::cout << "\nUnits:" << std::endl;
        for (const auto& unit : parse_result.units) {
            print_unit_info(unit);
        }
    }

    // Run all matchups mode
    if (config.run_all_matchups) {
        run_all_matchups(parse_result.units, config.num_games, config.verbose);
        return 0;
    }

    // Single matchup mode
    if (!config.unit_a_name.empty() && !config.unit_b_name.empty()) {
        const Unit* unit_a = find_unit_by_name(parse_result.units, config.unit_a_name);
        const Unit* unit_b = find_unit_by_name(parse_result.units, config.unit_b_name);

        if (!unit_a) {
            std::cerr << "Error: Unit not found: " << config.unit_a_name << std::endl;
            return 1;
        }
        if (!unit_b) {
            std::cerr << "Error: Unit not found: " << config.unit_b_name << std::endl;
            return 1;
        }

        run_matchup(*unit_a, *unit_b, config.num_games, config.verbose);
        return 0;
    }

    // No specific matchup requested - show available units and prompt
    std::cout << "\nAvailable units:" << std::endl;
    for (size_t i = 0; i < parse_result.units.size(); ++i) {
        std::cout << "  " << (i + 1) << ". " << parse_result.units[i].name.c_str()
                  << " (" << parse_result.units[i].points_cost << " pts)" << std::endl;
    }
    std::cout << "\nUse -a and -b flags to specify units, or -A for all matchups." << std::endl;
    std::cout << "Example: " << argv[0] << " -f " << config.units_file
              << " -a \"" << parse_result.units[0].name.c_str() << "\""
              << " -b \"" << parse_result.units[std::min(size_t(1), parse_result.units.size()-1)].name.c_str() << "\"" << std::endl;

    return 0;
}
