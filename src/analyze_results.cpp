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
    std::cout << "\nElo Rating Commands (requires aggregated format results):\n";
    std::cout << "  elo <results.bin> <units.txt> [output.txt] [N|all] - Elo ratings (N=20, or 'all')\n";
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

    // Elo ratings command
    if (command == "elo" && argc >= 4) {
        ResultAnalyzer analyzer;
        if (!analyzer.load_results(argv[2])) {
            std::cerr << "Failed to load results from: " << argv[2] << "\n";
            return 1;
        }

        if (!analyzer.is_aggregated()) {
            std::cerr << "Error: Elo ratings require aggregated format results.\n";
            std::cerr << "Use 'batch_sim -a' to generate aggregated results.\n";
            return 1;
        }

        auto parse_result = UnitParser::parse_file(argv[3]);
        if (parse_result.units.empty()) {
            std::cerr << "Failed to load units from: " << argv[3] << "\n";
            return 1;
        }

        // Parse optional output file and N
        // Usage: elo <results.bin> <units.txt> [output.txt] [N|all]
        std::string output_file;
        size_t n = 20;
        bool show_all = false;

        auto parse_count = [&](const std::string& arg) {
            if (arg == "all" || arg == "0") {
                show_all = true;
                n = SIZE_MAX;
            } else {
                n = std::stoul(arg);
            }
        };

        if (argc >= 5) {
            // Check if argv[4] is a number (N) or a filename
            bool is_number = true;
            std::string arg4 = argv[4];
            for (char c : arg4) {
                if (!std::isdigit(c)) { is_number = false; break; }
            }

            if ((is_number || arg4 == "all") && argc == 5) {
                // Only N provided
                parse_count(arg4);
            } else {
                // Output file provided
                output_file = arg4;
                if (argc >= 6) {
                    parse_count(argv[5]);
                }
            }
        }

        auto top_elo = analyzer.get_top_units_by_elo(show_all ? SIZE_MAX : n);
        auto elo_map = analyzer.calculate_elo_ratings();

        // Use file output if specified, otherwise stdout
        std::ofstream file_out;
        std::ostream* out = &std::cout;
        bool csv_output = false;
        if (!output_file.empty()) {
            file_out.open(output_file);
            if (!file_out) {
                std::cerr << "Failed to open output file: " << output_file << "\n";
                return 1;
            }
            out = &file_out;
            // Check if output file ends with .csv
            csv_output = (output_file.size() >= 4 &&
                         output_file.substr(output_file.size() - 4) == ".csv");
            std::cout << "Writing Elo ratings to: " << output_file << "\n";
        }

        if (csv_output) {
            // CSV format
            *out << "rank,unit_id,name,faction,points,elo,win_rate,matchups\n";
            for (size_t i = 0; i < top_elo.size(); ++i) {
                const auto& [id, elo] = top_elo[i];
                if (id < parse_result.units.size()) {
                    const auto& unit = parse_result.units[id];
                    const auto* stats = analyzer.get_aggregated_stats(id);
                    *out << (i + 1) << ","
                         << id << ","
                         << "\"" << unit.name.view() << "\","
                         << "\"" << unit.faction.view() << "\","
                         << unit.points_cost << ","
                         << std::fixed << std::setprecision(1) << elo << ","
                         << (stats ? stats->win_rate() : 0.0) << ","
                         << (stats ? stats->total_matchups : 0) << "\n";
                }
            }
            std::cout << "Done. Wrote " << top_elo.size() << " entries.\n";
        } else {
            // Text table format
            if (show_all) {
                *out << "=== All Units by Elo Rating ===\n\n";
            } else {
                *out << "=== Top " << n << " Units by Elo Rating ===\n\n";
            }
            *out << std::left << std::setw(5) << "Rank"
                 << std::setw(35) << "Unit Name"
                 << std::setw(7) << "Pts"
                 << std::setw(8) << "Elo"
                 << std::setw(9) << "WinRate"
                 << std::setw(10) << "Matchups"
                 << "\n";
            *out << std::string(74, '-') << "\n";

            for (size_t i = 0; i < top_elo.size(); ++i) {
                const auto& [id, elo] = top_elo[i];
                if (id < parse_result.units.size()) {
                    const auto& unit = parse_result.units[id];
                    const auto* stats = analyzer.get_aggregated_stats(id);
                    std::string name_str(unit.name.view());
                    if (name_str.size() > 33) name_str = name_str.substr(0, 33);
                    *out << std::left << std::setw(5) << (i + 1)
                         << std::setw(35) << name_str
                         << std::setw(7) << unit.points_cost
                         << std::fixed << std::setprecision(0)
                         << std::setw(8) << elo
                         << std::setprecision(1)
                         << std::setw(9) << (stats ? std::to_string((int)stats->win_rate()) + "%" : "N/A")
                         << std::setw(10) << (stats ? stats->total_matchups : 0)
                         << "\n";
                }
            }

            // Also show bottom 10 for context (skip if showing all)
            size_t bottom_n = 0;
            if (!show_all) {
                std::vector<std::pair<u32, f64>> all_elo(elo_map.begin(), elo_map.end());
                std::sort(all_elo.begin(), all_elo.end(),
                    [](const auto& a, const auto& b) { return a.second < b.second; });

                *out << "\n=== Bottom 10 Units by Elo Rating ===\n\n";
                *out << std::left << std::setw(5) << "Rank"
                     << std::setw(35) << "Unit Name"
                     << std::setw(7) << "Pts"
                     << std::setw(8) << "Elo"
                     << std::setw(9) << "WinRate"
                     << "\n";
                *out << std::string(64, '-') << "\n";

                bottom_n = std::min(size_t(10), all_elo.size());
                for (size_t i = 0; i < bottom_n; ++i) {
                    const auto& [id, elo] = all_elo[i];
                    if (id < parse_result.units.size()) {
                        const auto& unit = parse_result.units[id];
                        const auto* stats = analyzer.get_aggregated_stats(id);
                        std::string name_str(unit.name.view());
                        if (name_str.size() > 33) name_str = name_str.substr(0, 33);
                        *out << std::left << std::setw(5) << (i + 1)
                             << std::setw(35) << name_str
                             << std::setw(7) << unit.points_cost
                             << std::fixed << std::setprecision(0)
                             << std::setw(8) << elo
                             << std::setprecision(1)
                             << std::setw(9) << (stats ? std::to_string((int)stats->win_rate()) + "%" : "N/A")
                             << "\n";
                    }
                }
            }

            if (!output_file.empty()) {
                std::cout << "Done. Wrote " << (top_elo.size() + bottom_n) << " entries.\n";
            }
        }

        return 0;
    }

    print_usage(argv[0]);
    return 1;
}
