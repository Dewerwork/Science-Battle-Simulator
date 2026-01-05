#pragma once

#include "export/export_types.hpp"
#include <ostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <map>

namespace battle {
namespace export_data {

// ==============================================================================
// Text Formatter - Human-readable game report
// ==============================================================================

class TextFormatter {
public:
    static void write(std::ostream& out, const GameExport& data) {
        write_header(out, data.metadata);
        write_setup(out, data.setup);
        write_rounds(out, data.rounds);
        write_result(out, data.result);
    }

private:
    static const int LINE_WIDTH = 80;

    static std::string separator(char c = '=') {
        return std::string(LINE_WIDTH, c);
    }

    static std::string center(const std::string& text, int width = LINE_WIDTH) {
        int padding = (width - static_cast<int>(text.length())) / 2;
        if (padding <= 0) return text;
        return std::string(padding, ' ') + text;
    }

    static std::string dice_to_string(const std::vector<DieRoll>& dice) {
        std::string result = "[";
        for (size_t i = 0; i < dice.size(); ++i) {
            result += std::to_string(dice[i].value);
            if (i < dice.size() - 1) result += ", ";
        }
        result += "]";
        return result;
    }

    static void write_header(std::ostream& out, const ExportMetadata& meta) {
        out << separator() << "\n";
        out << center("GAME EXPORT REPORT") << "\n";
        out << separator() << "\n";
        out << "Generated: " << meta.timestamp << "\n";
        out << "Seed: " << meta.seed << "\n";
        out << "Simulator Version: " << meta.simulator_version << "\n";
        out << separator() << "\n\n";
    }

    static void write_setup(std::ostream& out, const GameSetup& setup) {
        out << "UNIT SETUP\n";
        out << separator('-') << "\n\n";

        write_unit_details(out, "UNIT A", setup.unit_a);
        out << "\n";
        write_unit_details(out, "UNIT B", setup.unit_b);

        out << "\nINITIAL POSITIONS\n";
        out << "  Unit A: " << static_cast<int>(setup.positions.unit_a_position)
            << "\" from center (" << (setup.positions.unit_a_position < 0 ? "own side" : "enemy side") << ")\n";
        out << "  Unit B: " << static_cast<int>(setup.positions.unit_b_position)
            << "\" from center (" << (setup.positions.unit_b_position > 0 ? "own side" : "enemy side") << ")\n";
        out << "  Distance between units: " << static_cast<int>(setup.positions.distance_between) << "\"\n";
        out << "\n";
    }

    static void write_unit_details(std::ostream& out, const std::string& label, const UnitExport& unit) {
        out << label << ": " << unit.name;
        if (!unit.faction.empty()) {
            out << " (" << unit.faction << ")";
        }
        out << "\n";

        out << "  Points: " << unit.points_cost
            << " | Quality: " << static_cast<int>(unit.quality) << "+"
            << " | Defense: " << static_cast<int>(unit.defense) << "+"
            << " | AI: " << unit.ai_type << "\n";
        out << "  Models: " << static_cast<int>(unit.model_count);
        if (unit.max_range > 0) {
            out << " | Max Range: " << static_cast<int>(unit.max_range) << "\"";
        }
        out << "\n";

        if (!unit.unit_rules.empty()) {
            out << "  Unit Rules: [";
            for (size_t i = 0; i < unit.unit_rules.size(); ++i) {
                out << unit.unit_rules[i];
                if (i < unit.unit_rules.size() - 1) out << "] [";
            }
            out << "]\n";
        }

        out << "\n";

        // Group similar models
        std::map<std::string, std::vector<size_t>> model_groups;
        for (size_t i = 0; i < unit.models.size(); ++i) {
            const auto& m = unit.models[i];
            std::string key = m.name + "|" + std::to_string(m.quality) + "|" + std::to_string(m.defense) + "|" + std::to_string(m.tough);
            model_groups[key].push_back(i);
        }

        int model_num = 1;
        for (const auto& [key, indices] : model_groups) {
            const auto& model = unit.models[indices[0]];

            if (indices.size() == 1) {
                out << "  [" << model_num << "] " << model.name;
            } else {
                out << "  [" << model_num << "-" << (model_num + indices.size() - 1) << "] "
                    << model.name << " x" << indices.size();
            }

            if (model.is_hero) out << " (Hero)";
            out << "\n";

            out << "      Q:" << static_cast<int>(model.quality) << "+"
                << " D:" << static_cast<int>(model.defense) << "+";
            if (model.tough > 1) {
                out << " Tough(" << static_cast<int>(model.tough) << ")";
            }
            out << "\n";

            if (!model.rules.empty()) {
                out << "      Rules: [";
                for (size_t i = 0; i < model.rules.size(); ++i) {
                    out << model.rules[i];
                    if (i < model.rules.size() - 1) out << "] [";
                }
                out << "]\n";
            }

            if (!model.weapons.empty()) {
                out << "      Weapons:\n";
                for (const auto& w : model.weapons) {
                    out << "        - " << w.name << " (";
                    if (w.range > 0) {
                        out << static_cast<int>(w.range) << "\", ";
                    } else {
                        out << "Melee, ";
                    }
                    out << "A" << static_cast<int>(w.attacks);
                    if (w.ap > 0) {
                        out << ", AP" << static_cast<int>(w.ap);
                    }
                    out << ")";
                    if (!w.rules.empty()) {
                        out << " [";
                        for (size_t i = 0; i < w.rules.size(); ++i) {
                            out << w.rules[i];
                            if (i < w.rules.size() - 1) out << "] [";
                        }
                        out << "]";
                    }
                    out << "\n";
                }
            }

            model_num += indices.size();
        }
    }

