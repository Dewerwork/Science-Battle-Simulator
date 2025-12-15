#include "parser/unit_parser.hpp"
#include "simulation/batch_simulator.hpp"
#include "core/faction_rules.hpp"
#include <iostream>
#include <iomanip>

using namespace battle;

int main(int argc, char* argv[]) {
    // Initialize faction rules
    initialize_faction_rules();

    std::cout << "=== Battle Simulator Benchmark ===\n\n";

    // Parse command line
    std::string unit_file = "";
    size_t num_matchups = 10000;

    if (argc > 1) {
        unit_file = argv[1];
    }
    if (argc > 2) {
        num_matchups = std::stoul(argv[2]);
    }

    std::vector<Unit> units;

    // Load units from file or use sample data
    if (!unit_file.empty()) {
        std::cout << "Loading units from: " << unit_file << "\n";
        auto result = UnitParser::parse_file(unit_file);
        units = std::move(result.units);
        std::cout << "Loaded " << units.size() << " units\n";
    } else {
        // Create sample units for benchmarking
        std::cout << "Using sample units (pass a unit file for real data)\n";

        std::string sample = R"(
Assault Walker [1] Q4+ D2+ | 350pts | Devout, Fear(2), Fearless, Regeneration, Tough(9)
Stomp (A3, AP(1)), Heavy Claw (A4, AP(1), Rending), Heavy Fist (A4, AP(4))

Battle Sisters [5] Q4+ D4+ | 100pts | Devout
5x CCWs (A5), 5x 24" Rifles (A5)

APC [1] Q4+ D2+ | 175pts | Devout, Impact(3), Tough(6)
24" Storm Rifle (A3, AP(1))

Assault Sisters [5] Q4+ D4+ | 195pts | Devout
5x Energy Swords (A10, AP(1), Rending), 5x 12" Heavy Pistols (A5, AP(1))
)";

        auto result = UnitParser::parse_string(sample, "Test");
        units = std::move(result.units);
        std::cout << "Created " << units.size() << " sample units\n";
    }

    if (units.empty()) {
        std::cout << "No units to simulate!\n";
        return 1;
    }

    // Run benchmark
    std::cout << "\n--- Benchmarking " << num_matchups << " matchups ---\n";
    benchmark_simulation(units, num_matchups);

    // Also test the BatchSimulator with progress reporting
    std::cout << "\n--- Testing BatchSimulator with progress ---\n";

    BatchConfig config;
    config.batch_size = 1000;
    config.output_file = "/tmp/benchmark_results.bin";
    config.checkpoint_file = "/tmp/benchmark_checkpoint.bin";

    BatchSimulator sim(config);

    auto progress_cb = [](const ProgressInfo& info) {
        std::cout << "\r  Progress: " << info.completed << "/" << info.total
                  << " (" << std::fixed << std::setprecision(1)
                  << (100.0 * info.completed / info.total) << "%) "
                  << std::setprecision(0) << info.matchups_per_second << " matchups/sec"
                  << std::flush;
    };

    // Simulate all pairs (n^2 matchups)
    size_t max_units = std::min(units.size(), size_t(50));  // Cap for benchmark
    std::vector<Unit> subset(units.begin(), units.begin() + max_units);

    std::cout << "Simulating " << (max_units * max_units) << " matchups ("
              << max_units << " x " << max_units << ")\n";

    sim.simulate_all(subset, subset, progress_cb);

    std::cout << "\n\nBenchmark complete!\n";
    std::cout << "Results written to: " << config.output_file << "\n";

    return 0;
}
