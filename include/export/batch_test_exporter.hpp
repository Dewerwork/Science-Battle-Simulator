#pragma once

#include "export/structured_match_logger.hpp"
#include "export/test_file_parser.hpp"
#include "core/unit.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>

namespace battle {

// ==============================================================================
// Iteration Result - Stores one iteration's MatchData with context
// ==============================================================================

struct IterationResult {
    int matchup_id = 0;
    int iteration = 0;
    MatchData match_data;
};

// ==============================================================================
// Matchup Result - All iterations for one matchup
// ==============================================================================

struct MatchupResult {
    int matchup_id = 0;
    TestMatchup definition;
    Unit unit_a;
    Unit unit_b;
    std::vector<IterationResult> iterations;
};

// ==============================================================================
// BatchTestExporter - Exports results to multiple CSV files
// ==============================================================================

class BatchTestExporter {
public:
    // Export all results to CSV files in the specified directory
    static bool export_results(const std::vector<MatchupResult>& results,
                               const std::string& output_dir) {
        // Create output directory if it doesn't exist
        std::filesystem::create_directories(output_dir);

        bool success = true;
        success &= export_matchups(results, output_dir + "/matchups.csv");
        success &= export_iterations(results, output_dir + "/iterations.csv");
        success &= export_rounds(results, output_dir + "/rounds.csv");
        success &= export_movements(results, output_dir + "/movements.csv");
        success &= export_morale(results, output_dir + "/morale.csv");
        success &= export_attacks(results, output_dir + "/attacks.csv");
        success &= export_rule_triggers(results, output_dir + "/rule_triggers.csv");
        success &= export_rolls(results, output_dir + "/rolls.csv");
        success &= export_spells(results, output_dir + "/spells.csv");
        success &= export_spell_tokens(results, output_dir + "/spell_tokens.csv");

        return success;
    }

private:
    // Escape CSV field (handle commas and quotes)
    static std::string escape_csv(const std::string& str) {
        if (str.find(',') == std::string::npos &&
            str.find('"') == std::string::npos &&
            str.find('\n') == std::string::npos) {
            return str;
        }

        std::string escaped = "\"";
        for (char c : str) {
            if (c == '"') {
                escaped += "\"\"";
            } else {
                escaped += c;
            }
        }
        escaped += "\"";
        return escaped;
    }

    // Format rules as comma-separated string
    static std::string format_rules(const Unit& unit) {
        std::ostringstream ss;
        bool first = true;
        for (size_t i = 0; i < unit.rule_count; ++i) {
            if (!first) ss << ", ";
            first = false;
            ss << rule_id_to_string(unit.rules[i].id);
            if (unit.rules[i].value > 0) {
                ss << "(" << static_cast<int>(unit.rules[i].value) << ")";
            }
        }
        return ss.str();
    }

    // Format weapons as string
    static std::string format_weapons(const Unit& unit) {
        std::ostringstream ss;
        bool first = true;
        for (size_t i = 0; i < unit.weapon_count; ++i) {
            if (!first) ss << ", ";
            first = false;
            ss << unit.weapons[i].name.c_str();
            ss << "(A" << static_cast<int>(unit.weapons[i].attacks);
            if (unit.weapons[i].ap != 0) {
                ss << " AP" << static_cast<int>(unit.weapons[i].ap);
            }
            ss << ")";
        }
        return ss.str();
    }