    static void write_rounds(std::ostream& out, const std::vector<RoundEvent>& rounds) {
        for (const auto& round : rounds) {
            out << "\n" << separator() << "\n";
            out << center("ROUND " + std::to_string(round.round_number)) << "\n";
            out << separator() << "\n\n";

            // Initiative
            out << "--- Activation Order: ";
            if (round.initiative.unit_a_goes_first) {
                out << "Unit A first";
            } else {
                out << "Unit B first";
            }
            out << " (" << round.initiative.reason << ") ---\n\n";

            // Activations
            for (const auto& act : round.activations) {
                write_activation(out, act);
            }

            // End of round summary
            out << "\n--- End of Round " << static_cast<int>(round.round_number) << " ---\n";
            out << "Objective: " << round.objective_control.holder;
            if (round.objective_control.holder == "contested") {
                out << " (both units in range)";
            }
            out << "\n";
            out << "Unit A: " << static_cast<int>(round.state_at_end.unit_a.models_alive) << "/"
                << static_cast<int>(round.state_at_end.unit_a.models_total) << " models | "
                << round.state_at_end.unit_a.status << "\n";
            out << "Unit B: " << static_cast<int>(round.state_at_end.unit_b.models_alive) << "/"
                << static_cast<int>(round.state_at_end.unit_b.models_total) << " models | "
                << round.state_at_end.unit_b.status << "\n";
        }
    }

    static void write_activation(std::ostream& out, const ActivationEvent& act) {
        out << act.unit_name << " ACTIVATION";
        if (act.is_unit_a) out << " (Unit A)";
        else out << " (Unit B)";
        out << "\n";
        out << separator('-') << "\n";

        // AI decision
        out << "Position: " << static_cast<int>(act.ai_decision.position) << "\" from center\n";
        out << "Action: " << act.action_type << "\n";
        out << "Reason: " << act.ai_decision.reasoning << "\n";

        // Movement
        if (act.has_movement) {
            out << "\n  MOVEMENT\n";
            out << "  " << act.movement.movement_type << ": "
                << static_cast<int>(act.movement.from_position) << "\" -> "
                << static_cast<int>(act.movement.to_position) << "\""
                << " (" << static_cast<int>(act.movement.distance_moved) << "\" moved)\n";
        }

        // Combat
        if (act.has_combat) {
            write_combat(out, act.combat);
        }

        // Counter attack
        if (act.has_counter_attack) {
            out << "\n  COUNTER-ATTACK\n";
            write_combat(out, act.counter_attack, true);
        }

        // Morale
        if (act.has_morale_check) {
            write_morale(out, act.morale);
        }

        out << "\n";
    }

