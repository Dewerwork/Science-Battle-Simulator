/**
 * OPR Pipeline - Main Executable
 *
 * C++ port of run_opr_pipeline_all_units_v3_mt.py
 *
 * Processes OPR unit data from JSON files and generates loadout combinations.
 *
 * Usage:
 *   opr_pipeline <input_path> [options]
 *
 * Input can be:
 *   - A single JSON file (*_units.json from parse_pdf_loadouts.py)
 *   - A directory containing JSON files
 *
 * Options:
 *   -o <dir>    Output directory (default: ./pipeline_output)
 *   -r          Raw loadout mode (default) - each combo gets a UID
 *   -g          Grouped mode - Stage-1/Stage-2 reduction
 *   -l <limit>  Max loadouts per unit (0 = no limit, default)
 *   -w <n>      Workers per unit (default: 32)
 *   -t <n>      Tasks per unit (default: 256)
 *   -q          Quiet mode
 *   -h          Show help
 */

#include "pipeline/opr_pipeline.hpp"
#include <iostream>
#include <string>
#include <cstring>

using namespace battle::pipeline;

void print_usage(const char* prog) {
    std::cout << "OPR Pipeline - Unit Loadout Generator\n";
    std::cout << "C++ port of run_opr_pipeline_all_units_v3_mt.py\n\n";
    std::cout << "Usage: " << prog << " <input_path> [options]\n\n";
    std::cout << "Input can be:\n";
    std::cout << "  - A single JSON file (*_units.json from parse_pdf_loadouts.py)\n";
    std::cout << "  - A directory containing JSON files\n\n";
    std::cout << "Options:\n";
    std::cout << "  -o <dir>    Output directory (default: ./pipeline_output)\n";
    std::cout << "  -r          Raw loadout mode (default) - each combo gets a UID\n";
    std::cout << "  -g          Grouped mode - Stage-1/Stage-2 reduction\n";
    std::cout << "  -l <limit>  Max loadouts per unit (0 = no limit, default)\n";
    std::cout << "  -w <n>      Workers per unit (default: 32)\n";
    std::cout << "  -t <n>      Tasks per unit (default: 256)\n";
    std::cout << "  -q          Quiet mode\n";
    std::cout << "  -h          Show help\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << prog << " ./data/Blessed_Sisters_units.json\n";
    std::cout << "  " << prog << " ./army_data -o ./output -g\n";
    std::cout << "  " << prog << " ./factions -l 10000 -w 16\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2 || std::strcmp(argv[1], "-h") == 0 || std::strcmp(argv[1], "--help") == 0) {
        print_usage(argv[0]);
        return argc < 2 ? 1 : 0;
    }

    PipelineConfig config;
    config.input_path = argv[1];
    config.output_dir = "./pipeline_output";

    bool quiet = false;

    // Parse command line options
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-o" && i + 1 < argc) {
            config.output_dir = argv[++i];
        } else if (arg == "-r") {
            config.raw_loadout_mode = true;
        } else if (arg == "-g") {
            config.raw_loadout_mode = false;
        } else if (arg == "-l" && i + 1 < argc) {
            config.max_loadouts_per_unit = std::stoul(argv[++i]);
        } else if (arg == "-w" && i + 1 < argc) {
            config.workers_per_unit = std::stoul(argv[++i]);
        } else if (arg == "-t" && i + 1 < argc) {
            config.tasks_per_unit = std::stoul(argv[++i]);
        } else if (arg == "-q") {
            quiet = true;
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    if (!quiet) {
        std::cout << "=== OPR Pipeline ===\n\n";
        std::cout << "Input: " << config.input_path << "\n";
        std::cout << "Output: " << config.output_dir << "\n";
        std::cout << "Mode: " << (config.raw_loadout_mode ? "Raw Loadout" : "Grouped") << "\n";
        std::cout << "Workers per unit: " << config.workers_per_unit << "\n";
        std::cout << "Tasks per unit: " << config.tasks_per_unit << "\n";
        if (config.max_loadouts_per_unit > 0) {
            std::cout << "Max loadouts per unit: " << config.max_loadouts_per_unit << "\n";
        }
        std::cout << "\n";
    }

    OprPipeline pipeline(config);

    // Run the pipeline
    auto results = pipeline.run();

    // Summary
    size_t total_units = 0;
    size_t total_loadouts = 0;
    for (const auto& faction_result : results) {
        total_units += faction_result.total_units_processed;
        for (const auto& unit_result : faction_result.unit_results) {
            total_loadouts += config.raw_loadout_mode
                ? unit_result.raw_loadouts.size()
                : unit_result.total_groups;
        }
    }

    if (!quiet) {
        std::cout << "\n=== Summary ===\n";
        std::cout << "Factions processed: " << results.size() << "\n";
        std::cout << "Units processed: " << total_units << "\n";
        std::cout << "Total loadouts/groups: " << total_loadouts << "\n";
    }

    return 0;
}
