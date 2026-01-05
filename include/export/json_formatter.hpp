#pragma once

#include "export/export_types.hpp"
#include <ostream>
#include <string>
#include <sstream>
#include <iomanip>

namespace battle {
namespace export_data {

// ==============================================================================
// Lightweight JSON Formatter - No external dependencies
// ==============================================================================

class JsonFormatter {
public:
    static void write(std::ostream& out, const GameExport& data) {
        out << "{\n";
        write_metadata(out, data.metadata, 1);
        out << ",\n";
        write_setup(out, data.setup, 1);
        out << ",\n";
        write_rounds(out, data.rounds, 1);
        out << ",\n";
        write_result(out, data.result, 1);
        out << "\n}\n";
    }

private:
    static std::string indent(int level) {
        return std::string(level * 2, ' ');
    }

    static std::string escape_string(const std::string& s) {
        std::string result;
        for (char c : s) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default: result += c;
            }
        }
        return result;
    }

    static void write_string(std::ostream& out, const std::string& key, const std::string& value, int level, bool last = false) {
        out << indent(level) << "\"" << key << "\": \"" << escape_string(value) << "\"";
        if (!last) out << ",";
        out << "\n";
    }

    static void write_number(std::ostream& out, const std::string& key, auto value, int level, bool last = false) {
        out << indent(level) << "\"" << key << "\": " << static_cast<int>(value);
        if (!last) out << ",";
        out << "\n";
    }

    static void write_bool(std::ostream& out, const std::string& key, bool value, int level, bool last = false) {
        out << indent(level) << "\"" << key << "\": " << (value ? "true" : "false");
        if (!last) out << ",";
        out << "\n";
    }

    static void write_string_array(std::ostream& out, const std::string& key, const std::vector<std::string>& arr, int level, bool last = false) {
        out << indent(level) << "\"" << key << "\": [";
        for (size_t i = 0; i < arr.size(); ++i) {
            out << "\"" << escape_string(arr[i]) << "\"";
            if (i < arr.size() - 1) out << ", ";
        }
        out << "]";
        if (!last) out << ",";
        out << "\n";
    }

    static void write_metadata(std::ostream& out, const ExportMetadata& meta, int level) {
        out << indent(level) << "\"metadata\": {\n";
        write_string(out, "export_version", meta.export_version, level + 1);
        write_string(out, "timestamp", meta.timestamp, level + 1);
        write_string(out, "simulator_version", meta.simulator_version, level + 1);
        write_number(out, "seed", meta.seed, level + 1, true);
        out << indent(level) << "}";
    }

    static void write_weapon_export(std::ostream& out, const WeaponExport& w, int level, bool last = false) {
        out << indent(level) << "{\n";
        write_string(out, "name", w.name, level + 1);
        write_number(out, "range", w.range, level + 1);
        write_number(out, "attacks", w.attacks, level + 1);
        write_number(out, "ap", w.ap, level + 1);
        write_string_array(out, "rules", w.rules, level + 1, true);
        out << indent(level) << "}";
        if (!last) out << ",";
        out << "\n";
    }

    static void write_model_export(std::ostream& out, const ModelExport& m, int level, bool last = false) {
        out << indent(level) << "{\n";
        write_string(out, "name", m.name, level + 1);
        write_number(out, "quality", m.quality, level + 1);
        write_number(out, "defense", m.defense, level + 1);
        write_number(out, "tough", m.tough, level + 1);
        write_bool(out, "is_hero", m.is_hero, level + 1);
        write_string_array(out, "rules", m.rules, level + 1);

        out << indent(level + 1) << "\"weapons\": [\n";
        for (size_t i = 0; i < m.weapons.size(); ++i) {
            write_weapon_export(out, m.weapons[i], level + 2, i == m.weapons.size() - 1);
        }
        out << indent(level + 1) << "]\n";

        out << indent(level) << "}";
        if (!last) out << ",";
        out << "\n";
    }

