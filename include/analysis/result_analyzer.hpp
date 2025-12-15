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

    // ===========================================================================
    // Comprehensive Export - Full Unit Information
    // ===========================================================================

    // Helper: Convert RuleId to string name
    static std::string rule_id_to_string(RuleId id) {
        switch (id) {
            case RuleId::None: return "None";
            case RuleId::AP: return "AP";
            case RuleId::Blast: return "Blast";
            case RuleId::Deadly: return "Deadly";
            case RuleId::Lance: return "Lance";
            case RuleId::Poison: return "Poison";
            case RuleId::Precise: return "Precise";
            case RuleId::Reliable: return "Reliable";
            case RuleId::Rending: return "Rending";
            case RuleId::Bane: return "Bane";
            case RuleId::Impact: return "Impact";
            case RuleId::Indirect: return "Indirect";
            case RuleId::Sniper: return "Sniper";
            case RuleId::Lock_On: return "Lock-On";
            case RuleId::Purge: return "Purge";
            case RuleId::Regeneration: return "Regeneration";
            case RuleId::Tough: return "Tough";
            case RuleId::Protected: return "Protected";
            case RuleId::Stealth: return "Stealth";
            case RuleId::ShieldWall: return "Shield Wall";
            case RuleId::Fearless: return "Fearless";
            case RuleId::Furious: return "Furious";
            case RuleId::Hero: return "Hero";
            case RuleId::Relentless: return "Relentless";
            case RuleId::Fear: return "Fear";
            case RuleId::Counter: return "Counter";
            case RuleId::Fast: return "Fast";
            case RuleId::Flying: return "Flying";
            case RuleId::Strider: return "Strider";
            case RuleId::Scout: return "Scout";
            case RuleId::Ambush: return "Ambush";
            case RuleId::Devout: return "Devout";
            case RuleId::PiercingAssault: return "Piercing Assault";
            case RuleId::Unstoppable: return "Unstoppable";
            case RuleId::Casting: return "Casting";
            case RuleId::Slow: return "Slow";
            case RuleId::Surge: return "Surge";
            case RuleId::Thrust: return "Thrust";
            case RuleId::Takedown: return "Takedown";
            case RuleId::Limited: return "Limited";
            case RuleId::Shielded: return "Shielded";
            case RuleId::Resistance: return "Resistance";
            case RuleId::NoRetreat: return "No Retreat";
            case RuleId::MoraleBoost: return "Morale Boost";
            case RuleId::Rupture: return "Rupture";
            case RuleId::Agile: return "Agile";
            case RuleId::HitAndRun: return "Hit & Run";
            case RuleId::PointBlankSurge: return "Point-Blank Surge";
            case RuleId::Shred: return "Shred";
            case RuleId::Smash: return "Smash";
            case RuleId::Battleborn: return "Battleborn";
            case RuleId::PredatorFighter: return "Predator Fighter";
            case RuleId::RapidCharge: return "Rapid Charge";
            case RuleId::SelfDestruct: return "Self-Destruct";
            case RuleId::VersatileAttack: return "Versatile Attack";
            case RuleId::GoodShot: return "Good Shot";
            case RuleId::BadShot: return "Bad Shot";
            case RuleId::MeleeEvasion: return "Melee Evasion";
            case RuleId::MeleeShrouding: return "Melee Shrouding";
            case RuleId::RangedShrouding: return "Ranged Shrouding";
            default: return "Unknown";
        }
    }

    // Helper: Format rule with value
    static std::string format_rule(const CompactRule& rule) {
        std::string name = rule_id_to_string(rule.id);
        if (rule.value > 0) {
            return name + "(" + std::to_string(rule.value) + ")";
        }
        return name;
    }

    // Helper: Get unit rules as string
    static std::string get_unit_rules_string(const Unit& u) {
        std::ostringstream ss;
        bool first = true;
        for (u8 i = 0; i < u.rule_count; ++i) {
            if (u.rules[i].id != RuleId::None) {
                if (!first) ss << ", ";
                first = false;
                ss << format_rule(u.rules[i]);
            }
        }
        return ss.str();
    }

    // Helper: Get weapon info as string
    static std::string get_weapon_string(const Weapon& w) {
        std::ostringstream ss;
        if (w.range > 0) {
            ss << w.range << "\" ";
        }
        ss << w.name.view() << " (A" << (int)w.attacks;
        if (w.ap > 0) {
            ss << ", AP(" << (int)w.ap << ")";
        }
        // Add weapon rules
        for (u8 i = 0; i < w.rule_count; ++i) {
            if (w.rules[i].id != RuleId::None && w.rules[i].id != RuleId::AP) {
                ss << ", " << format_rule(w.rules[i]);
            }
        }
        ss << ")";
        return ss.str();
    }

    // Helper: Get all weapons as string
    static std::string get_all_weapons_string(const Unit& u) {
        std::ostringstream ss;
        bool first = true;
        for (u8 i = 0; i < u.weapon_count; ++i) {
            if (!first) ss << "; ";
            first = false;
            ss << get_weapon_string(u.weapons[i]);
        }
        return ss.str();
    }

    // Helper: Get AI type as string
    static std::string ai_type_to_string(AIType type) {
        switch (type) {
            case AIType::Melee: return "Melee";
            case AIType::Shooting: return "Shooting";
            case AIType::Hybrid: return "Hybrid";
            default: return "Unknown";
        }
    }

    // Export comprehensive unit stats to CSV (includes all unit info)
    bool export_full_unit_stats_csv(const std::string& filename, const std::vector<Unit>& units) const {
        std::ofstream out(filename);
        if (!out) return false;

        auto stats = calculate_unit_stats();

        // Comprehensive header
        out << "unit_id,name,faction,points,quality,defense,models,tough,max_range,ai_type,"
            << "rules,weapons,"
            << "matches,wins,losses,draws,win_rate,loss_rate,draw_rate,"
            << "games_won,games_lost,game_win_rate,"
            << "points_efficiency\n";

        // Data rows
        for (const auto& [id, s] : stats) {
            if (id < units.size()) {
                const Unit& u = units[id];

                // Calculate tough value (from first model or rule)
                u8 tough = 1;
                if (u.model_count > 0) {
                    tough = u.models[0].tough;
                }

                // Points efficiency = win_rate / (points / 100)
                f64 points_efficiency = u.points_cost > 0
                    ? s.win_rate() / (u.points_cost / 100.0)
                    : 0.0;

                out << id << ","
                    << "\"" << u.name.view() << "\","
                    << "\"" << u.faction.view() << "\","
                    << u.points_cost << ","
                    << (int)u.quality << ","
                    << (int)u.defense << ","
                    << (int)u.model_count << ","
                    << (int)tough << ","
                    << (int)u.max_range << ","
                    << "\"" << ai_type_to_string(u.ai_type) << "\","
                    << "\"" << get_unit_rules_string(u) << "\","
                    << "\"" << get_all_weapons_string(u) << "\","
                    << s.matches_played << ","
                    << s.wins << ","
                    << s.losses << ","
                    << s.draws << ","
                    << s.win_rate() << ","
                    << (s.matches_played > 0 ? 100.0 * s.losses / s.matches_played : 0.0) << ","
                    << s.draw_rate() << ","
                    << s.games_won << ","
                    << s.games_lost << ","
                    << s.game_win_rate() << ","
                    << points_efficiency << "\n";
            }
        }

        return true;
    }

    // Export comprehensive unit stats to JSON (includes all unit info)
    std::string export_full_unit_stats_json(const std::vector<Unit>& units) const {
        std::ostringstream ss;
        auto stats = calculate_unit_stats();

        ss << "{\n  \"units\": [\n";

        bool first = true;
        for (const auto& [id, s] : stats) {
            if (id >= units.size()) continue;

            if (!first) ss << ",\n";
            first = false;

            const Unit& u = units[id];

            // Calculate tough value
            u8 tough = 1;
            if (u.model_count > 0) {
                tough = u.models[0].tough;
            }

            // Points efficiency
            f64 points_efficiency = u.points_cost > 0
                ? s.win_rate() / (u.points_cost / 100.0)
                : 0.0;

            ss << "    {\n";

            // Unit identification
            ss << "      \"id\": " << id << ",\n";
            ss << "      \"name\": \"" << u.name.view() << "\",\n";
            ss << "      \"faction\": \"" << u.faction.view() << "\",\n";

            // Unit stats
            ss << "      \"stats\": {\n";
            ss << "        \"points\": " << u.points_cost << ",\n";
            ss << "        \"quality\": " << (int)u.quality << ",\n";
            ss << "        \"defense\": " << (int)u.defense << ",\n";
            ss << "        \"models\": " << (int)u.model_count << ",\n";
            ss << "        \"tough\": " << (int)tough << ",\n";
            ss << "        \"max_range\": " << (int)u.max_range << ",\n";
            ss << "        \"ai_type\": \"" << ai_type_to_string(u.ai_type) << "\"\n";
            ss << "      },\n";

            // Rules array
            ss << "      \"rules\": [";
            bool first_rule = true;
            for (u8 i = 0; i < u.rule_count; ++i) {
                if (u.rules[i].id != RuleId::None) {
                    if (!first_rule) ss << ", ";
                    first_rule = false;
                    ss << "{\"name\": \"" << rule_id_to_string(u.rules[i].id) << "\"";
                    if (u.rules[i].value > 0) {
                        ss << ", \"value\": " << (int)u.rules[i].value;
                    }
                    ss << "}";
                }
            }
            ss << "],\n";

            // Weapons array
            ss << "      \"weapons\": [\n";
            for (u8 i = 0; i < u.weapon_count; ++i) {
                const Weapon& w = u.weapons[i];
                if (i > 0) ss << ",\n";
                ss << "        {\n";
                ss << "          \"name\": \"" << w.name.view() << "\",\n";
                ss << "          \"attacks\": " << (int)w.attacks << ",\n";
                ss << "          \"ap\": " << (int)w.ap << ",\n";
                ss << "          \"range\": " << (int)w.range << ",\n";
                ss << "          \"rules\": [";
                bool first_wrule = true;
                for (u8 j = 0; j < w.rule_count; ++j) {
                    if (w.rules[j].id != RuleId::None) {
                        if (!first_wrule) ss << ", ";
                        first_wrule = false;
                        ss << "{\"name\": \"" << rule_id_to_string(w.rules[j].id) << "\"";
                        if (w.rules[j].value > 0) {
                            ss << ", \"value\": " << (int)w.rules[j].value;
                        }
                        ss << "}";
                    }
                }
                ss << "]\n";
                ss << "        }";
            }
            ss << "\n      ],\n";

            // Performance statistics
            ss << "      \"performance\": {\n";
            ss << "        \"matches\": " << s.matches_played << ",\n";
            ss << "        \"wins\": " << s.wins << ",\n";
            ss << "        \"losses\": " << s.losses << ",\n";
            ss << "        \"draws\": " << s.draws << ",\n";
            ss << "        \"win_rate\": " << s.win_rate() << ",\n";
            ss << "        \"loss_rate\": " << (s.matches_played > 0 ? 100.0 * s.losses / s.matches_played : 0.0) << ",\n";
            ss << "        \"draw_rate\": " << s.draw_rate() << ",\n";
            ss << "        \"games_won\": " << s.games_won << ",\n";
            ss << "        \"games_lost\": " << s.games_lost << ",\n";
            ss << "        \"game_win_rate\": " << s.game_win_rate() << ",\n";
            ss << "        \"points_efficiency\": " << points_efficiency << "\n";
            ss << "      }\n";

            ss << "    }";
        }

        ss << "\n  ]\n}\n";
        return ss.str();
    }

    // Export matchups with full unit names to CSV
    bool export_full_matchups_csv(const std::string& filename,
                                   const std::vector<Unit>& units_a,
                                   const std::vector<Unit>& units_b) const {
        std::ofstream out(filename);
        if (!out) return false;

        // Header with unit names
        out << "unit_a_id,unit_a_name,unit_a_faction,unit_a_points,"
            << "unit_b_id,unit_b_name,unit_b_faction,unit_b_points,"
            << "winner,winner_name,games_a,games_b\n";

        // Data rows
        for (const auto& r : results_) {
            std::string a_name = r.unit_a_id < units_a.size()
                ? std::string(units_a[r.unit_a_id].name.view()) : "Unknown";
            std::string a_faction = r.unit_a_id < units_a.size()
                ? std::string(units_a[r.unit_a_id].faction.view()) : "";
            u16 a_points = r.unit_a_id < units_a.size()
                ? units_a[r.unit_a_id].points_cost : 0;

            std::string b_name = r.unit_b_id < units_b.size()
                ? std::string(units_b[r.unit_b_id].name.view()) : "Unknown";
            std::string b_faction = r.unit_b_id < units_b.size()
                ? std::string(units_b[r.unit_b_id].faction.view()) : "";
            u16 b_points = r.unit_b_id < units_b.size()
                ? units_b[r.unit_b_id].points_cost : 0;

            std::string winner_name = "Draw";
            if (r.winner == 0) winner_name = a_name;
            else if (r.winner == 1) winner_name = b_name;

            out << r.unit_a_id << ","
                << "\"" << a_name << "\","
                << "\"" << a_faction << "\","
                << a_points << ","
                << r.unit_b_id << ","
                << "\"" << b_name << "\","
                << "\"" << b_faction << "\","
                << b_points << ","
                << r.winner << ","
                << "\"" << winner_name << "\","
                << (int)r.games_a << ","
                << (int)r.games_b << "\n";
        }

        return true;
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
