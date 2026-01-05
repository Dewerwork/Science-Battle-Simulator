// ==============================================================================
// export_game - Export complete game data for validation and debugging
// ==============================================================================
//
// Usage:
//   ./export_game -a <unit_a.txt> -b <unit_b.txt> [options]
//
// Options:
//   -a, --unit-a <file>    Unit A text file (required)
//   -b, --unit-b <file>    Unit B text file (required)
//   -o, --output <path>    Output file path (without extension)
//   -s, --seed <number>    Random seed for reproducibility
//   --json-only            Only output JSON format
//   --text-only            Only output text format
//   -h, --help             Show this help message
//
// Example:
//   ./export_game -a hive_lord.txt -b battle_sisters.txt -o game_001 -s 12345
//
// ==============================================================================

#include "parser/unit_parser.hpp"
#include "export/export_game_runner.hpp"
#include "export/json_formatter.hpp"
#include "export/text_formatter.hpp"
#include "core/faction_rules.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <ctime>

using namespace battle;
using namespace battle::export_data;

std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " -a <unit_a.txt> -b <unit_b.txt> [options]\n\n";
    std::cout << "Export complete game data for validation and debugging.\n\n";
    std::cout << "Options:\n";
    std::cout << "  -a, --unit-a <file>    Unit A text file (required)\n";
    std::cout << "  -b, --unit-b <file>    Unit B text file (required)\n";
    std::cout << "  -o, --output <path>    Output file path (without extension)\n";
    std::cout << "                         Default: game_export\n";
    std::cout << "  -s, --seed <number>    Random seed for reproducibility\n";
    std::cout << "                         Default: current time\n";
    std::cout << "  --json-only            Only output JSON format\n";
    std::cout << "  --text-only            Only output text format\n";
    std::cout << "  -h, --help             Show this help message\n";
    std::cout << "\n";
    std::cout << "Unit file format:\n";
    std::cout << "  Unit Name [count] Q#+ D#+ | pts | Rules\n";
    std::cout << "  Weapon (A#, ...), ...\n";
    std::cout << "\n";
    std::cout << "Example:\n";
    std::cout << "  " << program_name << " -a hive_lord.txt -b sisters.txt -o game_001 -s 12345\n";
    std::cout << "\n";
    std::cout << "Output files:\n";
    std::cout << "  <path>.json   Complete JSON export with all dice rolls\n";
    std::cout << "  <path>.txt    Human-readable game report\n";
}

