#include "parser/unit_parser.hpp"
#include "simulation/batch_simulator.hpp"
#include "analysis/result_analyzer.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>

using namespace battle;

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " <units_file> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -o <file>     Output results file (default: results.bin)\n";
    std::cout << "  -c <file>     Checkpoint file (default: checkpoint.bin)\n";
    std::cout << "  -b <size>     Batch size (default: 10000)\n";
    std::cout << "  -i <interval> Checkpoint interval (default: 1000000)\n";
    std::cout << "  -r            Resume from checkpoint if available\n";
    std::cout << "  -q            Quiet mode (no progress output)\n";
    std::cout << "  -h            Show this help\n\n";
    std::cout << "Example:\n";
    std::cout << "  " << prog << " units.txt -o faction_results.bin -b 50000\n";
    std::cout << "  " << prog << " units.txt -r   # Resume interrupted simulation\n";
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
        } else if (arg == "-r") {
            try_resume = true;
        } else if (arg == "-q") {
            quiet = true;
            config.enable_progress = false;
        }
    }

    // Load units
    std::cout << "=== Battle Simulator ===\n\n";
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
    f64 estimated_bytes = total_matchups * 8.0;  // 8 bytes per result

    std::cout << "\n--- Simulation Configuration ---\n";
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