    // Convert rule ID to string
    static std::string rule_id_to_string(RuleId id) {
        switch (id) {
            case RuleId::None: return "None";
            case RuleId::Precise: return "Precise";
            case RuleId::Reliable: return "Reliable";
            case RuleId::Rending: return "Rending";
            case RuleId::Blast: return "Blast";
            case RuleId::Deadly: return "Deadly";
            case RuleId::Poison: return "Poison";
            case RuleId::Bane: return "Bane";
            case RuleId::AP: return "AP";
            case RuleId::Lance: return "Lance";
            case RuleId::Impact: return "Impact";
            case RuleId::Surge: return "Surge";
            case RuleId::Thrust: return "Thrust";
            case RuleId::Regeneration: return "Regeneration";
            case RuleId::Tough: return "Tough";
            case RuleId::Stealth: return "Stealth";
            case RuleId::Protected: return "Protected";
            case RuleId::Shielded: return "Shielded";
            case RuleId::ShieldWall: return "ShieldWall";
            case RuleId::Resistance: return "Resistance";
            case RuleId::Relentless: return "Relentless";
            case RuleId::Furious: return "Furious";
            case RuleId::PointBlankSurge: return "PointBlankSurge";
            case RuleId::PredatorFighter: return "PredatorFighter";
            case RuleId::Rupture: return "Rupture";
            case RuleId::Shred: return "Shred";
            case RuleId::Smash: return "Smash";
            case RuleId::Takedown: return "Takedown";
            case RuleId::SelfDestruct: return "SelfDestruct";
            case RuleId::Fast: return "Fast";
            case RuleId::Slow: return "Slow";
            case RuleId::Flying: return "Flying";
            case RuleId::Strider: return "Strider";
            case RuleId::Agile: return "Agile";
            case RuleId::RapidCharge: return "RapidCharge";
            case RuleId::Fearless: return "Fearless";
            case RuleId::NoRetreat: return "NoRetreat";
            case RuleId::MoraleBoost: return "MoraleBoost";
            case RuleId::HoldTheLine: return "HoldTheLine";
            case RuleId::Fear: return "Fear";
            case RuleId::GoodShot: return "GoodShot";
            case RuleId::BadShot: return "BadShot";
            case RuleId::Limited: return "Limited";
            case RuleId::Counter: return "Counter";
            case RuleId::HitAndRun: return "HitAndRun";
            case RuleId::Indirect: return "Indirect";
            case RuleId::Sniper: return "Sniper";
            case RuleId::Hero: return "Hero";
            case RuleId::Scout: return "Scout";
            case RuleId::Ambush: return "Ambush";
            case RuleId::Immobile: return "Immobile";
            case RuleId::Devout: return "Devout";
            case RuleId::Battleborn: return "Battleborn";
            case RuleId::Casting: return "Casting";
            case RuleId::HiveBond: return "HiveBond";
            case RuleId::Teleport: return "Teleport";
            case RuleId::PiercingTag: return "PiercingTag";
            default: return "Unknown(" + std::to_string(static_cast<int>(id)) + ")";
        }
    }

    // Export matchups.csv
    static bool export_matchups(const std::vector<MatchupResult>& results,
                                const std::string& filepath) {
        std::ofstream file(filepath);
        if (!file.is_open()) return false;

        // Header
        file << "matchup_id,matchup_name,iterations,"
             << "unit_a_name,unit_a_models,unit_a_quality,unit_a_defense,unit_a_points,unit_a_rules,unit_a_weapons,"
             << "unit_b_name,unit_b_models,unit_b_quality,unit_b_defense,unit_b_points,unit_b_rules,unit_b_weapons\n";

        for (const auto& result : results) {
            file << result.matchup_id << ","
                 << escape_csv(result.definition.name) << ","
                 << result.definition.iterations << ","
                 << escape_csv(result.unit_a.name.c_str()) << ","
                 << static_cast<int>(result.unit_a.model_count) << ","
                 << static_cast<int>(result.unit_a.quality) << ","
                 << static_cast<int>(result.unit_a.defense) << ","
                 << result.unit_a.points_cost << ","
                 << escape_csv(format_rules(result.unit_a)) << ","
                 << escape_csv(format_weapons(result.unit_a)) << ","
                 << escape_csv(result.unit_b.name.c_str()) << ","
                 << static_cast<int>(result.unit_b.model_count) << ","
                 << static_cast<int>(result.unit_b.quality) << ","
                 << static_cast<int>(result.unit_b.defense) << ","
                 << result.unit_b.points_cost << ","
                 << escape_csv(format_rules(result.unit_b)) << ","
                 << escape_csv(format_weapons(result.unit_b)) << "\n";
        }

        return true;
    }