    static void write_unit_export(std::ostream& out, const std::string& key, const UnitExport& u, int level, bool last = false) {
        out << indent(level) << "\"" << key << "\": {\n";
        write_string(out, "name", u.name, level + 1);
        write_string(out, "faction", u.faction, level + 1);
        write_number(out, "unit_id", u.unit_id, level + 1);
        write_number(out, "points_cost", u.points_cost, level + 1);
        write_string(out, "ai_type", u.ai_type, level + 1);
        write_number(out, "quality", u.quality, level + 1);
        write_number(out, "defense", u.defense, level + 1);
        write_number(out, "model_count", u.model_count, level + 1);
        write_number(out, "max_range", u.max_range, level + 1);
        write_string_array(out, "unit_rules", u.unit_rules, level + 1);

        out << indent(level + 1) << "\"models\": [\n";
        for (size_t i = 0; i < u.models.size(); ++i) {
            write_model_export(out, u.models[i], level + 2, i == u.models.size() - 1);
        }
        out << indent(level + 1) << "]\n";

        out << indent(level) << "}";
        if (!last) out << ",";
        out << "\n";
    }

    static void write_setup(std::ostream& out, const GameSetup& setup, int level) {
        out << indent(level) << "\"setup\": {\n";
        write_unit_export(out, "unit_a", setup.unit_a, level + 1);
        write_unit_export(out, "unit_b", setup.unit_b, level + 1);

        out << indent(level + 1) << "\"initial_positions\": {\n";
        write_number(out, "unit_a_position", setup.positions.unit_a_position, level + 2);
        write_number(out, "unit_b_position", setup.positions.unit_b_position, level + 2);
        write_number(out, "distance_between", setup.positions.distance_between, level + 2);
        write_number(out, "objective_position", setup.positions.objective_position, level + 2, true);
        out << indent(level + 1) << "}\n";

        out << indent(level) << "}";
    }

    static void write_die_roll(std::ostream& out, const DieRoll& die, int level, bool last = false) {
        out << indent(level) << "{\"value\": " << static_cast<int>(die.value)
            << ", \"is_success\": " << (die.is_success ? "true" : "false")
            << ", \"is_six\": " << (die.is_six ? "true" : "false") << "}";
        if (!last) out << ",";
        out << "\n";
    }

    static void write_dice_roll_event(std::ostream& out, const std::string& key, const DiceRollEvent& roll, int level, bool last = false) {
        if (roll.dice.empty()) {
            out << indent(level) << "\"" << key << "\": null";
            if (!last) out << ",";
            out << "\n";
            return;
        }

        out << indent(level) << "\"" << key << "\": {\n";
        write_string(out, "context", roll.context, level + 1);
        write_number(out, "target", roll.target, level + 1);
        write_number(out, "modifier", roll.modifier, level + 1);
        write_number(out, "effective_target", roll.effective_target, level + 1);
        write_number(out, "ap_value", roll.ap_value, level + 1);
        write_bool(out, "reroll_sixes", roll.reroll_sixes, level + 1);
        write_number(out, "total_successes", roll.total_successes, level + 1);
        write_number(out, "total_sixes", roll.total_sixes, level + 1);

        out << indent(level + 1) << "\"dice\": [\n";
        for (size_t i = 0; i < roll.dice.size(); ++i) {
            write_die_roll(out, roll.dice[i], level + 2, i == roll.dice.size() - 1);
        }
        out << indent(level + 1) << "]\n";

        out << indent(level) << "}";
        if (!last) out << ",";
        out << "\n";
    }

