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
#include <iomanip>

namespace battle {

// ==============================================================================
// Result File Header
// ==============================================================================

struct ResultFileHeader {
    u32 magic;      // 0x42415453 = "SABS"
    u32 version;    // 1 = compact (8 bytes), 2 = extended (24 bytes), 3 = compact_extended (16 bytes), 4 = aggregated (256 bytes/unit)
    u32 units_a_count;
    u32 units_b_count;

    bool is_valid() const { return magic == 0x42415453 && (version >= 1 && version <= 4); }
    bool is_extended() const { return version == 2; }
    bool is_compact_extended() const { return version == 3; }
    bool is_aggregated() const { return version == 4; }
    bool has_extended_data() const { return version == 2 || version == 3; }
    u64 expected_results() const {
        if (version == 4) return units_a_count;  // Aggregated: one result per unit
        return static_cast<u64>(units_a_count) * units_b_count;
    }
    size_t result_size() const {
        switch (version) {
            case 1: return sizeof(CompactMatchResult);
            case 2: return sizeof(ExtendedMatchResult);
            case 3: return sizeof(CompactExtendedMatchResult);
            case 4: return sizeof(AggregatedUnitResult);
            default: return sizeof(CompactMatchResult);
        }
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
// Extended Unit Statistics (includes full game stats)
// ==============================================================================

struct ExtendedUnitStats : public UnitStats {
    // Combat statistics
    u64 total_wounds_dealt = 0;
    u64 total_wounds_received = 0;
    u64 total_models_killed = 0;
    u64 total_models_lost = 0;

    // Objective control
    u64 total_rounds_holding = 0;
    u64 total_rounds_opponent_holding = 0;

    // Game length
    u64 total_rounds_played = 0;

    // Averages (computed)
    f64 avg_wounds_dealt() const {
        return matches_played > 0 ? static_cast<f64>(total_wounds_dealt) / matches_played : 0.0;
    }
    f64 avg_wounds_received() const {
        return matches_played > 0 ? static_cast<f64>(total_wounds_received) / matches_played : 0.0;
    }
    f64 avg_models_killed() const {
        return matches_played > 0 ? static_cast<f64>(total_models_killed) / matches_played : 0.0;
    }
    f64 avg_models_lost() const {
        return matches_played > 0 ? static_cast<f64>(total_models_lost) / matches_played : 0.0;
    }
    f64 avg_rounds_holding() const {
        return matches_played > 0 ? static_cast<f64>(total_rounds_holding) / matches_played : 0.0;
    }
    f64 avg_rounds_played() const {
        return matches_played > 0 ? static_cast<f64>(total_rounds_played) / matches_played : 0.0;
    }

    // Efficiency metrics
    f64 damage_efficiency() const {
        // Ratio of wounds dealt to wounds received
        return total_wounds_received > 0 ?
            static_cast<f64>(total_wounds_dealt) / total_wounds_received : 0.0;
    }
    f64 kill_efficiency() const {
        // Ratio of models killed to models lost
        return total_models_lost > 0 ?
            static_cast<f64>(total_models_killed) / total_models_lost : 0.0;
    }
    f64 objective_control_rate() const {
        // Percentage of total rounds where unit held objective
        u64 total_obj_rounds = total_rounds_holding + total_rounds_opponent_holding;
        return total_obj_rounds > 0 ?
            (100.0 * total_rounds_holding / total_obj_rounds) : 0.0;
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
// Extended Matchup Statistics (includes full game stats)
// ==============================================================================

struct ExtendedMatchupStats : public MatchupStats {
    // Combat statistics
    u64 wounds_dealt_a = 0;
    u64 wounds_dealt_b = 0;
    u64 models_killed_a = 0;
    u64 models_killed_b = 0;

    // Objective control
    u64 rounds_holding_a = 0;
    u64 rounds_holding_b = 0;

    // Averages
    f64 avg_wounds_a() const { return total() > 0 ? static_cast<f64>(wounds_dealt_a) / total() : 0.0; }
    f64 avg_wounds_b() const { return total() > 0 ? static_cast<f64>(wounds_dealt_b) / total() : 0.0; }
    f64 avg_models_killed_a() const { return total() > 0 ? static_cast<f64>(models_killed_a) / total() : 0.0; }
    f64 avg_models_killed_b() const { return total() > 0 ? static_cast<f64>(models_killed_b) / total() : 0.0; }
    f64 avg_rounds_holding_a() const { return total() > 0 ? static_cast<f64>(rounds_holding_a) / total() : 0.0; }
    f64 avg_rounds_holding_b() const { return total() > 0 ? static_cast<f64>(rounds_holding_b) / total() : 0.0; }
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

    // Load results from binary file (auto-detects format)
    bool load_results(const std::string& filename, bool verbose = false) {
        std::ifstream in(filename, std::ios::binary);
        if (!in) {
            if (verbose) std::cerr << "  Error: Cannot open file\n";
            return false;
        }

        // Read header
        in.read(reinterpret_cast<char*>(&header_), sizeof(header_));
        if (!in) {
            if (verbose) std::cerr << "  Error: Cannot read header (file too small?)\n";
            return false;
        }
        if (!header_.is_valid()) {
            if (verbose) {
                std::cerr << "  Error: Invalid header\n";
                std::cerr << "    Magic: 0x" << std::hex << header_.magic << std::dec
                          << " (expected 0x42415453 'SABS')\n";
                std::cerr << "    Version: " << header_.version << " (expected 1, 2, 3, or 4)\n";
            }
            return false;
        }

        // Clear previous results
        results_.clear();
        extended_results_.clear();
        compact_extended_results_.clear();
        aggregated_results_.clear();

        // Read results based on version
        if (header_.is_aggregated()) {
            aggregated_results_.reserve(header_.expected_results());
            AggregatedUnitResult result;
            while (in.read(reinterpret_cast<char*>(&result), sizeof(result))) {
                aggregated_results_.push_back(result);
            }
        } else if (header_.is_extended()) {
            extended_results_.reserve(header_.expected_results());
            ExtendedMatchResult result;
            while (in.read(reinterpret_cast<char*>(&result), sizeof(result))) {
                extended_results_.push_back(result);
            }
        } else if (header_.is_compact_extended()) {
            compact_extended_results_.reserve(header_.expected_results());
            CompactExtendedMatchResult result;
            while (in.read(reinterpret_cast<char*>(&result), sizeof(result))) {
                compact_extended_results_.push_back(result);
            }
        } else {
            results_.reserve(header_.expected_results());
            CompactMatchResult result;
            while (in.read(reinterpret_cast<char*>(&result), sizeof(result))) {
                results_.push_back(result);
            }
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
    bool has_extended_data() const { return header_.has_extended_data(); }
    bool is_aggregated() const { return header_.is_aggregated(); }
    size_t result_count() const {
        if (header_.is_aggregated()) return aggregated_results_.size();
        if (header_.is_extended()) return extended_results_.size();
        if (header_.is_compact_extended()) return compact_extended_results_.size();
        return results_.size();
    }

    // Access aggregated results directly
    const std::vector<AggregatedUnitResult>& aggregated_results() const { return aggregated_results_; }

    // Get aggregated stats for a specific unit
    const AggregatedUnitResult* get_aggregated_stats(u32 unit_id) const {
        if (!header_.is_aggregated()) return nullptr;
        for (const auto& r : aggregated_results_) {
            if (r.unit_id == unit_id) return &r;
        }
        return nullptr;
    }

    // ===========================================================================
    // Queries
    // ===========================================================================

    // Get all results for a specific unit (compact format)
    std::vector<CompactMatchResult> get_results_for_unit(u32 unit_id, bool as_unit_a = true) const {
        std::vector<CompactMatchResult> filtered;
        if (header_.is_extended()) {
            for (const auto& r : extended_results_) {
                if (as_unit_a && r.unit_a_id == unit_id) {
                    filtered.push_back(r.to_compact());
                } else if (!as_unit_a && r.unit_b_id == unit_id) {
                    filtered.push_back(r.to_compact());
                }
            }
        } else if (header_.is_compact_extended()) {
            for (const auto& r : compact_extended_results_) {
                if (as_unit_a && r.unit_a_id == unit_id) {
                    filtered.push_back(r.to_compact());
                } else if (!as_unit_a && r.unit_b_id == unit_id) {
                    filtered.push_back(r.to_compact());
                }
            }
        } else {
            for (const auto& r : results_) {
                if (as_unit_a && r.unit_a_id == unit_id) {
                    filtered.push_back(r);
                } else if (!as_unit_a && r.unit_b_id == unit_id) {
                    filtered.push_back(r);
                }
            }
        }
        return filtered;
    }

    // Get all extended results for a specific unit (converts compact_extended to extended)
    std::vector<ExtendedMatchResult> get_extended_results_for_unit(u32 unit_id, bool as_unit_a = true) const {
        std::vector<ExtendedMatchResult> filtered;
        if (header_.is_extended()) {
            for (const auto& r : extended_results_) {
                if (as_unit_a && r.unit_a_id == unit_id) {
                    filtered.push_back(r);
                } else if (!as_unit_a && r.unit_b_id == unit_id) {
                    filtered.push_back(r);
                }
            }
        } else if (header_.is_compact_extended()) {
            for (const auto& r : compact_extended_results_) {
                if (as_unit_a && r.unit_a_id == unit_id) {
                    filtered.push_back(r.to_extended());
                } else if (!as_unit_a && r.unit_b_id == unit_id) {
                    filtered.push_back(r.to_extended());
                }
            }
        }
        return filtered;
    }

    // Get matchup between two specific units
    MatchupStats get_matchup(u32 unit_a_id, u32 unit_b_id) const {
        MatchupStats stats;
        stats.unit_a_id = unit_a_id;
        stats.unit_b_id = unit_b_id;

        if (header_.is_extended()) {
            for (const auto& r : extended_results_) {
                if (r.unit_a_id == unit_a_id && r.unit_b_id == unit_b_id) {
                    if (r.winner == 0) stats.a_wins++;
                    else if (r.winner == 1) stats.b_wins++;
                    else stats.draws++;
                    stats.games_a += r.games_a;
                    stats.games_b += r.games_b;
                }
            }
        } else if (header_.is_compact_extended()) {
            for (const auto& r : compact_extended_results_) {
                if (r.unit_a_id == unit_a_id && r.unit_b_id == unit_b_id) {
                    if (r.winner() == 0) stats.a_wins++;
                    else if (r.winner() == 1) stats.b_wins++;
                    else stats.draws++;
                    stats.games_a += r.games_a();
                    stats.games_b += r.games_b();
                }
            }
        } else {
            for (const auto& r : results_) {
                if (r.unit_a_id == unit_a_id && r.unit_b_id == unit_b_id) {
                    if (r.winner == 0) stats.a_wins++;
                    else if (r.winner == 1) stats.b_wins++;
                    else stats.draws++;
                    stats.games_a += r.games_a;
                    stats.games_b += r.games_b;
                }
            }
        }
        return stats;
    }

    // Get extended matchup stats (works with extended and compact_extended formats)
    ExtendedMatchupStats get_extended_matchup(u32 unit_a_id, u32 unit_b_id) const {
        ExtendedMatchupStats stats;
        stats.unit_a_id = unit_a_id;
        stats.unit_b_id = unit_b_id;

        if (header_.is_extended()) {
            for (const auto& r : extended_results_) {
                if (r.unit_a_id == unit_a_id && r.unit_b_id == unit_b_id) {
                    if (r.winner == 0) stats.a_wins++;
                    else if (r.winner == 1) stats.b_wins++;
                    else stats.draws++;
                    stats.games_a += r.games_a;
                    stats.games_b += r.games_b;
                    stats.wounds_dealt_a += r.wounds_dealt_a;
                    stats.wounds_dealt_b += r.wounds_dealt_b;
                    stats.models_killed_a += r.models_killed_a;
                    stats.models_killed_b += r.models_killed_b;
                    stats.rounds_holding_a += r.rounds_holding_a;
                    stats.rounds_holding_b += r.rounds_holding_b;
                }
            }
        } else if (header_.is_compact_extended()) {
            for (const auto& r : compact_extended_results_) {
                if (r.unit_a_id == unit_a_id && r.unit_b_id == unit_b_id) {
                    if (r.winner() == 0) stats.a_wins++;
                    else if (r.winner() == 1) stats.b_wins++;
                    else stats.draws++;
                    stats.games_a += r.games_a();
                    stats.games_b += r.games_b();
                    stats.wounds_dealt_a += r.wounds_dealt_a();
                    stats.wounds_dealt_b += r.wounds_dealt_b();
                    stats.models_killed_a += r.models_killed_a;
                    stats.models_killed_b += r.models_killed_b;
                    stats.rounds_holding_a += r.rounds_holding_a();
                    stats.rounds_holding_b += r.rounds_holding_b();
                }
            }
        }
        return stats;
    }

    // ===========================================================================
    // Statistics
    // ===========================================================================

    // Calculate statistics for all units (works with all formats)
    std::unordered_map<u32, UnitStats> calculate_unit_stats() const {
        std::unordered_map<u32, UnitStats> stats;

        if (header_.is_extended()) {
            for (const auto& r : extended_results_) {
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
        } else if (header_.is_compact_extended()) {
            for (const auto& r : compact_extended_results_) {
                // Unit A stats
                auto& sa = stats[r.unit_a_id];
                sa.unit_id = r.unit_a_id;
                sa.matches_played++;
                if (r.winner() == 0) sa.wins++;
                else if (r.winner() == 1) sa.losses++;
                else sa.draws++;
                sa.games_won += r.games_a();
                sa.games_lost += r.games_b();

                // Unit B stats
                auto& sb = stats[r.unit_b_id];
                sb.unit_id = r.unit_b_id;
                sb.matches_played++;
                if (r.winner() == 1) sb.wins++;
                else if (r.winner() == 0) sb.losses++;
                else sb.draws++;
                sb.games_won += r.games_b();
                sb.games_lost += r.games_a();
            }
        } else {
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
        }

        return stats;
    }

    // Calculate extended statistics for all units (works with extended and compact_extended formats)
    std::unordered_map<u32, ExtendedUnitStats> calculate_extended_unit_stats() const {
        std::unordered_map<u32, ExtendedUnitStats> stats;

        if (!header_.has_extended_data()) return stats;

        if (header_.is_extended()) {
            for (const auto& r : extended_results_) {
                // Unit A stats
                auto& sa = stats[r.unit_a_id];
                sa.unit_id = r.unit_a_id;
                sa.matches_played++;
                if (r.winner == 0) sa.wins++;
                else if (r.winner == 1) sa.losses++;
                else sa.draws++;
                sa.games_won += r.games_a;
                sa.games_lost += r.games_b;
                sa.total_wounds_dealt += r.wounds_dealt_a;
                sa.total_wounds_received += r.wounds_dealt_b;
                sa.total_models_killed += r.models_killed_a;
                sa.total_models_lost += r.models_killed_b;
                sa.total_rounds_holding += r.rounds_holding_a;
                sa.total_rounds_opponent_holding += r.rounds_holding_b;
                sa.total_rounds_played += r.total_rounds;

                // Unit B stats
                auto& sb = stats[r.unit_b_id];
                sb.unit_id = r.unit_b_id;
                sb.matches_played++;
                if (r.winner == 1) sb.wins++;
                else if (r.winner == 0) sb.losses++;
                else sb.draws++;
                sb.games_won += r.games_b;
                sb.games_lost += r.games_a;
                sb.total_wounds_dealt += r.wounds_dealt_b;
                sb.total_wounds_received += r.wounds_dealt_a;
                sb.total_models_killed += r.models_killed_b;
                sb.total_models_lost += r.models_killed_a;
                sb.total_rounds_holding += r.rounds_holding_b;
                sb.total_rounds_opponent_holding += r.rounds_holding_a;
                sb.total_rounds_played += r.total_rounds;
            }
        } else if (header_.is_compact_extended()) {
            for (const auto& r : compact_extended_results_) {
                // Unit A stats
                auto& sa = stats[r.unit_a_id];
                sa.unit_id = r.unit_a_id;
                sa.matches_played++;
                if (r.winner() == 0) sa.wins++;
                else if (r.winner() == 1) sa.losses++;
                else sa.draws++;
                sa.games_won += r.games_a();
                sa.games_lost += r.games_b();
                sa.total_wounds_dealt += r.wounds_dealt_a();
                sa.total_wounds_received += r.wounds_dealt_b();
                sa.total_models_killed += r.models_killed_a;
                sa.total_models_lost += r.models_killed_b;
                sa.total_rounds_holding += r.rounds_holding_a();
                sa.total_rounds_opponent_holding += r.rounds_holding_b();
                sa.total_rounds_played += r.total_rounds;

                // Unit B stats
                auto& sb = stats[r.unit_b_id];
                sb.unit_id = r.unit_b_id;
                sb.matches_played++;
                if (r.winner() == 1) sb.wins++;
                else if (r.winner() == 0) sb.losses++;
                else sb.draws++;
                sb.games_won += r.games_b();
                sb.games_lost += r.games_a();
                sb.total_wounds_dealt += r.wounds_dealt_b();
                sb.total_wounds_received += r.wounds_dealt_a();
                sb.total_models_killed += r.models_killed_b;
                sb.total_models_lost += r.models_killed_a;
                sb.total_rounds_holding += r.rounds_holding_b();
                sb.total_rounds_opponent_holding += r.rounds_holding_a();
                sb.total_rounds_played += r.total_rounds;
            }
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

        const char* format_name = "Compact";
        if (header_.is_extended()) format_name = "Extended (full game stats)";
        else if (header_.is_compact_extended()) format_name = "Compact Extended (compressed game stats)";

        ss << "=== Battle Simulation Results Summary ===\n\n";
        ss << "Format: " << format_name << "\n";
        ss << "Total Results: " << result_count() << "\n";
        ss << "Units A: " << header_.units_a_count << "\n";
        ss << "Units B: " << header_.units_b_count << "\n\n";

        // Count outcomes
        u64 a_wins = 0, b_wins = 0, draws = 0;
        u64 total_wounds = 0, total_models_killed = 0, total_obj_rounds = 0;

        if (header_.is_extended()) {
            for (const auto& r : extended_results_) {
                if (r.winner == 0) a_wins++;
                else if (r.winner == 1) b_wins++;
                else draws++;
                total_wounds += r.wounds_dealt_a + r.wounds_dealt_b;
                total_models_killed += r.models_killed_a + r.models_killed_b;
                total_obj_rounds += r.rounds_holding_a + r.rounds_holding_b;
            }
        } else if (header_.is_compact_extended()) {
            for (const auto& r : compact_extended_results_) {
                if (r.winner() == 0) a_wins++;
                else if (r.winner() == 1) b_wins++;
                else draws++;
                total_wounds += r.wounds_dealt_a() + r.wounds_dealt_b();
                total_models_killed += r.models_killed_a + r.models_killed_b;
                total_obj_rounds += r.rounds_holding_a() + r.rounds_holding_b();
            }
        } else {
            for (const auto& r : results_) {
                if (r.winner == 0) a_wins++;
                else if (r.winner == 1) b_wins++;
                else draws++;
            }
        }

        size_t total = result_count();
        ss << "Outcomes:\n";
        ss << "  Unit A wins: " << a_wins << " ("
           << (total > 0 ? 100.0 * a_wins / total : 0.0) << "%)\n";
        ss << "  Unit B wins: " << b_wins << " ("
           << (total > 0 ? 100.0 * b_wins / total : 0.0) << "%)\n";
        ss << "  Draws: " << draws << " ("
           << (total > 0 ? 100.0 * draws / total : 0.0) << "%)\n";

        // Extended stats summary
        if (header_.has_extended_data() && total > 0) {
            ss << "\nFull Game Statistics:\n";
            ss << "  Avg wounds per match: " << std::fixed << std::setprecision(2)
               << (static_cast<f64>(total_wounds) / total) << "\n";
            ss << "  Avg models killed per match: "
               << (static_cast<f64>(total_models_killed) / total) << "\n";
            ss << "  Avg objective rounds per match: "
               << (static_cast<f64>(total_obj_rounds) / total) << "\n";
        }

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
        ss << "=== Unit Report: " << unit.name.view() << " ===\n\n";
        ss << "Points: " << unit.points_cost << "\n";
        ss << "Quality: " << (int)unit.quality << "+\n";
        ss << "Defense: " << (int)unit.defense << "+\n";
        ss << "Models: " << (int)unit.model_count << "\n\n";

        if (header_.has_extended_data()) {
            auto stats = calculate_extended_unit_stats();
            auto it = stats.find(unit_id);
            if (it != stats.end()) {
                const auto& s = it->second;
                ss << "Performance:\n";
                ss << "  Matches: " << s.matches_played << "\n";
                ss << "  Win Rate: " << std::fixed << std::setprecision(1) << s.win_rate() << "%\n";
                ss << "  Wins/Losses/Draws: " << s.wins << "/" << s.losses << "/" << s.draws << "\n";
                ss << "  Game Win Rate: " << s.game_win_rate() << "%\n\n";

                ss << "Combat Statistics:\n";
                ss << "  Avg wounds dealt: " << std::setprecision(2) << s.avg_wounds_dealt() << "\n";
                ss << "  Avg wounds received: " << s.avg_wounds_received() << "\n";
                ss << "  Damage efficiency: " << s.damage_efficiency() << "x\n";
                ss << "  Avg models killed: " << s.avg_models_killed() << "\n";
                ss << "  Avg models lost: " << s.avg_models_lost() << "\n";
                ss << "  Kill efficiency: " << s.kill_efficiency() << "x\n\n";

                ss << "Objective Control:\n";
                ss << "  Avg rounds holding: " << s.avg_rounds_holding() << "\n";
                ss << "  Objective control rate: " << s.objective_control_rate() << "%\n";
            }
        } else {
            auto stats = calculate_unit_stats();
            auto it = stats.find(unit_id);
            if (it != stats.end()) {
                const auto& s = it->second;
                ss << "Performance:\n";
                ss << "  Matches: " << s.matches_played << "\n";
                ss << "  Win Rate: " << std::fixed << std::setprecision(1) << s.win_rate() << "%\n";
                ss << "  Wins/Losses/Draws: " << s.wins << "/" << s.losses << "/" << s.draws << "\n";
                ss << "  Game Win Rate: " << s.game_win_rate() << "%\n";
            }
        }

        return ss.str();
    }

    // Generate game stats report (works with extended and compact_extended formats)
    std::string generate_game_stats_report(const std::vector<Unit>& units, size_t top_n = 10) const {
        std::ostringstream ss;

        if (!header_.has_extended_data()) {
            ss << "Error: Game stats require extended format results.\n";
            ss << "Use -e or -E flag when running batch_sim to generate extended results.\n";
            return ss.str();
        }

        ss << "=== Full Game Statistics Report ===\n\n";

        auto stats = calculate_extended_unit_stats();

        // Top damage dealers
        std::vector<std::pair<u32, ExtendedUnitStats>> ranked(stats.begin(), stats.end());
        std::sort(ranked.begin(), ranked.end(),
            [](const auto& a, const auto& b) {
                return a.second.avg_wounds_dealt() > b.second.avg_wounds_dealt();
            });

        ss << "Top " << top_n << " Damage Dealers (avg wounds per match):\n";
        for (size_t i = 0; i < std::min(top_n, ranked.size()); ++i) {
            const auto& [id, s] = ranked[i];
            if (id < units.size()) {
                ss << "  " << (i+1) << ". " << units[id].name.view()
                   << " (" << units[id].points_cost << "pts) - "
                   << std::fixed << std::setprecision(2) << s.avg_wounds_dealt()
                   << " avg wounds\n";
            }
        }

        // Top damage efficiency
        std::sort(ranked.begin(), ranked.end(),
            [](const auto& a, const auto& b) {
                return a.second.damage_efficiency() > b.second.damage_efficiency();
            });

        ss << "\nTop " << top_n << " Damage Efficiency (wounds dealt / wounds received):\n";
        for (size_t i = 0; i < std::min(top_n, ranked.size()); ++i) {
            const auto& [id, s] = ranked[i];
            if (id < units.size() && s.total_wounds_received > 0) {
                ss << "  " << (i+1) << ". " << units[id].name.view()
                   << " (" << units[id].points_cost << "pts) - "
                   << std::fixed << std::setprecision(2) << s.damage_efficiency() << "x\n";
            }
        }

        // Top objective controllers
        std::sort(ranked.begin(), ranked.end(),
            [](const auto& a, const auto& b) {
                return a.second.objective_control_rate() > b.second.objective_control_rate();
            });

        ss << "\nTop " << top_n << " Objective Controllers:\n";
        for (size_t i = 0; i < std::min(top_n, ranked.size()); ++i) {
            const auto& [id, s] = ranked[i];
            if (id < units.size() && s.total_rounds_holding > 0) {
                ss << "  " << (i+1) << ". " << units[id].name.view()
                   << " (" << units[id].points_cost << "pts) - "
                   << std::fixed << std::setprecision(1) << s.objective_control_rate() << "%\n";
            }
        }

        return ss.str();
    }

    // Generate extended matchup report (works with extended and compact_extended formats)
    std::string generate_extended_matchup_report(u32 unit_a_id, u32 unit_b_id,
                                                  const std::vector<Unit>& units) const {
        std::ostringstream ss;

        if (!header_.has_extended_data()) {
            ss << "Error: Extended matchup data requires extended format results.\n";
            return ss.str();
        }

        auto stats = get_extended_matchup(unit_a_id, unit_b_id);

        ss << "=== Extended Matchup Report ===\n\n";
        if (unit_a_id < units.size()) {
            ss << "Unit A: " << units[unit_a_id].name.view()
               << " (" << units[unit_a_id].points_cost << "pts)\n";
        } else {
            ss << "Unit A: ID " << unit_a_id << "\n";
        }
        if (unit_b_id < units.size()) {
            ss << "Unit B: " << units[unit_b_id].name.view()
               << " (" << units[unit_b_id].points_cost << "pts)\n";
        } else {
            ss << "Unit B: ID " << unit_b_id << "\n";
        }

        ss << "\nMatch Results:\n";
        ss << "  Total matches: " << stats.total() << "\n";
        ss << "  Unit A wins: " << stats.a_wins << " (" << std::fixed << std::setprecision(1)
           << stats.a_win_rate() << "%)\n";
        ss << "  Unit B wins: " << stats.b_wins << " (" << stats.b_win_rate() << "%)\n";
        ss << "  Draws: " << stats.draws << "\n";

        ss << "\nCombat Statistics:\n";
        ss << "  Unit A avg wounds dealt: " << std::setprecision(2)
           << stats.avg_wounds_a() << "\n";
        ss << "  Unit B avg wounds dealt: " << stats.avg_wounds_b() << "\n";
        ss << "  Unit A avg models killed: " << stats.avg_models_killed_a() << "\n";
        ss << "  Unit B avg models killed: " << stats.avg_models_killed_b() << "\n";

        ss << "\nObjective Control:\n";
        ss << "  Unit A avg rounds holding: " << stats.avg_rounds_holding_a() << "\n";
        ss << "  Unit B avg rounds holding: " << stats.avg_rounds_holding_b() << "\n";

        return ss.str();
    }

    // ===========================================================================
    // Export
    // ===========================================================================

    // Export unit stats to CSV (with extended stats if available)
    // Returns pair: success flag and number of rows written
    std::pair<bool, size_t> export_unit_stats_csv_with_count(const std::string& filename, const std::vector<Unit>& units) const {
        std::ofstream out(filename);
        if (!out) return {false, 0};

        size_t rows_written = 0;

        if (header_.has_extended_data()) {
            auto stats = calculate_extended_unit_stats();

            // Extended header
            out << "unit_id,name,faction,points,quality,defense,models,"
                << "matches,wins,losses,draws,win_rate,game_win_rate,"
                << "avg_wounds_dealt,avg_wounds_received,damage_efficiency,"
                << "avg_models_killed,avg_models_lost,kill_efficiency,"
                << "avg_rounds_holding,objective_control_rate\n";

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
                        << s.game_win_rate() << ","
                        << s.avg_wounds_dealt() << ","
                        << s.avg_wounds_received() << ","
                        << s.damage_efficiency() << ","
                        << s.avg_models_killed() << ","
                        << s.avg_models_lost() << ","
                        << s.kill_efficiency() << ","
                        << s.avg_rounds_holding() << ","
                        << s.objective_control_rate() << "\n";
                    rows_written++;
                }
            }
        } else {
            auto stats = calculate_unit_stats();

            // Basic header
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
                    rows_written++;
                }
            }
        }

        return {true, rows_written};
    }

    // Legacy wrapper for backwards compatibility
    bool export_unit_stats_csv(const std::string& filename, const std::vector<Unit>& units) const {
        auto [success, count] = export_unit_stats_csv_with_count(filename, units);
        return success;
    }

    // Export matchups to CSV (with extended stats if available)
    bool export_matchups_csv(const std::string& filename) const {
        std::ofstream out(filename);
        if (!out) return false;

        if (header_.is_extended()) {
            // Extended header
            out << "unit_a_id,unit_b_id,winner,games_a,games_b,"
                << "wounds_dealt_a,wounds_dealt_b,models_killed_a,models_killed_b,"
                << "rounds_holding_a,rounds_holding_b,total_rounds\n";

            // Data rows
            for (const auto& r : extended_results_) {
                out << r.unit_a_id << ","
                    << r.unit_b_id << ","
                    << (int)r.winner << ","
                    << (int)r.games_a << ","
                    << (int)r.games_b << ","
                    << r.wounds_dealt_a << ","
                    << r.wounds_dealt_b << ","
                    << (int)r.models_killed_a << ","
                    << (int)r.models_killed_b << ","
                    << (int)r.rounds_holding_a << ","
                    << (int)r.rounds_holding_b << ","
                    << (int)r.total_rounds << "\n";
            }
        } else if (header_.is_compact_extended()) {
            // Compact extended header (same columns as extended)
            out << "unit_a_id,unit_b_id,winner,games_a,games_b,"
                << "wounds_dealt_a,wounds_dealt_b,models_killed_a,models_killed_b,"
                << "rounds_holding_a,rounds_holding_b,total_rounds\n";

            // Data rows
            for (const auto& r : compact_extended_results_) {
                out << r.unit_a_id << ","
                    << r.unit_b_id << ","
                    << (int)r.winner() << ","
                    << (int)r.games_a() << ","
                    << (int)r.games_b() << ","
                    << r.wounds_dealt_a() << ","
                    << r.wounds_dealt_b() << ","
                    << (int)r.models_killed_a << ","
                    << (int)r.models_killed_b << ","
                    << (int)r.rounds_holding_a() << ","
                    << (int)r.rounds_holding_b() << ","
                    << (int)r.total_rounds << "\n";
            }
        } else {
            // Basic header
            out << "unit_a_id,unit_b_id,winner,games_a,games_b\n";

            // Data rows
            for (const auto& r : results_) {
                out << r.unit_a_id << ","
                    << r.unit_b_id << ","
                    << r.winner << ","
                    << (int)r.games_a << ","
                    << (int)r.games_b << "\n";
            }
        }

        return true;
    }

    // Export to JSON format (with extended stats if available)
    std::string export_unit_stats_json(const std::vector<Unit>& units) const {
        std::ostringstream ss;

        const char* format_str = "compact";
        if (header_.is_extended()) format_str = "extended";
        else if (header_.is_compact_extended()) format_str = "compact_extended";

        ss << "{\n";
        ss << "  \"format\": \"" << format_str << "\",\n";
        ss << "  \"units\": [\n";

        if (header_.has_extended_data()) {
            auto stats = calculate_extended_unit_stats();
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
                ss << "      \"game_win_rate\": " << s.game_win_rate() << ",\n";
                ss << "      \"avg_wounds_dealt\": " << s.avg_wounds_dealt() << ",\n";
                ss << "      \"avg_wounds_received\": " << s.avg_wounds_received() << ",\n";
                ss << "      \"damage_efficiency\": " << s.damage_efficiency() << ",\n";
                ss << "      \"avg_models_killed\": " << s.avg_models_killed() << ",\n";
                ss << "      \"avg_models_lost\": " << s.avg_models_lost() << ",\n";
                ss << "      \"kill_efficiency\": " << s.kill_efficiency() << ",\n";
                ss << "      \"avg_rounds_holding\": " << s.avg_rounds_holding() << ",\n";
                ss << "      \"objective_control_rate\": " << s.objective_control_rate() << "\n";
                ss << "    }";
            }
        } else {
            auto stats = calculate_unit_stats();
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
        }

        ss << "\n  ]\n}\n";
        return ss.str();
    }

private:
    ResultFileHeader header_{};
    std::vector<CompactMatchResult> results_;
    std::vector<ExtendedMatchResult> extended_results_;
    std::vector<CompactExtendedMatchResult> compact_extended_results_;
    std::vector<AggregatedUnitResult> aggregated_results_;
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