    // Export iterations.csv
    static bool export_iterations(const std::vector<MatchupResult>& results,
                                  const std::string& filepath) {
        std::ofstream file(filepath);
        if (!file.is_open()) return false;

        // Header
        file << "matchup_id,iteration,seed,winner,games_won_a,games_won_b,"
             << "total_wounds_dealt_a,total_wounds_dealt_b,"
             << "total_models_killed_a,total_models_killed_b\n";

        for (const auto& result : results) {
            for (const auto& iter : result.iterations) {
                const auto& match = iter.match_data;
                file << iter.matchup_id << ","
                     << iter.iteration << ","
                     << match.seed << ","
                     << escape_csv(match.overall_winner) << ","
                     << static_cast<int>(match.games_won_a) << ","
                     << static_cast<int>(match.games_won_b) << ","
                     << match.total_wounds_dealt_a << ","
                     << match.total_wounds_dealt_b << ","
                     << match.total_models_killed_a << ","
                     << match.total_models_killed_b << "\n";
            }
        }

        return true;
    }

    // Export rounds.csv - round-level data including objective control
    static bool export_rounds(const std::vector<MatchupResult>& results,
                              const std::string& filepath) {
        std::ofstream file(filepath);
        if (!file.is_open()) return false;

        // Header
        file << "matchup_id,iteration,game,round,"
             << "initiative_roll,unit_a_first,initiative_reason,"
             << "a_controls_objective,b_controls_objective,control_reason,"
             << "unit_a_models_remaining,unit_b_models_remaining,"
             << "unit_a_status,unit_b_status\n";

        for (const auto& result : results) {
            for (const auto& iter : result.iterations) {
                const auto& match = iter.match_data;

                for (size_t game_idx = 0; game_idx < match.games.size(); ++game_idx) {
                    const auto& game = match.games[game_idx];

                    for (const auto& round : game.rounds) {
                        file << iter.matchup_id << ","
                             << iter.iteration << ","
                             << (game_idx + 1) << ","
                             << static_cast<int>(round.round_number) << ","
                             << static_cast<int>(round.initiative_roll) << ","
                             << (round.unit_a_first ? 1 : 0) << ","
                             << escape_csv(round.initiative_reason) << ","
                             << (round.a_controls ? 1 : 0) << ","
                             << (round.b_controls ? 1 : 0) << ","
                             << escape_csv(round.control_reason) << ","
                             << static_cast<int>(round.unit_a_models) << ","
                             << static_cast<int>(round.unit_b_models) << ","
                             << escape_csv(round.unit_a_status) << ","
                             << escape_csv(round.unit_b_status) << "\n";
                    }
                }
            }
        }

        return true;
    }

    // Export movements.csv - movement events
    static bool export_movements(const std::vector<MatchupResult>& results,
                                 const std::string& filepath) {
        std::ofstream file(filepath);
        if (!file.is_open()) return false;

        // Header
        file << "matchup_id,iteration,game,round,unit,move_seq,"
             << "move_type,from_pos,to_pos,distance_moved,reason\n";

        for (const auto& result : results) {
            for (const auto& iter : result.iterations) {
                const auto& match = iter.match_data;

                for (size_t game_idx = 0; game_idx < match.games.size(); ++game_idx) {
                    const auto& game = match.games[game_idx];

                    for (const auto& round : game.rounds) {
                        for (const auto& activation : round.activations) {
                            int move_seq = 0;
                            std::string unit = activation.is_unit_a ? "A" : "B";

                            for (const auto& movement : activation.movements) {
                                file << iter.matchup_id << ","
                                     << iter.iteration << ","
                                     << (game_idx + 1) << ","
                                     << static_cast<int>(round.round_number) << ","
                                     << unit << ","
                                     << move_seq << ","
                                     << escape_csv(movement.move_type) << ","
                                     << static_cast<int>(movement.from_pos) << ","
                                     << static_cast<int>(movement.to_pos) << ","
                                     << static_cast<int>(movement.distance_moved) << ","
                                     << escape_csv(movement.reason) << "\n";
                                move_seq++;
                            }
                        }
                    }
                }
            }
        }

        return true;
    }