    static void write_attack_sequence(std::ostream& out, const AttackSequence& attack, int level, bool last = false) {
        out << indent(level) << "{\n";
        write_string(out, "attacker_model_name", attack.attacker_model_name, level + 1);
        write_number(out, "attacker_model_index", attack.attacker_model_index, level + 1);

        // Weapon info
        out << indent(level + 1) << "\"weapon\": {\n";
        write_string(out, "name", attack.weapon.name, level + 2);
        write_number(out, "range", attack.weapon.range, level + 2);
        write_number(out, "attacks", attack.weapon.attacks, level + 2);
        write_number(out, "ap", attack.weapon.ap, level + 2);
        write_bool(out, "is_melee", attack.weapon.is_melee, level + 2);
        write_string_array(out, "rules", attack.weapon.rules, level + 2, true);
        out << indent(level + 1) << "},\n";

        write_number(out, "base_attacks", attack.base_attacks, level + 1);
        write_number(out, "total_attacks", attack.total_attacks, level + 1);

        write_dice_roll_event(out, "hit_roll", attack.hit_roll, level + 1);
        write_number(out, "hits_before_modifiers", attack.hits_before_modifiers, level + 1);
        write_number(out, "hits_after_modifiers", attack.hits_after_modifiers, level + 1);
        write_number(out, "rending_hits", attack.rending_hits, level + 1);

        write_dice_roll_event(out, "defense_roll", attack.defense_roll, level + 1);
        write_dice_roll_event(out, "rending_defense_roll", attack.rending_defense_roll, level + 1);

        write_number(out, "base_ap", attack.base_ap, level + 1);
        write_number(out, "effective_ap", attack.effective_ap, level + 1);
        write_number(out, "wounds_from_normal_hits", attack.wounds_from_normal_hits, level + 1);
        write_number(out, "wounds_from_rending_hits", attack.wounds_from_rending_hits, level + 1);
        write_number(out, "total_wounds", attack.total_wounds, level + 1);
        write_string_array(out, "triggered_rules", attack.triggered_rules, level + 1, true);

        out << indent(level) << "}";
        if (!last) out << ",";
        out << "\n";
    }

    static void write_model_wound_event(std::ostream& out, const ModelWoundEvent& evt, int level, bool last = false) {
        out << indent(level) << "{\n";
        write_string(out, "model_name", evt.model_name, level + 1);
        write_number(out, "model_index", evt.model_index, level + 1);
        write_number(out, "wounds_allocated", evt.wounds_allocated, level + 1);
        write_number(out, "tough_value", evt.tough_value, level + 1);
        write_number(out, "wounds_before", evt.wounds_before, level + 1);
        write_number(out, "wounds_after", evt.wounds_after, level + 1);
        write_bool(out, "model_killed", evt.model_killed, level + 1, true);
        out << indent(level) << "}";
        if (!last) out << ",";
        out << "\n";
    }

    static void write_wound_allocation(std::ostream& out, const std::string& key, const WoundAllocationSequence& seq, int level, bool last = false) {
        out << indent(level) << "\"" << key << "\": {\n";
        write_number(out, "total_wounds_to_allocate", seq.total_wounds_to_allocate, level + 1);
        write_string_array(out, "allocation_order", seq.allocation_order, level + 1);

        out << indent(level + 1) << "\"allocations\": [\n";
        for (size_t i = 0; i < seq.allocations.size(); ++i) {
            write_model_wound_event(out, seq.allocations[i], level + 2, i == seq.allocations.size() - 1);
        }
        out << indent(level + 1) << "],\n";

        write_number(out, "total_wounds_dealt", seq.total_wounds_dealt, level + 1);
        write_number(out, "total_wounds_regenerated", seq.total_wounds_regenerated, level + 1);
        write_number(out, "total_models_killed", seq.total_models_killed, level + 1);
        write_number(out, "overkill_wounds", seq.overkill_wounds, level + 1, true);
        out << indent(level) << "}";
        if (!last) out << ",";
        out << "\n";
    }

