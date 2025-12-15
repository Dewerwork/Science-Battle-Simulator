#include "parser/unit_parser.hpp"
#include "analysis/result_analyzer.hpp"
#include <iostream>
#include <iomanip>

using namespace battle;

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " <command> [options]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  summary <results.bin>                    - Print summary report\n";
    std::cout << "  top <results.bin> <units.txt> [N]        - Show top N units (default 20)\n";
    std::cout << "  unit <results.bin> <units.txt> <id>      - Show stats for unit ID\n";
    std::cout << "  matchup <results.bin> <id_a> <id_b>      - Show matchup between two units\n";
    std::cout << "  csv-stats <results.bin> <units.txt> <out.csv>  - Export stats to CSV\n";
    std::cout << "  csv-matchups <results.bin> <out.csv>     - Export matchups to CSV\n";
    std::cout << "  json <results.bin> <units.txt>           - Export stats to JSON (stdout)\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    std::string command = argv[1];

    // Summary command
    if (command == "summary" && argc >= 3) {
        ResultAnalyzer analyzer;
        if (!analyzer.load_results(argv[2])) {
            std::cerr << "Failed to load results from: " << argv[2] << "\n";
            return 1;
        }
        std::cout << analyzer.generate_summary_report();
        return 0;
    }

    // Top units command
    if (command == "top" && argc >= 4) {
        ResultAnalyzer analyzer;
        if (!analyzer.load_results(argv[2])) {
            std::cerr << "Failed to load results from: " << argv[2] << "\n";
            return 1;
        }

        auto parse_result = UnitParser::parse_file(argv[3]);
        if (parse_result.units.empty()) {
            std::cerr << "Failed to load units from: " << argv[3] << "\n";
            return 1;
        }

        size_t n = (argc >= 5) ? std::stoul(argv[4]) : 20;
        auto top = analyzer.get_top_units(n, 3);

        std::cout << "=== Top " << n << " Units by Win Rate ===\n\n";
        std::cout << std::left << std::setw(5) << "Rank"
                  << std::setw(40) << "Unit Name"
                  << std::setw(8) << "Points"
                  << std::setw(10) << "Win Rate"
                  << std::setw(12) << "Matches"
                  << "\n";
        std::cout << std::string(75, '-') << "\n";

        for (size_t i = 0; i < top.size(); ++i) {
            const auto& [id, stats] = top[i];
            if (id < parse_result.units.size()) {
                const auto& unit = parse_result.units[id];
                std::string name_str(unit.name.view());
                if (name_str.size() > 38) name_str = name_str.substr(0, 38);
                std::cout << std::left << std::setw(5) << (i + 1)
                          << std::setw(40) << name_str
                          << std::setw(8) << unit.points_cost
                          << std::fixed << std::setprecision(1)
                          << std::setw(10) << (std::to_string((int)stats.win_rate()) + "%")
                          << std::setw(12) << stats.matches_played
                          << "\n";
            }
        }
        return 0;
    }

    // Unit stats command
    if (command == "unit" && argc >= 5) {
        ResultAnalyzer analyzer;
        if (!analyzer.load_results(argv[2])) {
            std::cerr << "Failed to load results from: " << argv[2] << "\n";
            return 1;
        }

        auto parse_result = UnitParser::parse_file(argv[3]);
        if (parse_result.units.empty()) {
            std::cerr << "Failed to load units from: " << argv[3] << "\n";
            return 1;
        }

        u32 unit_id = std::stoul(argv[4]);
        std::cout << analyzer.generate_unit_report(unit_id, parse_result.units);
        return 0;
    }

    // Matchup command
    if (command == "matchup" && argc >= 5) {
        ResultAnalyzer analyzer;
        if (!analyzer.load_results(argv[2])) {
            std::cerr << "Failed to load results from: " << argv[2] << "\n";
            return 1;
        }

        u32 id_a = std::stoul(argv[3]);
        u32 id_b = std::stoul(argv[4]);
        auto stats = analyzer.get_matchup(id_a, id_b);

        std::cout << "=== Matchup: Unit " << id_a << " vs Unit " << id_b << " ===\n\n";
        std::cout << "Total matches: " << stats.total() << "\n";
        std::cout << "Unit A wins: " << stats.a_wins << " (" << stats.a_win_rate() << "%)\n";
        std::cout << "Unit B wins: " << stats.b_wins << " (" << stats.b_win_rate() << "%)\n";
        std::cout << "Draws: " << stats.draws << "\n";
        std::cout << "Games won - A: " << (int)stats.games_a << ", B: " << (int)stats.games_b << "\n";
        return 0;
    }

    // CSV stats export
    if (command == "csv-stats" && argc >= 5) {
        ResultAnalyzer analyzer;
        if (!analyzer.load_results(argv[2])) {
            std::cerr << "Failed to load results from: " << argv[2] << "\n";
            return 1;
        }

        auto parse_result = UnitParser::parse_file(argv[3]);
        if (parse_result.units.empty()) {
            std::cerr << "Failed to load units from: " << argv[3] << "\n";
            return 1;
        }

        if (analyzer.export_unit_stats_csv(argv[4], parse_result.units)) {
            std::cout << "Exported stats to: " << argv[4] << "\n";
        } else {
            std::cerr << "Failed to export to: " << argv[4] << "\n";
            return 1;
        }
        return 0;
    }

    // CSV matchups export
    if (command == "csv-matchups" && argc >= 4) {
        ResultAnalyzer analyzer;
        if (!analyzer.load_results(argv[2])) {
            std::cerr << "Failed to load results from: " << argv[2] << "\n";
            return 1;
        }

        if (analyzer.export_matchups_csv(argv[3])) {
            std::cout << "Exported matchups to: " << argv[3] << "\n";
        } else {
            std::cerr << "Failed to export to: " << argv[3] << "\n";
            return 1;
        }
        return 0;
    }

    // JSON export
    if (command == "json" && argc >= 4) {
        ResultAnalyzer analyzer;
        if (!analyzer.load_results(argv[2])) {
            std::cerr << "Failed to load results from: " << argv[2] << "\n";
            return 1;
        }

        auto parse_result = UnitParser::parse_file(argv[3]);
        if (parse_result.units.empty()) {
            std::cerr << "Failed to load units from: " << argv[3] << "\n";
            return 1;
        }

        std::cout << analyzer.export_unit_stats_json(parse_result.units);
        return 0;
    }

    print_usage(argv[0]);
    return 1;
}