    // Export morale.csv - morale check details
    static bool export_morale(const std::vector<MatchupResult>& results,
                              const std::string& filepath) {
        std::ofstream file(filepath);
        if (!file.is_open()) return false;

        // Header
        file << "matchup_id,iteration,game,round,unit,morale_seq,"
             << "unit_name,trigger_reason,models_remaining,models_total,"
             << "wounds_taken,wounds_dealt,"
             << "morale_roll,morale_target,morale_passed,"
             << "had_fearless_reroll,fearless_roll,fearless_target,fearless_passed,"
             << "final_passed,old_status,new_status,result_description\n";

        for (const auto& result : results) {
            for (const auto& iter : result.iterations) {
                const auto& match = iter.match_data;

                for (size_t game_idx = 0; game_idx < match.games.size(); ++game_idx) {
                    const auto& game = match.games[game_idx];

                    for (const auto& round : game.rounds) {
                        for (const auto& activation : round.activations) {
                            int morale_seq = 0;
                            std::string unit = activation.is_unit_a ? "A" : "B";

                            for (const auto& morale : activation.morale_checks) {
                                file << iter.matchup_id << ","
                                     << iter.iteration << ","
                                     << (game_idx + 1) << ","
                                     << static_cast<int>(round.round_number) << ","
                                     << unit << ","
                                     << morale_seq << ","
                                     << escape_csv(morale.unit_name) << ","
                                     << escape_csv(morale.trigger_reason) << ","
                                     << static_cast<int>(morale.models_remaining) << ","
                                     << static_cast<int>(morale.models_total) << ","
                                     << morale.wounds_taken << ","
                                     << morale.wounds_dealt << ","
                                     << static_cast<int>(morale.morale_roll) << ","
                                     << static_cast<int>(morale.morale_target) << ","
                                     << (morale.morale_passed ? 1 : 0) << ","
                                     << (morale.had_fearless_reroll ? 1 : 0) << ","
                                     << static_cast<int>(morale.fearless_roll) << ","
                                     << static_cast<int>(morale.fearless_target) << ","
                                     << (morale.fearless_passed ? 1 : 0) << ","
                                     << (morale.final_passed ? 1 : 0) << ","
                                     << escape_csv(morale.old_status) << ","
                                     << escape_csv(morale.new_status) << ","
                                     << escape_csv(morale.result_description) << "\n";
                                morale_seq++;
                            }
                        }
                    }
                }
            }
        }

        return true;
    }

    // Export attacks.csv
    static bool export_attacks(const std::vector<MatchupResult>& results,
                               const std::string& filepath) {
        std::ofstream file(filepath);
        if (!file.is_open()) return false;

        // Header
        file << "matchup_id,iteration,game,round,attacker,attack_seq,"
             << "weapon,is_melee,distance,models_attacking,total_attacks,"
             << "hit_target,hits,hit_sixes,"
             << "ap_applied,defense_target,saves,wounds,"
             << "wounds_allocated,models_killed,overkill\n";

        for (const auto& result : results) {
            for (const auto& iter : result.iterations) {
                const auto& match = iter.match_data;

                for (size_t game_idx = 0; game_idx < match.games.size(); ++game_idx) {
                    const auto& game = match.games[game_idx];

                    for (const auto& round : game.rounds) {
                        for (const auto& activation : round.activations) {
                            int attack_seq = 0;
                            std::string attacker = activation.is_unit_a ? "A" : "B";

                            for (const auto& attack : activation.attack_sequences) {
                                file << iter.matchup_id << ","
                                     << iter.iteration << ","
                                     << (game_idx + 1) << ","
                                     << static_cast<int>(round.round_number) << ","
                                     << attacker << ","
                                     << attack_seq << ","
                                     << escape_csv(attack.weapon_name) << ","
                                     << (attack.is_melee ? 1 : 0) << ","
                                     << static_cast<int>(attack.distance) << ","
                                     << static_cast<int>(attack.models_attacking) << ","
                                     << attack.total_attacks << ","
                                     << static_cast<int>(attack.hit_rolls.target) << ","
                                     << attack.hit_rolls.successes << ","
                                     << attack.hit_rolls.sixes << ","
                                     << static_cast<int>(attack.total_ap) << ","
                                     << static_cast<int>(attack.defense_rolls.target) << ","
                                     << attack.saves << ","
                                     << attack.wounds << ","
                                     << attack.wounds_allocated << ","
                                     << attack.models_killed << ","
                                     << attack.overkill_wounds << "\n";

                                attack_seq++;
                            }
                        }
                    }
                }
            }
        }

        return true;
    }