    static void write_combat_event(std::ostream& out, const std::string& key, const CombatEvent& combat, int level, bool last = false) {
        out << indent(level) << "\"" << key << "\": {\n";
        write_string(out, "phase", combat.phase, level + 1);
        write_string(out, "attacker_name", combat.attacker_name, level + 1);
        write_string(out, "defender_name", combat.defender_name, level + 1);
        write_bool(out, "is_charging", combat.is_charging, level + 1);
        write_bool(out, "has_impact", combat.has_impact, level + 1);

        out << indent(level + 1) << "\"attacks\": [\n";
        for (size_t i = 0; i < combat.attacks.size(); ++i) {
            write_attack_sequence(out, combat.attacks[i], level + 2, i == combat.attacks.size() - 1);
        }
        out << indent(level + 1) << "],\n";

        write_wound_allocation(out, "wound_allocation", combat.wound_allocation, level + 1);
        write_number(out, "total_wounds_dealt", combat.total_wounds_dealt, level + 1);
        write_number(out, "total_models_killed", combat.total_models_killed, level + 1);
        write_bool(out, "target_destroyed", combat.target_destroyed, level + 1);
        write_bool(out, "target_shaken", combat.target_shaken, level + 1);
        write_bool(out, "target_routed", combat.target_routed, level + 1, true);
        out << indent(level) << "}";
        if (!last) out << ",";
        out << "\n";
    }

    static void write_morale_event(std::ostream& out, const MoraleEvent& morale, int level, bool last = false) {
        out << indent(level) << "\"morale\": {\n";
        write_string(out, "unit_name", morale.unit_name, level + 1);
        write_string(out, "trigger_reason", morale.trigger_reason, level + 1);
        write_bool(out, "is_from_melee", morale.is_from_melee, level + 1);
        write_number(out, "quality_target", morale.quality_target, level + 1);
        write_dice_roll_event(out, "morale_roll", morale.morale_roll, level + 1);
        write_bool(out, "initial_pass", morale.initial_pass, level + 1);
        write_bool(out, "has_fearless", morale.has_fearless, level + 1);
        write_dice_roll_event(out, "fearless_reroll", morale.fearless_reroll, level + 1);
        write_bool(out, "fearless_pass", morale.fearless_pass, level + 1);
        write_bool(out, "final_pass", morale.final_pass, level + 1);
        write_string(out, "result", morale.result, level + 1);
        write_string(out, "previous_status", morale.previous_status, level + 1);
        write_string(out, "new_status", morale.new_status, level + 1, true);
        out << indent(level) << "}";
        if (!last) out << ",";
        out << "\n";
    }

    static void write_model_snapshot(std::ostream& out, const ModelSnapshot& snap, int level, bool last = false) {
        out << indent(level) << "{\n";
        write_string(out, "name", snap.name, level + 1);
        write_number(out, "index", snap.index, level + 1);
        write_number(out, "wounds_taken", snap.wounds_taken, level + 1);
        write_number(out, "wounds_remaining", snap.wounds_remaining, level + 1);
        write_string(out, "state", snap.state, level + 1);
        write_bool(out, "is_hero", snap.is_hero, level + 1, true);
        out << indent(level) << "}";
        if (!last) out << ",";
        out << "\n";
    }

    static void write_unit_snapshot(std::ostream& out, const std::string& key, const UnitSnapshot& snap, int level, bool last = false) {
        out << indent(level) << "\"" << key << "\": {\n";
        write_string(out, "name", snap.name, level + 1);
        write_string(out, "faction", snap.faction, level + 1);
        write_number(out, "unit_id", snap.unit_id, level + 1);
        write_number(out, "points_cost", snap.points_cost, level + 1);
        write_string(out, "ai_type", snap.ai_type, level + 1);
        write_number(out, "quality", snap.quality, level + 1);
        write_number(out, "defense", snap.defense, level + 1);
        write_string(out, "status", snap.status, level + 1);
        write_bool(out, "is_fatigued", snap.is_fatigued, level + 1);
        write_number(out, "position", snap.position, level + 1);
        write_number(out, "models_alive", snap.models_alive, level + 1);
        write_number(out, "models_total", snap.models_total, level + 1);
        write_number(out, "wounds_remaining", snap.wounds_remaining, level + 1);

        out << indent(level + 1) << "\"models\": [\n";
        for (size_t i = 0; i < snap.models.size(); ++i) {
            write_model_snapshot(out, snap.models[i], level + 2, i == snap.models.size() - 1);
        }
        out << indent(level + 1) << "]\n";

        out << indent(level) << "}";
        if (!last) out << ",";
        out << "\n";
    }