int main(int argc, char* argv[]) {
    // Default options
    std::string output_path = "game_export";
    std::string unit_a_file;
    std::string unit_b_file;
    u64 seed = static_cast<u64>(std::time(nullptr));
    bool output_json = true;
    bool output_text = true;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (std::strcmp(argv[i], "-a") == 0 || std::strcmp(argv[i], "--unit-a") == 0) {
            if (i + 1 < argc) {
                unit_a_file = argv[++i];
            } else {
                std::cerr << "Error: -a requires an argument\n";
                return 1;
            }
        } else if (std::strcmp(argv[i], "-b") == 0 || std::strcmp(argv[i], "--unit-b") == 0) {
            if (i + 1 < argc) {
                unit_b_file = argv[++i];
            } else {
                std::cerr << "Error: -b requires an argument\n";
                return 1;
            }
        } else if (std::strcmp(argv[i], "-o") == 0 || std::strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) {
                output_path = argv[++i];
            } else {
                std::cerr << "Error: -o requires an argument\n";
                return 1;
            }
        } else if (std::strcmp(argv[i], "-s") == 0 || std::strcmp(argv[i], "--seed") == 0) {
            if (i + 1 < argc) {
                seed = std::strtoull(argv[++i], nullptr, 10);
            } else {
                std::cerr << "Error: -s requires an argument\n";
                return 1;
            }
        } else if (std::strcmp(argv[i], "--json-only") == 0) {
            output_json = true;
            output_text = false;
        } else if (std::strcmp(argv[i], "--text-only") == 0) {
            output_json = false;
            output_text = true;
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    // Validate required arguments
    if (unit_a_file.empty() || unit_b_file.empty()) {
        std::cerr << "Error: Both -a and -b unit files are required\n\n";
        print_usage(argv[0]);
        return 1;
    }

    // Initialize faction rules
    initialize_faction_rules();

    std::cout << "=== Battle Simulator Game Export ===\n\n";
    std::cout << "Seed: " << seed << "\n\n";

    // Read unit files
    std::string unit_a_def = read_file(unit_a_file);
    if (unit_a_def.empty()) {
        std::cerr << "Error: Could not read unit A file: " << unit_a_file << "\n";
        return 1;
    }

    std::string unit_b_def = read_file(unit_b_file);
    if (unit_b_def.empty()) {
        std::cerr << "Error: Could not read unit B file: " << unit_b_file << "\n";
        return 1;
    }

    std::cout << "Unit A file: " << unit_a_file << "\n";
    std::cout << "Unit B file: " << unit_b_file << "\n\n";

    // Parse units
    auto result_a = UnitParser::parse_string(unit_a_def, "Unknown");
    auto result_b = UnitParser::parse_string(unit_b_def, "Unknown");

    if (result_a.units.empty() || result_b.units.empty()) {
        std::cerr << "Error: Failed to parse units\n";
        return 1;
    }

    Unit& unit_a = result_a.units[0];
    Unit& unit_b = result_b.units[0];

    std::cout << "Unit A: " << unit_a.name.c_str()
              << " (" << unit_a.points_cost << "pts)"
              << " - " << static_cast<int>(unit_a.model_count) << " models"
              << " - AI: " << (unit_a.ai_type == AIType::Melee ? "Melee" :
                              unit_a.ai_type == AIType::Shooting ? "Shooting" : "Hybrid")
              << "\n";
    std::cout << "Unit B: " << unit_b.name.c_str()
              << " (" << unit_b.points_cost << "pts)"
              << " - " << static_cast<int>(unit_b.model_count) << " models"
              << " - AI: " << (unit_b.ai_type == AIType::Melee ? "Melee" :
                              unit_b.ai_type == AIType::Shooting ? "Shooting" : "Hybrid")
              << "\n\n";

    // Run game with export logging
    std::cout << "Running game with full export logging...\n";

    ExportGameRunner runner(seed);
    GameResult result = runner.run_game(unit_a, unit_b);

    const GameExport& export_data = runner.get_export();

    std::cout << "Game complete!\n";
    std::cout << "  Rounds played: " << static_cast<int>(result.rounds_played) << "\n";
    std::cout << "  Winner: ";
    switch (result.winner) {
        case GameWinner::UnitA: std::cout << unit_a.name.c_str(); break;
        case GameWinner::UnitB: std::cout << unit_b.name.c_str(); break;
        case GameWinner::Draw: std::cout << "DRAW"; break;
    }
    std::cout << "\n\n";

    // Write output files
    if (output_json) {
        std::string json_path = output_path + ".json";
        std::ofstream json_file(json_path);
        if (json_file.is_open()) {
            JsonFormatter::write(json_file, export_data);
            json_file.close();
            std::cout << "JSON export written to: " << json_path << "\n";
        } else {
            std::cerr << "Error: Could not open " << json_path << " for writing\n";
        }
    }

    if (output_text) {
        std::string text_path = output_path + ".txt";
        std::ofstream text_file(text_path);
        if (text_file.is_open()) {
            TextFormatter::write(text_file, export_data);
            text_file.close();
            std::cout << "Text report written to: " << text_path << "\n";
        } else {
            std::cerr << "Error: Could not open " << text_path << " for writing\n";
        }
    }

    std::cout << "\n=== Export complete ===\n";
    return 0;
}