    // Export rule_triggers.csv
    static bool export_rule_triggers(const std::vector<MatchupResult>& results,
                                     const std::string& filepath) {
        std::ofstream file(filepath);
        if (!file.is_open()) return false;

        // Header
        file << "matchup_id,iteration,game,round,attacker,attack_seq,"
             << "rule_name,effect,value,phase\n";

        for (const auto& result : results) {
            for (const auto& iter : result.iterations) {
                const auto& match = iter.match_data;

                for (size_t game_idx = 0; game_idx < match.games.size(); ++game_idx) {
                    const auto& game = match.games[game_idx];

                    // Deployment rules
                    for (const auto& trigger : game.deployment_rules) {
                        file << iter.matchup_id << ","
                             << iter.iteration << ","
                             << (game_idx + 1) << ","
                             << 0 << ","  // round 0 = deployment
                             << (trigger.phase.find("_a") != std::string::npos ? "A" : "B") << ","
                             << 0 << ","  // no attack seq
                             << escape_csv(trigger.rule_name) << ","
                             << escape_csv(trigger.effect) << ","
                             << trigger.value << ","
                             << escape_csv(trigger.phase) << "\n";
                    }

                    // Movement rules
                    for (const auto& trigger : game.movement_rules) {
                        file << iter.matchup_id << ","
                             << iter.iteration << ","
                             << (game_idx + 1) << ","
                             << 0 << ","
                             << (trigger.phase.find("_a") != std::string::npos ? "A" : "B") << ","
                             << 0 << ","
                             << escape_csv(trigger.rule_name) << ","
                             << escape_csv(trigger.effect) << ","
                             << trigger.value << ","
                             << escape_csv(trigger.phase) << "\n";
                    }

                    for (const auto& round : game.rounds) {
                        // End round rules
                        for (const auto& trigger : round.end_round_rules) {
                            file << iter.matchup_id << ","
                                 << iter.iteration << ","
                                 << (game_idx + 1) << ","
                                 << static_cast<int>(round.round_number) << ","
                                 << (trigger.phase.find("_a") != std::string::npos ? "A" : "B") << ","
                                 << 0 << ","
                                 << escape_csv(trigger.rule_name) << ","
                                 << escape_csv(trigger.effect) << ","
                                 << trigger.value << ","
                                 << escape_csv(trigger.phase) << "\n";
                        }

                        for (const auto& activation : round.activations) {
                            std::string attacker = activation.is_unit_a ? "A" : "B";

                            // Activation-level rule triggers
                            for (const auto& trigger : activation.rule_triggers) {
                                file << iter.matchup_id << ","
                                     << iter.iteration << ","
                                     << (game_idx + 1) << ","
                                     << static_cast<int>(round.round_number) << ","
                                     << attacker << ","
                                     << 0 << ","
                                     << escape_csv(trigger.rule_name) << ","
                                     << escape_csv(trigger.effect) << ","
                                     << trigger.value << ","
                                     << escape_csv(trigger.phase) << "\n";
                            }

                            // Attack-level rule triggers
                            int attack_seq = 0;
                            for (const auto& attack : activation.attack_sequences) {
                                for (const auto& trigger : attack.rule_triggers) {
                                    file << iter.matchup_id << ","
                                         << iter.iteration << ","
                                         << (game_idx + 1) << ","
                                         << static_cast<int>(round.round_number) << ","
                                         << attacker << ","
                                         << attack_seq << ","
                                         << escape_csv(trigger.rule_name) << ","
                                         << escape_csv(trigger.effect) << ","
                                         << trigger.value << ","
                                         << escape_csv(trigger.phase) << "\n";
                                }
                                attack_seq++;
                            }
                        }
                    }
                }
            }
        }

        return true;
    }

