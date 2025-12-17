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
    std::cout << "\nFull Game Statistics Commands (requires extended format results):\n";
    std::cout << "  game-stats <results.bin> <units.txt> [N] - Show game stats report (top N=10)\n";
    std::cout << "  ext-matchup <results.bin> <units.txt> <id_a> <id_b> - Extended matchup report\n";
    std::cout << "\nNote: Extended format results are generated using 'batch_sim -e' or 'batch_sim -E'\n";
    std::cout << "  -e: Extended format (24 bytes/result, full precision)\n";
    std::cout << "  -E: Compact extended (16 bytes/result, recommended for large simulations)\n";
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

        if (analyzer.is_aggregated()) {
            // Aggregated format - show per-unit stats directly
            const auto& results = analyzer.aggregated_results();
            std::vector<std::pair<u32, const AggregatedUnitResult*>> ranked;
            for (const auto& r : results) {
                if (r.total_matchups > 0) {
                    ranked.push_back({r.unit_id, &r});
                }
            }
            std::sort(ranked.begin(), ranked.end(),
                [](const auto& a, const auto& b) {
                    return a.second->win_rate() > b.second->win_rate();
                });

            std::cout << "=== Top " << n << " Units by Win Rate (Aggregated Stats) ===\n\n";
            std::cout << std::left << std::setw(5) << "Rank"
                      << std::setw(35) << "Unit Name"
                      << std::setw(7) << "Pts"
                      << std::setw(9) << "WinRate"
                      << std::setw(10) << "Matchups"
                      << std::setw(8) << "AvgDmg"
                      << std::setw(8) << "AvgKill"
                      << "\n";
            std::cout << std::string(82, '-') << "\n";

            for (size_t i = 0; i < std::min(n, ranked.size()); ++i) {
                const auto& [id, r] = ranked[i];
                if (id < parse_result.units.size()) {
                    const auto& unit = parse_result.units[id];
                    std::string name_str(unit.name.view());
                    if (name_str.size() > 33) name_str = name_str.substr(0, 33);
                    std::cout << std::left << std::setw(5) << (i + 1)
                              << std::setw(35) << name_str
                              << std::setw(7) << unit.points_cost
                              << std::fixed << std::setprecision(1)
                              << std::setw(9) << (std::to_string((int)r->win_rate()) + "%")
                              << std::setw(10) << r->total_matchups
                              << std::setprecision(2)
                              << std::setw(8) << r->avg_wounds_dealt()
                              << std::setw(8) << r->avg_models_killed()
                              << "\n";
                }
            }
        } else if (analyzer.has_extended_data()) {
            // Extended format - show more stats
            auto stats = analyzer.calculate_extended_unit_stats();
            std::vector<std::pair<u32, ExtendedUnitStats>> ranked(stats.begin(), stats.end());
            std::sort(ranked.begin(), ranked.end(),
                [](const auto& a, const auto& b) {
                    return a.second.win_rate() > b.second.win_rate();
                });

            std::cout << "=== Top " << n << " Units by Win Rate (Extended Stats) ===\n\n";
            std::cout << std::left << std::setw(5) << "Rank"
                      << std::setw(35) << "Unit Name"
                      << std::setw(7) << "Pts"
                      << std::setw(9) << "WinRate"
                      << std::setw(8) << "DmgEff"
                      << std::setw(8) << "KillEff"
                      << std::setw(8) << "ObjCtrl"
                      << "\n";
            std::cout << std::string(80, '-') << "\n";

            for (size_t i = 0; i < std::min(n, ranked.size()); ++i) {
                const auto& [id, s] = ranked[i];
                if (id < parse_result.units.size() && s.matches_played >= 3) {
                    const auto& unit = parse_result.units[id];
                    std::string name_str(unit.name.view());
                    if (name_str.size() > 33) name_str = name_str.substr(0, 33);
                    std::cout << std::left << std::setw(5) << (i + 1)
                              << std::setw(35) << name_str
                              << std::setw(7) << unit.points_cost
                              << std::fixed << std::setprecision(1)
                              << std::setw(9) << (std::to_string((int)s.win_rate()) + "%")
                              << std::setprecision(2)
                              << std::setw(8) << s.damage_efficiency()
                              << std::setw(8) << s.kill_efficiency()
                              << std::setprecision(1)
                              << std::setw(8) << (std::to_string((int)s.objective_control_rate()) + "%")
                              << "\n";
                }
            }
        } else {
            // Compact format - basic stats only
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

        std::cout << "=== Matchup: Unit " << id_a << " vs Unit " << id_b << " ===\n\n";

        if (analyzer.has_extended_data()) {
            // Extended format - show full combat stats
            auto stats = analyzer.get_extended_matchup(id_a, id_b);

            std::cout << "Match Results:\n";
            std::cout << "  Total matches: " << stats.total() << "\n";
            std::cout << "  Unit A wins: " << stats.a_wins << " (" << std::fixed << std::setprecision(1) << stats.a_win_rate() << "%)\n";
            std::cout << "  Unit B wins: " << stats.b_wins << " (" << stats.b_win_rate() << "%)\n";
            std::cout << "  Draws: " << stats.draws << "\n";
            std::cout << "  Games won - A: " << (int)stats.games_a << ", B: " << (int)stats.games_b << "\n";

            std::cout << "\nCombat Statistics:\n";
            std::cout << "  Unit A avg wounds dealt: " << std::setprecision(2) << stats.avg_wounds_a() << "\n";
            std::cout << "  Unit B avg wounds dealt: " << stats.avg_wounds_b() << "\n";
            std::cout << "  Unit A avg models killed: " << stats.avg_models_killed_a() << "\n";
            std::cout << "  Unit B avg models killed: " << stats.avg_models_killed_b() << "\n";

            std::cout << "\nObjective Control:\n";
            std::cout << "  Unit A avg rounds holding: " << stats.avg_rounds_holding_a() << "\n";
            std::cout << "  Unit B avg rounds holding: " << stats.avg_rounds_holding_b() << "\n";
        } else {
            // Compact format - basic stats only
            auto stats = analyzer.get_matchup(id_a, id_b);

            std::cout << "Total matches: " << stats.total() << "\n";
            std::cout << "Unit A wins: " << stats.a_wins << " (" << stats.a_win_rate() << "%)\n";
            std::cout << "Unit B wins: " << stats.b_wins << " (" << stats.b_win_rate() << "%)\n";
            std::cout << "Draws: " << stats.draws << "\n";
            std::cout << "Games won - A: " << (int)stats.games_a << ", B: " << (int)stats.games_b << "\n";
        }
        return 0;
    }

    // CSV stats export
    if (command == "csv-stats" && argc >= 5) {
        ResultAnalyzer analyzer;
        std::cout << "Loading results from: " << argv[2] << "\n";
        if (!analyzer.load_results(argv[2], true)) {
            std::cerr << "Failed to load results from: " << argv[2] << "\n";
            return 1;
        }
        std::cout << "  Results loaded: " << analyzer.result_count() << " entries\n";
        const char* fmt = "Compact";
        if (analyzer.is_aggregated()) fmt = "Aggregated";
        else if (analyzer.has_extended_data()) fmt = "Extended";
        std::cout << "  Format: " << fmt << "\n";

        std::cout << "Loading units from: " << argv[3] << "\n";
        auto parse_result = UnitParser::parse_file(argv[3]);
        if (parse_result.units.empty()) {
            std::cerr << "Failed to load units from: " << argv[3] << "\n";
            return 1;
        }
        std::cout << "  Units loaded: " << parse_result.units.size() << "\n";

        std::cout << "Exporting to: " << argv[4] << "\n";
        auto [success, rows] = analyzer.export_unit_stats_csv_with_count(argv[4], parse_result.units);
        if (success) {
            std::cout << "Exported " << rows << " unit stats to: " << argv[4] << "\n";
            if (rows == 0) {
                std::cerr << "Warning: No rows written. Unit IDs in results may not match the units file.\n";
                std::cerr << "Make sure you're using the same units file that was used for batch_sim.\n";
            }
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

    // Game stats report (extended format)
    if (command == "game-stats" && argc >= 4) {
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

        size_t n = (argc >= 5) ? std::stoul(argv[4]) : 10;
        std::cout << analyzer.generate_game_stats_report(parse_result.units, n);
        return 0;
    }

    // Extended matchup report
    if (command == "ext-matchup" && argc >= 6) {
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

        u32 id_a = std::stoul(argv[4]);
        u32 id_b = std::stoul(argv[5]);
        std::cout << analyzer.generate_extended_matchup_report(id_a, id_b, parse_result.units);
        return 0;
    }

    print_usage(argv[0]);
    return 1;
}
