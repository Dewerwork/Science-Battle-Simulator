/**
 * Batch Simulator - Large-Scale Matchup Processing
 *
 * Designed for running 100 billion+ matchup simulations with:
 * - Parallel processing across all CPU cores
 * - Checkpoint/resume support for long-running jobs
 * - Compact 8-byte result format for efficient storage
 *
 * For interactive simulation with fewer matchups, use battle_sim instead.
 */

#include "parser/unit_parser.hpp"
#include "simulation/batch_simulator.hpp"
#include "analysis/result_analyzer.hpp"
#include "core/faction_rules.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>

using namespace battle;

void print_usage(const char* prog) {
    std::cout << "Batch Simulator - Large-Scale Matchup Processing\n";
    std::cout << "For 100B+ matchups with checkpoint/resume support.\n\n";
    std::cout << "Usage: " << prog << " <units_file> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -o <file>     Output results file (default: results.bin)\n";
    std::cout << "  -c <file>     Checkpoint file (default: checkpoint.bin)\n";
    std::cout << "  -b <size>     Batch size (default: 10000)\n";
    std::cout << "  -i <interval> Checkpoint interval (default: 1000000)\n";
    std::cout << "  -e            Extended format - full game statistics (24 bytes/result)\n";
    std::cout << "  -E            Compact extended - compressed game stats (16 bytes/result)\n";
    std::cout << "  -A            Aggregated format - per-unit summary stats (256 bytes/unit)\n";
    std::cout << "                Massive file size reduction: ~5MB vs ~5GB for extended\n";
    std::cout << "                Default compact format uses 8 bytes/result\n";
    std::cout << "  -r            Resume from checkpoint if available\n";
    std::cout << "  -q            Quiet mode (no progress output)\n";
    std::cout << "  -h            Show this help\n\n";
    std::cout << "Output Formats:\n";
    std::cout << "  (default)     8 bytes/result  - win/loss only\n";
    std::cout << "  -E            16 bytes/result - game stats (wounds, kills, objectives)\n";
    std::cout << "  -e            24 bytes/result - full precision game stats\n";
    std::cout << "  -A            256 bytes/unit  - comprehensive per-unit aggregated stats\n\n";
    std::cout << "Example:\n";
    std::cout << "  " << prog << " units.txt -o faction_results.bin -b 50000\n";
    std::cout << "  " << prog << " units.txt -E -o extended_results.bin  # Compact extended\n";
    std::cout << "  " << prog << " units.txt -e -o full_results.bin      # Full extended\n";
    std::cout << "  " << prog << " units.txt -A -o summary.bin           # Aggregated stats\n";
    std::cout << "  " << prog << " units.txt -r   # Resume interrupted simulation\n";
    std::cout << "\nFor interactive simulation, use battle_sim instead.\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2 || std::string(argv[1]) == "-h") {
        print_usage(argv[0]);
        return argc < 2 ? 1 : 0;
    }

    // Parse command line
    std::string unit_file = argv[1];
    BatchConfig config;
    config.output_file = "results.bin";
    config.checkpoint_file = "checkpoint.bin";
    config.batch_size = 10000;
    config.checkpoint_interval = 1000000;
    bool quiet = false;
    bool try_resume = false;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            config.output_file = argv[++i];
        } else if (arg == "-c" && i + 1 < argc) {
            config.checkpoint_file = argv[++i];
        } else if (arg == "-b" && i + 1 < argc) {
            config.batch_size = std::stoul(argv[++i]);
        } else if (arg == "-i" && i + 1 < argc) {
            config.checkpoint_interval = std::stoul(argv[++i]);
        } else if (arg == "-e") {
            config.format = ResultFormat::Extended;
        } else if (arg == "-E") {
            config.format = ResultFormat::CompactExtended;
        } else if (arg == "-A") {
            config.format = ResultFormat::Aggregated;
        } else if (arg == "-r") {
            try_resume = true;
        } else if (arg == "-q") {
            quiet = true;
            config.enable_progress = false;
        }
    }

    // Initialize faction rules
    initialize_faction_rules();

    // Load units
    std::cout << "=== Batch Simulator ===\n\n";
    std::cout << "Loading units from: " << unit_file << "\n";

    auto start_load = std::chrono::high_resolution_clock::now();
    auto parse_result = UnitParser::parse_file(unit_file);
    auto end_load = std::chrono::high_resolution_clock::now();
    f64 load_time = std::chrono::duration<f64>(end_load - start_load).count();

    if (parse_result.units.empty()) {
        std::cerr << "Error: Failed to load units from " << unit_file << "\n";
        return 1;
    }

    std::cout << "Loaded " << parse_result.units.size() << " units in "
              << std::fixed << std::setprecision(2) << load_time << "s\n";

    if (!parse_result.errors.empty()) {
        std::cout << "Warnings: " << parse_result.errors.size() << " parse errors\n";
    }

    // Calculate simulation size
    u64 total_matchups = static_cast<u64>(parse_result.units.size()) * parse_result.units.size();
    f64 bytes_per_result = static_cast<f64>(config.result_size());
    f64 estimated_bytes;

    // For aggregated format, size is per-unit, not per-matchup
    if (config.format == ResultFormat::Aggregated) {
        estimated_bytes = parse_result.units.size() * bytes_per_result + 16; // units * 256 + header
    } else {
        estimated_bytes = total_matchups * bytes_per_result;
    }

    // Format name for display
    const char* format_name = "Compact (8 bytes)";
    switch (config.format) {
        case ResultFormat::Compact:
            format_name = "Compact (8 bytes - win/loss only)";
            break;
        case ResultFormat::Extended:
            format_name = "Extended (24 bytes - full game stats)";
            break;
        case ResultFormat::CompactExtended:
            format_name = "Compact Extended (16 bytes - compressed game stats)";
            break;
        case ResultFormat::Aggregated:
            format_name = "Aggregated (256 bytes/unit - comprehensive per-unit stats)";
            break;
    }

    std::cout << "\n--- Simulation Configuration ---\n";
    std::cout << "Simulation Mode: Full Game (movement, AI, objectives, 4 rounds max)\n";
    std::cout << "Result Format: " << format_name << "\n";
    std::cout << "Units: " << parse_result.units.size() << "\n";
    std::cout << "Total matchups: " << total_matchups;
    if (total_matchups >= 1e9) {
        std::cout << " (" << (total_matchups / 1e9) << " billion)";
    } else if (total_matchups >= 1e6) {
        std::cout << " (" << (total_matchups / 1e6) << " million)";
    }
    std::cout << "\n";
    std::cout << "Estimated output size: ";
    if (estimated_bytes >= 1e12) {
        std::cout << (estimated_bytes / 1e12) << " TB\n";
    } else if (estimated_bytes >= 1e9) {
        std::cout << (estimated_bytes / 1e9) << " GB\n";
    } else if (estimated_bytes >= 1e6) {
        std::cout << (estimated_bytes / 1e6) << " MB\n";
    } else {
        std::cout << (estimated_bytes / 1e3) << " KB\n";
    }
    std::cout << "Output file: " << config.output_file << "\n";
    std::cout << "Batch size: " << config.batch_size << "\n";
    std::cout << "Checkpoint interval: " << config.checkpoint_interval << "\n";

    // Create simulator
    BatchSimulator sim(config);
    std::cout << "Threads: " << sim.thread_count() << "\n";

    // Check for checkpoint to resume
    if (try_resume) {
        CheckpointData checkpoint = sim.check_checkpoint(
            parse_result.units.size(), parse_result.units.size());
        if (checkpoint.valid) {
            f64 percent = 100.0 * checkpoint.completed / checkpoint.total;
            std::cout << "\n*** RESUMING from checkpoint ***\n";
            std::cout << "  Previously completed: " << checkpoint.completed << "/" << checkpoint.total
                      << " (" << std::fixed << std::setprecision(1) << percent << "%)\n";
            std::cout << "  Remaining: " << (checkpoint.total - checkpoint.completed) << " matchups\n";
        } else {
            std::cout << "\nNo valid checkpoint found - starting fresh\n";
        }
    }

    // Track if we resumed for final stats
    u64 started_from = 0;
    bool did_resume = false;

    // Progress callback
    auto last_update = std::chrono::steady_clock::now();
    auto progress_cb = [&](const ProgressInfo& info) {
        // Track resume state on first callback
        if (info.resumed && !did_resume) {
            did_resume = true;
            // Calculate where we started from
            // (first progress report shows current completed count)
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<f64>(now - last_update).count();

        // Update at most every 0.5 seconds
        if (elapsed < 0.5 && info.completed < info.total) return;
        last_update = now;

        f64 percent = 100.0 * info.completed / info.total;
        std::cout << "\r  ";
        if (info.resumed) std::cout << "[RESUMED] ";
        std::cout << "Progress: " << info.completed << "/" << info.total
                  << " (" << std::fixed << std::setprecision(1) << percent << "%) "
                  << std::setprecision(0) << info.matchups_per_second << " matchups/sec";

        if (info.estimated_remaining_seconds > 0) {
            u64 remaining = static_cast<u64>(info.estimated_remaining_seconds);
            if (remaining >= 86400) {
                std::cout << " | ETA: " << (remaining / 86400) << "d "
                          << ((remaining % 86400) / 3600) << "h";
            } else if (remaining >= 3600) {
                std::cout << " | ETA: " << (remaining / 3600) << "h "
                          << ((remaining % 3600) / 60) << "m";
            } else if (remaining >= 60) {
                std::cout << " | ETA: " << (remaining / 60) << "m "
                          << (remaining % 60) << "s";
            } else {
                std::cout << " | ETA: " << remaining << "s";
            }
        }
        std::cout << "      " << std::flush;
    };

    // Run simulation
    std::cout << "\n\n--- Running Simulation ---\n";
    auto start_sim = std::chrono::high_resolution_clock::now();

    try {
        if (quiet) {
            sim.simulate_all(parse_result.units, parse_result.units, nullptr, try_resume);
        } else {
            sim.simulate_all(parse_result.units, parse_result.units, progress_cb, try_resume);
        }
    } catch (const std::exception& e) {
        std::cerr << "\nError during simulation: " << e.what() << "\n";
        return 1;
    }

    auto end_sim = std::chrono::high_resolution_clock::now();
    f64 sim_time = std::chrono::duration<f64>(end_sim - start_sim).count();

    // Print summary
    std::cout << "\n\n--- Simulation Complete ---\n";
    std::cout << "Session time: ";
    if (sim_time >= 86400) {
        std::cout << (sim_time / 86400) << " days\n";
    } else if (sim_time >= 3600) {
        std::cout << (sim_time / 3600) << " hours\n";
    } else if (sim_time >= 60) {
        std::cout << (sim_time / 60) << " minutes\n";
    } else {
        std::cout << sim_time << " seconds\n";
    }
    std::cout << "Results saved to: " << config.output_file << "\n";

    // Print full game simulation stats
    const auto& game_stats = sim.game_stats();
    std::cout << "\n--- Full Game Simulation Stats ---\n";
    std::cout << "  Total games played: " << game_stats.total_games_played.load() << "\n";
    std::cout << "  Avg wounds per game: " << std::fixed << std::setprecision(2)
              << game_stats.avg_wounds_per_game() << "\n";
    std::cout << "  Avg models killed per game: " << std::fixed << std::setprecision(2)
              << game_stats.avg_models_killed_per_game() << "\n";
    std::cout << "  Total objective rounds: " << game_stats.total_objective_rounds.load() << "\n";
    std::cout << "  Games with objective control: " << std::fixed << std::setprecision(1)
              << game_stats.objective_game_percent() << "%\n";

    // Quick analysis
    std::cout << "\n--- Quick Analysis ---\n";
    ResultAnalyzer analyzer;
    if (analyzer.load_results(config.output_file)) {
        std::cout << analyzer.generate_summary_report();

        // Top 10 units
        std::cout << "\nTop 10 Units by Win Rate:\n";
        auto top = analyzer.get_top_units(10, 3);
        for (size_t i = 0; i < top.size(); ++i) {
            const auto& [id, stats] = top[i];
            if (id < parse_result.units.size()) {
                std::cout << "  " << (i + 1) << ". " << parse_result.units[id].name.view()
                          << " (" << parse_result.units[id].points_cost << "pts) - "
                          << std::fixed << std::setprecision(1) << stats.win_rate() << "% win rate\n";
            }
        }
    }

    std::cout << "\nUse 'analyze_results' tool for detailed analysis.\n";
    return 0;
}