    // Export rolls.csv (individual dice rolls)
    static bool export_rolls(const std::vector<MatchupResult>& results,
                             const std::string& filepath) {
        std::ofstream file(filepath);
        if (!file.is_open()) return false;

        // Header - added reroll_value column
        file << "matchup_id,iteration,game,round,attacker,attack_seq,"
             << "phase,roll_index,roll_value,reroll_value,target,success\n";

        for (const auto& result : results) {
            for (const auto& iter : result.iterations) {
                const auto& match = iter.match_data;

                for (size_t game_idx = 0; game_idx < match.games.size(); ++game_idx) {
                    const auto& game = match.games[game_idx];

                    for (const auto& round : game.rounds) {
                        for (const auto& activation : round.activations) {
                            std::string attacker = activation.is_unit_a ? "A" : "B";
                            int attack_seq = 0;

                            for (const auto& attack : activation.attack_sequences) {
                                // Hit rolls (no rerolls)
                                for (size_t i = 0; i < attack.hit_rolls.rolls.size(); ++i) {
                                    u8 roll = attack.hit_rolls.rolls[i];
                                    bool success = roll >= attack.hit_rolls.target;
                                    file << iter.matchup_id << ","
                                         << iter.iteration << ","
                                         << (game_idx + 1) << ","
                                         << static_cast<int>(round.round_number) << ","
                                         << attacker << ","
                                         << attack_seq << ","
                                         << "hit," << i << ","
                                         << static_cast<int>(roll) << ","
                                         << ","  // No reroll for hit rolls
                                         << static_cast<int>(attack.hit_rolls.target) << ","
                                         << (success ? 1 : 0) << "\n";
                                }

                                // Defense rolls (with rerolls for 6s when Poison/Bane active)
                                size_t reroll_idx = 0;
                                for (size_t i = 0; i < attack.defense_rolls.rolls.size(); ++i) {
                                    u8 roll = attack.defense_rolls.rolls[i];
                                    bool success;
                                    std::string reroll_str;

                                    // If this roll was a 6 and we have rerolls, success is based on reroll
                                    if (roll == 6 && reroll_idx < attack.defense_rolls.rerolls.size()) {
                                        u8 reroll_value = attack.defense_rolls.rerolls[reroll_idx];
                                        reroll_str = std::to_string(static_cast<int>(reroll_value));
                                        success = reroll_value >= attack.defense_rolls.target;
                                        reroll_idx++;
                                    } else {
                                        success = roll >= attack.defense_rolls.target;
                                    }

                                    file << iter.matchup_id << ","
                                         << iter.iteration << ","
                                         << (game_idx + 1) << ","
                                         << static_cast<int>(round.round_number) << ","
                                         << attacker << ","
                                         << attack_seq << ","
                                         << "defense," << i << ","
                                         << static_cast<int>(roll) << ","
                                         << reroll_str << ","
                                         << static_cast<int>(attack.defense_rolls.target) << ","
                                         << (success ? 1 : 0) << "\n";
                                }

                                // Rending defense rolls (no rerolls)
                                for (size_t i = 0; i < attack.rending_defense_rolls.rolls.size(); ++i) {
                                    u8 roll = attack.rending_defense_rolls.rolls[i];
                                    bool success = roll >= attack.rending_defense_rolls.target;
                                    file << iter.matchup_id << ","
                                         << iter.iteration << ","
                                         << (game_idx + 1) << ","
                                         << static_cast<int>(round.round_number) << ","
                                         << attacker << ","
                                         << attack_seq << ","
                                         << "rending_defense," << i << ","
                                         << static_cast<int>(roll) << ","
                                         << ","  // No reroll for rending defense
                                         << static_cast<int>(attack.rending_defense_rolls.target) << ","
                                         << (success ? 1 : 0) << "\n";
                                }

                                // Regeneration rolls (no rerolls)
                                for (size_t i = 0; i < attack.regen_rolls.rolls.size(); ++i) {
                                    u8 roll = attack.regen_rolls.rolls[i];
                                    bool success = roll >= attack.regen_rolls.target;
                                    file << iter.matchup_id << ","
                                         << iter.iteration << ","
                                         << (game_idx + 1) << ","
                                         << static_cast<int>(round.round_number) << ","
                                         << attacker << ","
                                         << attack_seq << ","
                                         << "regeneration," << i << ","
                                         << static_cast<int>(roll) << ","
                                         << ","  // No reroll for regeneration
                                         << static_cast<int>(attack.regen_rolls.target) << ","
                                         << (success ? 1 : 0) << "\n";
                                }

                                attack_seq++;
                            }
                        }
                    }
                }
            }
        }

        return true;
    }

