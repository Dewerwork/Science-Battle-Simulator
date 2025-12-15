#pragma once

#include "core/types.hpp"
#include "core/unit.hpp"
#include "simulation/batch_simulator.hpp"
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace battle {

// ==============================================================================
// Result File Header
// ==============================================================================

struct ResultFileHeader {
    u32 magic;      // 0x42415453 = "SABS"
    u32 version;
    u32 units_a_count;
    u32 units_b_count;

    bool is_valid() const { return magic == 0x42415453 && version == 1; }
    u64 expected_results() const {
        return static_cast<u64>(units_a_count) * units_b_count;
    }
};

// ==============================================================================
// Unit Statistics
// ==============================================================================

struct UnitStats {
    u32 unit_id = 0;
    u64 matches_played = 0;
    u64 wins = 0;
    u64 losses = 0;
    u64 draws = 0;
    u64 games_won = 0;      // Individual games in matches
    u64 games_lost = 0;

    f64 win_rate() const {
        return matches_played > 0 ? (100.0 * wins / matches_played) : 0.0;
    }
    f64 draw_rate() const {
        return matches_played > 0 ? (100.0 * draws / matches_played) : 0.0;
    }
    f64 game_win_rate() const {
        u64 total = games_won + games_lost;
        return total > 0 ? (100.0 * games_won / total) : 0.0;
    }
};

// ==============================================================================
// Matchup Statistics
// ==============================================================================

struct MatchupStats {
    u32 unit_a_id = 0;
    u32 unit_b_id = 0;
    u32 a_wins = 0;
    u32 b_wins = 0;
    u32 draws = 0;
    u8 games_a = 0;         // Total games won by A
    u8 games_b = 0;         // Total games won by B

    u32 total() const { return a_wins + b_wins + draws; }
    f64 a_win_rate() const { return total() > 0 ? (100.0 * a_wins / total()) : 0.0; }
    f64 b_win_rate() const { return total() > 0 ? (100.0 * b_wins / total()) : 0.0; }
};

// ==============================================================================
// Query Filter
// ==============================================================================

struct ResultFilter {
    std::vector<u32> unit_ids;           // Filter to specific units
    u32 min_points = 0;
    u32 max_points = UINT32_MAX;
    std::string faction;                  // Filter by faction
    bool only_wins = false;              // Only show results where unit won
    bool only_losses = false;

    bool matches_unit(u32 id, const std::vector<Unit>& units) const {
        // Check unit ID filter
        if (!unit_ids.empty()) {
            bool found = false;
            for (u32 uid : unit_ids) {
                if (uid == id) { found = true; break; }
            }
            if (!found) return false;
        }

        // Check points range
        if (id < units.size()) {
            u32 pts = units[id].points_cost;
            if (pts < min_points || pts > max_points) return false;

            // Check faction
            if (!faction.empty()) {
                std::string_view unit_faction = units[id].faction.view();
                if (unit_faction != faction) return false;
            }
        }

        return true;
    }
};

// ==============================================================================
// Result Analyzer - Read and analyze simulation results
// ==============================================================================

class ResultAnalyzer {
public:
    ResultAnalyzer() = default;

    // Load results from binary file
    bool load_results(const std::string& filename) {
        std::ifstream in(filename, std::ios::binary);
        if (!in) return false;

        // Read header
        in.read(reinterpret_cast<char*>(&header_), sizeof(header_));
        if (!header_.is_valid()) return false;

        // Read results
        results_.clear();
        results_.reserve(header_.expected_results());

        CompactMatchResult result;
        while (in.read(reinterpret_cast<char*>(&result), sizeof(result))) {
            results_.push_back(result);
        }

        return true;
    }

    // Load unit data for queries
    void set_units(const std::vector<Unit>& units_a, const std::vector<Unit>& units_b) {
        units_a_ = units_a;
        units_b_ = units_b;
    }

    // Get header info
    const ResultFileHeader& header() const { return header_; }
    size_t result_count() const { return results_.size(); }

    // ===========================================================================
    // Queries
    // ===========================================================================

    // Get all results for a specific unit
    std::vector<CompactMatchResult> get_results_for_unit(u32 unit_id, bool as_unit_a = true) const {
        std::vector<CompactMatchResult> filtered;
        for (const auto& r : results_) {
            if (as_unit_a && r.unit_a_id == unit_id) {
                filtered.push_back(r);
            } else if (!as_unit_a && r.unit_b_id == unit_id) {
                filtered.push_back(r);
            }
        }
        return filtered;
    }

    // Get matchup between two specific units
    MatchupStats get_matchup(u32 unit_a_id, u32 unit_b_id) const {
        MatchupStats stats;
        stats.unit_a_id = unit_a_id;
        stats.unit_b_id = unit_b_id;

        for (const auto& r : results_) {
            if (r.unit_a_id == unit_a_id && r.unit_b_id == unit_b_id) {
                if (r.winner == 0) stats.a_wins++;
                else if (r.winner == 1) stats.b_wins++;
                else stats.draws++;
                stats.games_a += r.games_a;
                stats.games_b += r.games_b;
            }
        }
        return stats;
    }

    // ===========================================================================
    // Statistics
    // ===========================================================================

    // Calculate statistics for all units
    std::unordered_map<u32, UnitStats> calculate_unit_stats() const {
        std::unordered_map<u32, UnitStats> stats;

        for (const auto& r : results_) {
            // Unit A stats
            auto& sa = stats[r.unit_a_id];
            sa.unit_id = r.unit_a_id;
            sa.matches_played++;
            if (r.winner == 0) sa.wins++;
            else if (r.winner == 1) sa.losses++;
            else sa.draws++;
            sa.games_won += r.games_a;
            sa.games_lost += r.games_b;

            // Unit B stats
            auto& sb = stats[r.unit_b_id];
            sb.unit_id = r.unit_b_id;
            sb.matches_played++;
            if (r.winner == 1) sb.wins++;
            else if (r.winner == 0) sb.losses++;
            else sb.draws++;
            sb.games_won += r.games_b;
            sb.games_lost += r.games_a;
        }

        return stats;
    }

    // Get top N units by win rate
    std::vector<std::pair<u32, UnitStats>> get_top_units(size_t n, u32 min_matches = 1) const {
        auto stats = calculate_unit_stats();

        std::vector<std::pair<u32, UnitStats>> ranked;
        for (const auto& [id, s] : stats) {
            if (s.matches_played >= min_matches) {
                ranked.emplace_back(id, s);
            }
        }

        std::sort(ranked.begin(), ranked.end(),
            [](const auto& a, const auto& b) {
                return a.second.win_rate() > b.second.win_rate();
            });

        if (ranked.size() > n) {
            ranked.resize(n);
        }
        return ranked;
    }

    // Get units with best win rate in their points bracket
    std::vector<std::pair<u32, UnitStats>> get_best_value_units(
        const std::vector<Unit>& units,
        u32 points_bracket = 50,
        size_t per_bracket = 5
    ) const {
        auto stats = calculate_unit_stats();

        // Group by points bracket
        std::unordered_map<u32, std::vector<std::pair<u32, UnitStats>>> brackets;
        for (const auto& [id, s] : stats) {
            if (id < units.size()) {
                u32 bracket = (units[id].points_cost / points_bracket) * points_bracket;
                brackets[bracket].emplace_back(id, s);
            }
        }

        // Sort each bracket and take top N
        std::vector<std::pair<u32, UnitStats>> result;
        for (auto& [bracket, vec] : brackets) {
            std::sort(vec.begin(), vec.end(),
                [](const auto& a, const auto& b) {
                    return a.second.win_rate() > b.second.win_rate();
                });

            size_t count = std::min(vec.size(), per_bracket);
            for (size_t i = 0; i < count; ++i) {
                result.push_back(vec[i]);
            }
        }

        return result;
    }

    // ===========================================================================
    // Reports
    // ===========================================================================

    // Generate summary report as string
    std::string generate_summary_report() const {
        std::ostringstream ss;

        ss << "=== Battle Simulation Results Summary ===\n\n";
        ss << "Total Results: " << results_.size() << "\n";
        ss << "Units A: " << header_.units_a_count << "\n";
        ss << "Units B: " << header_.units_b_count << "\n\n";

        // Count outcomes
        u64 a_wins = 0, b_wins = 0, draws = 0;
        for (const auto& r : results_) {
            if (r.winner == 0) a_wins++;
            else if (r.winner == 1) b_wins++;
            else draws++;
        }

        ss << "Outcomes:\n";
        ss << "  Unit A wins: " << a_wins << " ("
           << (100.0 * a_wins / results_.size()) << "%)\n";
        ss << "  Unit B wins: " << b_wins << " ("
           << (100.0 * b_wins / results_.size()) << "%)\n";
        ss << "  Draws: " << draws << " ("
           << (100.0 * draws / results_.size()) << "%)\n";

        return ss.str();
    }

    // Generate detailed unit report
    std::string generate_unit_report(u32 unit_id, const std::vector<Unit>& units) const {
        std::ostringstream ss;

        if (unit_id >= units.size()) {
            ss << "Unit ID out of range\n";
            return ss.str();
        }

        const Unit& unit = units[unit_id];
        auto stats = calculate_unit_stats();
        auto it = stats.find(unit_id);

        ss << "=== Unit Report: " << unit.name.view() << " ===\n\n";
        ss << "Points: " << unit.points_cost << "\n";
        ss << "Quality: " << (int)unit.quality << "+\n";
        ss << "Defense: " << (int)unit.defense << "+\n";
        ss << "Models: " << (int)unit.model_count << "\n\n";

        if (it != stats.end()) {
            const auto& s = it->second;
            ss << "Performance:\n";
            ss << "  Matches: " << s.matches_played << "\n";
            ss << "  Win Rate: " << s.win_rate() << "%\n";
            ss << "  Wins/Losses/Draws: " << s.wins << "/" << s.losses << "/" << s.draws << "\n";
            ss << "  Game Win Rate: " << s.game_win_rate() << "%\n";
        }

        return ss.str();
    }

    // ===========================================================================
    // Export
    // ===========================================================================

    // Export unit stats to CSV
    bool export_unit_stats_csv(const std::string& filename, const std::vector<Unit>& units) const {
        std::ofstream out(filename);
        if (!out) return false;

        auto stats = calculate_unit_stats();

        // Header
        out << "unit_id,name,faction,points,quality,defense,models,"
            << "matches,wins,losses,draws,win_rate,game_win_rate\n";

        // Data rows
        for (const auto& [id, s] : stats) {
            if (id < units.size()) {
                const Unit& u = units[id];
                out << id << ","
                    << "\"" << u.name.view() << "\","
                    << "\"" << u.faction.view() << "\","
                    << u.points_cost << ","
                    << (int)u.quality << ","
                    << (int)u.defense << ","
                    << (int)u.model_count << ","
                    << s.matches_played << ","
                    << s.wins << ","
                    << s.losses << ","
                    << s.draws << ","
                    << s.win_rate() << ","
                    << s.game_win_rate() << "\n";
            }
        }

        return true;
    }

    // Export matchups to CSV
    bool export_matchups_csv(const std::string& filename) const {
        std::ofstream out(filename);
        if (!out) return false;

        // Header
        out << "unit_a_id,unit_b_id,winner,games_a,games_b\n";

        // Data rows
        for (const auto& r : results_) {
            out << r.unit_a_id << ","
                << r.unit_b_id << ","
                << r.winner << ","
                << (int)r.games_a << ","
                << (int)r.games_b << "\n";
        }

        return true;
    }

    // Export to JSON format
    std::string export_unit_stats_json(const std::vector<Unit>& units) const {
        std::ostringstream ss;
        auto stats = calculate_unit_stats();

        ss << "{\n  \"units\": [\n";

        bool first = true;
        for (const auto& [id, s] : stats) {
            if (id >= units.size()) continue;

            if (!first) ss << ",\n";
            first = false;

            const Unit& u = units[id];
            ss << "    {\n";
            ss << "      \"id\": " << id << ",\n";
            ss << "      \"name\": \"" << u.name.view() << "\",\n";
            ss << "      \"faction\": \"" << u.faction.view() << "\",\n";
            ss << "      \"points\": " << u.points_cost << ",\n";
            ss << "      \"matches\": " << s.matches_played << ",\n";
            ss << "      \"wins\": " << s.wins << ",\n";
            ss << "      \"losses\": " << s.losses << ",\n";
            ss << "      \"draws\": " << s.draws << ",\n";
            ss << "      \"win_rate\": " << s.win_rate() << ",\n";
            ss << "      \"game_win_rate\": " << s.game_win_rate() << "\n";
            ss << "    }";
        }

        ss << "\n  ]\n}\n";
        return ss.str();
    }

private:
    ResultFileHeader header_{};
    std::vector<CompactMatchResult> results_;
    std::vector<Unit> units_a_;
    std::vector<Unit> units_b_;
};

// ==============================================================================
// Quick Analysis Helper
// ==============================================================================

inline void print_quick_analysis(const std::string& result_file, const std::vector<Unit>& units) {
    ResultAnalyzer analyzer;

    if (!analyzer.load_results(result_file)) {
        std::cout << "Failed to load results from: " << result_file << "\n";
        return;
    }

    std::cout << analyzer.generate_summary_report() << "\n";

    // Top 10 units
    std::cout << "\n=== Top 10 Units by Win Rate ===\n";
    auto top = analyzer.get_top_units(10, 5);
    for (size_t i = 0; i < top.size(); ++i) {
        const auto& [id, stats] = top[i];
        if (id < units.size()) {
            std::cout << (i+1) << ". " << units[id].name.view()
                      << " (" << units[id].points_cost << "pts) - "
                      << stats.win_rate() << "% win rate ("
                      << stats.matches_played << " matches)\n";
        }
    }
}

} // namespace battle