    static void write_game_state_snapshot(std::ostream& out, const std::string& key, const GameStateSnapshot& snap, int level, bool last = false) {
        out << indent(level) << "\"" << key << "\": {\n";
        write_unit_snapshot(out, "unit_a", snap.unit_a, level + 1);
        write_unit_snapshot(out, "unit_b", snap.unit_b, level + 1);
        write_bool(out, "in_melee", snap.in_melee, level + 1);
        write_number(out, "distance_between", snap.distance_between, level + 1);
        write_string(out, "objective_holder", snap.objective_holder, level + 1, true);
        out << indent(level) << "}";
        if (!last) out << ",";
        out << "\n";
    }

    static void write_ai_decision(std::ostream& out, const AIDecisionEvent& ai, int level, bool last = false) {
        out << indent(level) << "\"ai_decision\": {\n";
        write_string(out, "unit_name", ai.unit_name, level + 1);
        write_string(out, "ai_type", ai.ai_type, level + 1);
        write_string(out, "current_status", ai.current_status, level + 1);
        write_number(out, "position", ai.position, level + 1);
        write_number(out, "distance_to_enemy", ai.distance_to_enemy, level + 1);
        write_number(out, "distance_to_objective", ai.distance_to_objective, level + 1);
        write_bool(out, "controls_objective", ai.controls_objective, level + 1);
        write_bool(out, "enemy_destroyed", ai.enemy_destroyed, level + 1);
        write_bool(out, "in_melee", ai.in_melee, level + 1);
        write_string(out, "decision", ai.decision, level + 1);
        write_string(out, "reasoning", ai.reasoning, level + 1, true);
        out << indent(level) << "}";
        if (!last) out << ",";
        out << "\n";
    }

    static void write_movement(std::ostream& out, const MovementEvent& move, int level, bool last = false) {
        out << indent(level) << "\"movement\": {\n";
        write_string(out, "unit_name", move.unit_name, level + 1);
        write_string(out, "movement_type", move.movement_type, level + 1);
        write_number(out, "from_position", move.from_position, level + 1);
        write_number(out, "to_position", move.to_position, level + 1);
        write_number(out, "distance_moved", move.distance_moved, level + 1);
        write_string(out, "reason", move.reason, level + 1, true);
        out << indent(level) << "}";
        if (!last) out << ",";
        out << "\n";
    }

    static void write_activation(std::ostream& out, const ActivationEvent& act, int level, bool last = false) {
        out << indent(level) << "{\n";
        write_string(out, "unit_name", act.unit_name, level + 1);
        write_bool(out, "is_unit_a", act.is_unit_a, level + 1);
        write_string(out, "action_type", act.action_type, level + 1);

        write_ai_decision(out, act.ai_decision, level + 1);

        write_bool(out, "has_movement", act.has_movement, level + 1);
        if (act.has_movement) {
            write_movement(out, act.movement, level + 1);
        }

        write_bool(out, "has_combat", act.has_combat, level + 1);
        if (act.has_combat) {
            write_combat_event(out, "combat", act.combat, level + 1);
        }

        write_bool(out, "has_counter_attack", act.has_counter_attack, level + 1);
        if (act.has_counter_attack) {
            write_combat_event(out, "counter_attack", act.counter_attack, level + 1);
        }

        write_bool(out, "has_morale_check", act.has_morale_check, level + 1);
        if (act.has_morale_check) {
            write_morale_event(out, act.morale, level + 1);
        }

        write_game_state_snapshot(out, "state_after", act.state_after, level + 1, true);

        out << indent(level) << "}";
        if (!last) out << ",";
        out << "\n";
    }