    // Export spells.csv - individual spell cast attempts
    static bool export_spells(const std::vector<MatchupResult>& results,
                              const std::string& filepath) {
        std::ofstream file(filepath);
        if (!file.is_open()) return false;

        // Header
        file << "matchup_id,iteration,game,round,caster,spell_seq,"
             << "spell_name,spell_cost,tokens_before,tokens_after,range,target_type,"
             << "was_interfered,interference_tokens,interference_modifier,"
             << "roll,target_number,total_modifier,success,"
             << "effect_type,hits_dealt,wounds_dealt,models_killed,buff_applied\n";

        for (const auto& result : results) {
            for (const auto& iter : result.iterations) {
                const auto& match = iter.match_data;

                for (size_t game_idx = 0; game_idx < match.games.size(); ++game_idx) {
                    const auto& game = match.games[game_idx];

                    for (const auto& round : game.rounds) {
                        for (const auto& activation : round.activations) {
                            std::string caster = activation.is_unit_a ? "A" : "B";
                            int spell_seq = 0;

                            for (const auto& spell : activation.spell_casts) {
                                file << iter.matchup_id << ","
                                     << iter.iteration << ","
                                     << (game_idx + 1) << ","
                                     << static_cast<int>(round.round_number) << ","
                                     << caster << ","
                                     << spell_seq << ","
                                     << escape_csv(spell.spell_name) << ","
                                     << static_cast<int>(spell.spell_cost) << ","
                                     << static_cast<int>(spell.tokens_before) << ","
                                     << static_cast<int>(spell.tokens_after) << ","
                                     << static_cast<int>(spell.range) << ","
                                     << escape_csv(spell.target_type) << ","
                                     << (spell.was_interfered ? 1 : 0) << ","
                                     << static_cast<int>(spell.interference_tokens) << ","
                                     << static_cast<int>(spell.interference_modifier) << ","
                                     << static_cast<int>(spell.roll) << ","
                                     << static_cast<int>(spell.target_number) << ","
                                     << static_cast<int>(spell.total_modifier) << ","
                                     << (spell.success ? 1 : 0) << ","
                                     << escape_csv(spell.effect_type) << ","
                                     << spell.hits_dealt << ","
                                     << spell.wounds_dealt << ","
                                     << static_cast<int>(spell.models_killed) << ","
                                     << escape_csv(spell.buff_applied) << "\n";

                                spell_seq++;
                            }
                        }
                    }
                }
            }
        }

        return true;
    }

    // Export spell_tokens.csv - spell token grant/spend events
    static bool export_spell_tokens(const std::vector<MatchupResult>& results,
                                    const std::string& filepath) {
        std::ofstream file(filepath);
        if (!file.is_open()) return false;

        // Header
        file << "matchup_id,iteration,game,round,unit,token_seq,"
             << "event_type,tokens_changed,tokens_before,tokens_after,"
             << "caster_value,spell_name\n";

        for (const auto& result : results) {
            for (const auto& iter : result.iterations) {
                const auto& match = iter.match_data;

                for (size_t game_idx = 0; game_idx < match.games.size(); ++game_idx) {
                    const auto& game = match.games[game_idx];

                    for (const auto& round : game.rounds) {
                        for (const auto& activation : round.activations) {
                            std::string unit = activation.is_unit_a ? "A" : "B";
                            int token_seq = 0;

                            for (const auto& token : activation.spell_tokens) {
                                // Token events may belong to either unit
                                std::string token_unit = token.is_unit_a ? "A" : "B";

                                file << iter.matchup_id << ","
                                     << iter.iteration << ","
                                     << (game_idx + 1) << ","
                                     << static_cast<int>(round.round_number) << ","
                                     << token_unit << ","
                                     << token_seq << ","
                                     << escape_csv(token.event_type) << ","
                                     << static_cast<int>(token.tokens_changed) << ","
                                     << static_cast<int>(token.tokens_before) << ","
                                     << static_cast<int>(token.tokens_after) << ","
                                     << static_cast<int>(token.caster_value) << ","
                                     << escape_csv(token.spell_name) << "\n";

                                token_seq++;
                            }
                        }
                    }
                }
            }
        }

        return true;
    }
};

} // namespace battle