    static void write_combat(std::ostream& out, const CombatEvent& combat, bool is_counter = false) {
        std::string indent = is_counter ? "    " : "  ";

        if (!is_counter) {
            out << "\n" << indent << combat.phase << " COMBAT vs " << combat.defender_name << "\n";
            out << indent << separator('-').substr(0, 40) << "\n";
        }

        // Impact
        if (combat.has_impact && combat.impact.effective_impact > 0) {
            out << "\n" << indent << "Impact Attacks:\n";
            out << indent << "  Impact value: " << static_cast<int>(combat.impact.impact_value);
            if (combat.impact.counter_reduction > 0) {
                out << " (reduced by " << static_cast<int>(combat.impact.counter_reduction) << " from Counter)";
            }
            out << " = " << static_cast<int>(combat.impact.effective_impact) << " attacks\n";
            out << indent << "  Roll (2+): " << dice_to_string(combat.impact.impact_roll.dice)
                << " -> " << combat.impact.impact_hits << " hits\n";
            if (combat.impact.impact_hits > 0) {
                out << indent << "  Defense: " << dice_to_string(combat.impact.impact_defense_roll.dice)
                    << " -> " << combat.impact.impact_wounds << " wounds\n";
            }
        }

        // Weapon attacks
        for (const auto& attack : combat.attacks) {
            out << "\n" << indent << attack.attacker_model_name << " attacks with " << attack.weapon.name << ":\n";

            // Attacks
            out << indent << "  Attacks: " << static_cast<int>(attack.base_attacks);
            if (!attack.bonus_hits.empty()) {
                for (const auto& [rule, count] : attack.bonus_hits) {
                    out << " +" << count << " (" << rule << ")";
                }
            }
            out << " = " << static_cast<int>(attack.total_attacks) << " total\n";

            // To hit roll
            out << indent << "  To Hit (" << static_cast<int>(attack.hit_roll.effective_target) << "+): "
                << dice_to_string(attack.hit_roll.dice);
            out << " -> " << attack.hits_before_modifiers << " hits";
            if (attack.hit_roll.total_sixes > 0) {
                out << " (" << attack.hit_roll.total_sixes << " sixes)";
            }
            out << "\n";

            // Triggered rules
            if (!attack.triggered_rules.empty()) {
                out << indent << "  Triggered: ";
                for (size_t i = 0; i < attack.triggered_rules.size(); ++i) {
                    out << attack.triggered_rules[i];
                    if (i < attack.triggered_rules.size() - 1) out << ", ";
                }
                out << "\n";
            }

            if (attack.hits_after_modifiers != attack.hits_before_modifiers) {
                out << indent << "  Final hits: " << attack.hits_after_modifiers;
                if (attack.rending_hits > 0) {
                    out << " (" << attack.rending_hits << " with Rending AP+" << 4 << ")";
                }
                out << "\n";
            }

            // Defense rolls
            if (!attack.defense_roll.dice.empty()) {
                out << indent << "  Defense (";
                out << static_cast<int>(attack.defense_roll.effective_target) << "+, AP"
                    << static_cast<int>(attack.effective_ap) << "): "
                    << dice_to_string(attack.defense_roll.dice);
                out << " -> " << attack.wounds_from_normal_hits << " wounds\n";
            }

            if (!attack.rending_defense_roll.dice.empty()) {
                out << indent << "  Rending Defense ("
                    << static_cast<int>(attack.rending_defense_roll.effective_target) << "+, AP"
                    << static_cast<int>(attack.effective_ap + 4) << "): "
                    << dice_to_string(attack.rending_defense_roll.dice);
                out << " -> " << attack.wounds_from_rending_hits << " wounds\n";
            }

            out << indent << "  Total wounds: " << attack.total_wounds << "\n";
        }

        // Wound allocation
        if (!combat.wound_allocation.allocations.empty()) {
            out << "\n" << indent << "Wound Allocation:\n";
            if (combat.wound_allocation.total_wounds_regenerated > 0) {
                out << indent << "  Regeneration saved " << combat.wound_allocation.total_wounds_regenerated << " wounds\n";
            }
            for (const auto& alloc : combat.wound_allocation.allocations) {
                out << indent << "  -> " << alloc.model_name << ": "
                    << static_cast<int>(alloc.wounds_allocated) << " wound(s)";
                if (alloc.model_killed) {
                    out << " -> KILLED";
                }
                out << "\n";
            }
        }

        // Summary
        out << "\n" << indent << "Combat Summary: " << combat.total_wounds_dealt << " wounds dealt, "
            << combat.total_models_killed << " models killed";
        if (combat.target_destroyed) out << " [TARGET DESTROYED]";
        else if (combat.target_routed) out << " [TARGET ROUTED]";
        else if (combat.target_shaken) out << " [TARGET SHAKEN]";
        out << "\n";
    }