    static void write_round(std::ostream& out, const RoundEvent& round, int level, bool last = false) {
        out << indent(level) << "{\n";
        write_number(out, "round_number", round.round_number, level + 1);
        write_game_state_snapshot(out, "state_at_start", round.state_at_start, level + 1);

        // Initiative
        out << indent(level + 1) << "\"initiative\": {\n";
        write_bool(out, "is_round_one", round.initiative.is_round_one, level + 2);
        write_bool(out, "unit_a_goes_first", round.initiative.unit_a_goes_first, level + 2);
        write_string(out, "reason", round.initiative.reason, level + 2, true);
        out << indent(level + 1) << "},\n";

        write_string_array(out, "activation_order", round.activation_order, level + 1);

        out << indent(level + 1) << "\"activations\": [\n";
        for (size_t i = 0; i < round.activations.size(); ++i) {
            write_activation(out, round.activations[i], level + 2, i == round.activations.size() - 1);
        }
        out << indent(level + 1) << "],\n";

        // Objective control
        out << indent(level + 1) << "\"objective_control\": {\n";
        write_number(out, "unit_a_distance", round.objective_control.unit_a_distance, level + 2);
        write_number(out, "unit_b_distance", round.objective_control.unit_b_distance, level + 2);
        write_bool(out, "unit_a_in_range", round.objective_control.unit_a_in_range, level + 2);
        write_bool(out, "unit_b_in_range", round.objective_control.unit_b_in_range, level + 2);
        write_string(out, "holder", round.objective_control.holder, level + 2);
        write_string(out, "reason", round.objective_control.reason, level + 2, true);
        out << indent(level + 1) << "},\n";

        write_game_state_snapshot(out, "state_at_end", round.state_at_end, level + 1, true);

        out << indent(level) << "}";
        if (!last) out << ",";
        out << "\n";
    }

    static void write_rounds(std::ostream& out, const std::vector<RoundEvent>& rounds, int level) {
        out << indent(level) << "\"rounds\": [\n";
        for (size_t i = 0; i < rounds.size(); ++i) {
            write_round(out, rounds[i], level + 1, i == rounds.size() - 1);
        }
        out << indent(level) << "]";
    }

    static void write_unit_final_stats(std::ostream& out, const std::string& key, const UnitFinalStats& stats, int level, bool last = false) {
        out << indent(level) << "\"" << key << "\": {\n";
        write_number(out, "total_wounds_dealt", stats.total_wounds_dealt, level + 1);
        write_number(out, "total_models_killed", stats.total_models_killed, level + 1);
        write_number(out, "rounds_holding_objective", stats.rounds_holding_objective, level + 1);
        write_bool(out, "first_blood", stats.first_blood, level + 1);
        write_number(out, "models_remaining", stats.models_remaining, level + 1);
        write_string(out, "final_status", stats.final_status, level + 1, true);
        out << indent(level) << "}";
        if (!last) out << ",";
        out << "\n";
    }

    static void write_result(std::ostream& out, const GameResultExport& result, int level) {
        out << indent(level) << "\"result\": {\n";
        write_string(out, "winner", result.winner, level + 1);
        write_string(out, "victory_type", result.victory_type, level + 1);
        write_number(out, "rounds_played", result.rounds_played, level + 1);
        write_string(out, "ending_condition", result.ending_condition, level + 1);
        write_unit_final_stats(out, "unit_a_stats", result.unit_a_stats, level + 1);
        write_unit_final_stats(out, "unit_b_stats", result.unit_b_stats, level + 1, true);
        out << indent(level) << "}";
    }
};

} // namespace export_data
} // namespace battle
