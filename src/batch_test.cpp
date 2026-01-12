// ==============================================================================
// Batch Test - Run multiple matchups and export detailed results for querying
// ==============================================================================
//
// Usage:
//   ./batch_test <input_file> [options]
//
// Options:
//   --output <dir>       Output directory for CSV files (default: ./batch_test_results)
//   --iterations <N>     Override iterations for all matchups
//   --matchup <name>     Run only the named matchup
//   --verbose            Print progress during execution
//   -h, --help           Show help
//
// Input format:
//   Matchup_name: <test_name>
//
//   <Unit A definition>
//   <Unit A weapons>
//
//   <Unit B definition>
//   <Unit B weapons>
//

#include "parser/unit_parser.hpp"
#include "engine/game_runner.hpp"
#include "engine/dice.hpp"
#include "export/structured_match_logger.hpp"
#include "export/test_file_parser.hpp"
#include "export/batch_test_exporter.hpp"
#include <iostream>
#include <chrono>
#include <string>
#include <cstring>

using namespace battle;

void print_usage(const char* prog) {
    std::cout << "Batch Test - Run multiple matchups with detailed logging\n\n";
    std::cout << "Usage: " << prog << " <input_file> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --output <dir>       Output directory for CSV files (default: ./batch_test_results)\n";
    std::cout << "  --iterations <N>     Override iterations for all matchups\n";
    std::cout << "  --matchup <name>     Run only the named matchup\n";
    std::cout << "  --verbose            Print progress during execution\n";
    std::cout << "  -h, --help           Show this help\n\n";
    std::cout << "Input format:\n";
    std::cout << "  Matchup_name: <test_name>\n";
    std::cout << "\n";
    std::cout << "  <Unit A definition>\n";
    std::cout << "  <Unit A weapons>\n";
    std::cout << "\n";
    std::cout << "  <Unit B definition>\n";
    std::cout << "  <Unit B weapons>\n\n";
    std::cout << "Example:\n";
    std::cout << "  " << prog << " test_matchups.txt --output ./results --verbose\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    // Parse arguments
    std::string input_file;
    std::string output_dir = "./batch_test_results";
    int override_iterations = 0;
    std::string filter_matchup;
    bool verbose = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--output" && i + 1 < argc) {
            output_dir = argv[++i];
        } else if (arg == "--iterations" && i + 1 < argc) {
            override_iterations = std::stoi(argv[++i]);
        } else if (arg == "--matchup" && i + 1 < argc) {
            filter_matchup = argv[++i];
        } else if (arg == "--verbose") {
            verbose = true;
        } else if (arg[0] != '-' && input_file.empty()) {
            input_file = arg;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            return 1;
        }
    }

    if (input_file.empty()) {
        std::cerr << "Error: Input file required\n";
        print_usage(argv[0]);
        return 1;
    }

    // Parse input file
    if (verbose) {
        std::cout << "Parsing input file: " << input_file << "\n";
    }

    auto parse_result = TestFileParser::parse_file(input_file);

    if (!parse_result.errors.empty()) {
        std::cerr << "Parse errors:\n";
        for (const auto& error : parse_result.errors) {
            std::cerr << "  " << error << "\n";
        }
    }

    if (parse_result.matchups.empty()) {
        std::cerr << "Error: No matchups found in input file\n";
        return 1;
    }

    if (verbose) {
        std::cout << "Found " << parse_result.matchups.size() << " matchups\n";
    }

    // Process matchups
    std::vector<MatchupResult> results;
    int matchup_id = 1;

    auto total_start = std::chrono::high_resolution_clock::now();

    for (auto& matchup : parse_result.matchups) {
        // Apply filter if specified
        if (!filter_matchup.empty() && matchup.name != filter_matchup) {
            continue;
        }

        // Override iterations if specified
        if (override_iterations > 0) {
            matchup.iterations = override_iterations;
        }

        if (verbose) {
            std::cout << "\n[" << matchup_id << "] " << matchup.name
                      << " (" << matchup.iterations << " iterations)\n";
        }

        // Parse units
        auto unit_a_opt = UnitParser::parse_unit(matchup.unit_a_header, matchup.unit_a_weapons);
        if (!unit_a_opt) {
            std::cerr << "Error parsing Unit A for matchup '" << matchup.name << "'\n";
            std::cerr << "  Header: " << matchup.unit_a_header << "\n";
            std::cerr << "  Weapons: " << matchup.unit_a_weapons << "\n";
            continue;
        }

        auto unit_b_opt = UnitParser::parse_unit(matchup.unit_b_header, matchup.unit_b_weapons);
        if (!unit_b_opt) {
            std::cerr << "Error parsing Unit B for matchup '" << matchup.name << "'\n";
            std::cerr << "  Header: " << matchup.unit_b_header << "\n";
            std::cerr << "  Weapons: " << matchup.unit_b_weapons << "\n";
            continue;
        }

        Unit unit_a = std::move(*unit_a_opt);
        Unit unit_b = std::move(*unit_b_opt);

        // Create result entry
        MatchupResult result;
        result.matchup_id = matchup_id;
        result.definition = matchup;
        result.unit_a = unit_a;
        result.unit_b = unit_b;

        if (verbose) {
            std::cout << "  Unit A: " << unit_a.name.c_str()
                      << " [" << static_cast<int>(unit_a.model_count) << "]"
                      << " Q" << static_cast<int>(unit_a.quality) << "+"
                      << " D" << static_cast<int>(unit_a.defense) << "+\n";
            std::cout << "  Unit B: " << unit_b.name.c_str()
                      << " [" << static_cast<int>(unit_b.model_count) << "]"
                      << " Q" << static_cast<int>(unit_b.quality) << "+"
                      << " D" << static_cast<int>(unit_b.defense) << "+\n";
        }

        auto matchup_start = std::chrono::high_resolution_clock::now();

        // Run iterations
        for (int iter = 0; iter < matchup.iterations; ++iter) {
            // Determine seed
            uint64_t seed;
            if (matchup.seed.has_value()) {
                seed = *matchup.seed + iter;  // Vary seed per iteration but deterministically
            } else {
                seed = static_cast<uint64_t>(
                    std::chrono::high_resolution_clock::now().time_since_epoch().count());
            }

            // Create logger and runner
            StructuredMatchLogger logger;
            DiceRoller dice(seed);
            GameRunner runner(dice, &logger);

            // Run match
            runner.run_match(unit_a, unit_b);

            // Store result
            IterationResult iter_result;
            iter_result.matchup_id = matchup_id;
            iter_result.iteration = iter;
            iter_result.match_data = logger.get_match_data();

            result.iterations.push_back(std::move(iter_result));

            // Progress indicator
            if (verbose && matchup.iterations >= 10 && (iter + 1) % (matchup.iterations / 10) == 0) {
                std::cout << "  Progress: " << (iter + 1) << "/" << matchup.iterations << "\r" << std::flush;
            }
        }

        auto matchup_end = std::chrono::high_resolution_clock::now();
        auto matchup_duration = std::chrono::duration_cast<std::chrono::milliseconds>(matchup_end - matchup_start);

        if (verbose) {
            // Calculate win rate
            int wins_a = 0, wins_b = 0, draws = 0;
            for (const auto& iter : result.iterations) {
                if (iter.match_data.overall_winner == "A") wins_a++;
                else if (iter.match_data.overall_winner == "B") wins_b++;
                else draws++;
            }

            std::cout << "  Results: A=" << wins_a << " B=" << wins_b << " Draw=" << draws
                      << " (" << matchup_duration.count() << "ms)\n";
        }

        results.push_back(std::move(result));
        matchup_id++;
    }

    auto total_end = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(total_end - total_start);

    if (verbose) {
        std::cout << "\nCompleted " << results.size() << " matchups in "
                  << total_duration.count() << "ms\n";
    }

    // Export results
    if (verbose) {
        std::cout << "\nExporting results to: " << output_dir << "\n";
    }

    if (!BatchTestExporter::export_results(results, output_dir)) {
        std::cerr << "Error: Failed to export results\n";
        return 1;
    }

    if (verbose) {
        std::cout << "Exported files:\n";
        std::cout << "  " << output_dir << "/matchups.csv\n";
        std::cout << "  " << output_dir << "/iterations.csv\n";
        std::cout << "  " << output_dir << "/rounds.csv\n";
        std::cout << "  " << output_dir << "/movements.csv\n";
        std::cout << "  " << output_dir << "/morale.csv\n";
        std::cout << "  " << output_dir << "/attacks.csv\n";
        std::cout << "  " << output_dir << "/rule_triggers.csv\n";
        std::cout << "  " << output_dir << "/rolls.csv\n";
        std::cout << "  " << output_dir << "/spells.csv\n";
        std::cout << "  " << output_dir << "/spell_tokens.csv\n";
    }

    std::cout << "Results exported to: " << output_dir << "\n";

    return 0;
}