    static void write_morale(std::ostream& out, const MoraleEvent& morale) {
        out << "\n  MORALE CHECK\n";
        out << "  " << separator('-').substr(0, 40) << "\n";
        out << "  Trigger: " << morale.trigger_reason;
        if (morale.is_from_melee) {
            out << " (took " << morale.wounds_taken << " wounds, dealt " << morale.wounds_dealt << ")";
        }
        out << "\n";

        out << "  Test (" << static_cast<int>(morale.quality_target) << "+): rolled "
            << static_cast<int>(morale.morale_roll.dice[0].value);
        if (morale.initial_pass) {
            out << " -> PASSED\n";
        } else {
            out << " -> FAILED";
            if (morale.has_fearless && morale.fearless_reroll.dice.size() > 0) {
                out << "\n  Fearless reroll (4+): rolled "
                    << static_cast<int>(morale.fearless_reroll.dice[0].value);
                if (morale.fearless_pass) {
                    out << " -> PASSED";
                } else {
                    out << " -> FAILED";
                }
            }
            out << "\n";
        }

        out << "  Result: " << morale.result << " (" << morale.previous_status << " -> " << morale.new_status << ")\n";
    }

    static void write_result(std::ostream& out, const GameResultExport& result) {
        out << "\n" << separator() << "\n";
        out << center("GAME RESULT") << "\n";
        out << separator() << "\n\n";

        std::string winner_name;
        if (result.winner == "unit_a") winner_name = "UNIT A";
        else if (result.winner == "unit_b") winner_name = "UNIT B";
        else winner_name = "DRAW";

        out << "WINNER: " << winner_name << "\n";
        out << "Victory Type: " << result.victory_type << "\n";
        out << "Rounds Played: " << static_cast<int>(result.rounds_played) << "\n";
        out << "Ending: " << result.ending_condition << "\n\n";

        out << "Final State:\n";
        out << "  Unit A: " << static_cast<int>(result.unit_a_stats.models_remaining)
            << " models remaining, " << result.unit_a_stats.final_status << "\n";
        out << "  Unit B: " << static_cast<int>(result.unit_b_stats.models_remaining)
            << " models remaining, " << result.unit_b_stats.final_status << "\n\n";

        out << "Statistics:\n";
        out << std::setw(25) << "" << std::setw(15) << "Unit A" << std::setw(15) << "Unit B" << "\n";
        out << std::setw(25) << "Wounds Dealt:"
            << std::setw(15) << result.unit_a_stats.total_wounds_dealt
            << std::setw(15) << result.unit_b_stats.total_wounds_dealt << "\n";
        out << std::setw(25) << "Models Killed:"
            << std::setw(15) << static_cast<int>(result.unit_a_stats.total_models_killed)
            << std::setw(15) << static_cast<int>(result.unit_b_stats.total_models_killed) << "\n";
        out << std::setw(25) << "Obj Rounds:"
            << std::setw(15) << static_cast<int>(result.unit_a_stats.rounds_holding_objective)
            << std::setw(15) << static_cast<int>(result.unit_b_stats.rounds_holding_objective) << "\n";
        out << std::setw(25) << "First Blood:"
            << std::setw(15) << (result.unit_a_stats.first_blood ? "Yes" : "No")
            << std::setw(15) << (result.unit_b_stats.first_blood ? "Yes" : "No") << "\n";

        out << "\n" << separator() << "\n";
    }
};

} // namespace export_data
} // namespace battle
