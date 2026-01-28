#pragma once

#include "engine/match_logger.hpp"
#include "engine/game_state.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace battle {

/**
 * Text-based implementation of MatchLogger for debugging.
 * Outputs human-readable game logs to an ostream (default: std::cout).
 */
class TextMatchLogger : public MatchLogger {
public:
    explicit TextMatchLogger(std::ostream& out = std::cout) : out_(out), indent_(0) {}

    // =========================================================================
    // MATCH LIFECYCLE
    // =========================================================================

    void on_match_start(const Unit& unit_a, const Unit& unit_b, u64 seed) override {
        out_ << "================================================================================\n";
        out_ << "MATCH START\n";
        out_ << "================================================================================\n";
        out_ << "Seed: " << seed << "\n";
        out_ << "Unit A: " << unit_a.name.c_str() << " (" << unit_a.faction.c_str() << ") - " << (int)unit_a.model_count << " models\n";
        out_ << "Unit B: " << unit_b.name.c_str() << " (" << unit_b.faction.c_str() << ") - " << (int)unit_b.model_count << " models\n";
        out_ << "\n";
    }

    void on_match_game_start(u8 game_number, bool positions_swapped) override {
        out_ << "--------------------------------------------------------------------------------\n";
        out_ << "GAME " << (int)game_number << (positions_swapped ? " (positions swapped)" : "") << "\n";
        out_ << "--------------------------------------------------------------------------------\n";
    }

    void on_match_end(const MatchResult& result) override {
        out_ << "================================================================================\n";
        out_ << "MATCH END\n";
        out_ << "================================================================================\n";
        out_ << "Games: Unit A won " << (int)result.games_won_a << ", Unit B won " << (int)result.games_won_b << "\n";
        out_ << "Overall Winner: ";
        switch (result.overall_winner) {
            case GameWinner::UnitA: out_ << "Unit A"; break;
            case GameWinner::UnitB: out_ << "Unit B"; break;
            case GameWinner::Draw: out_ << "Draw"; break;
        }
        out_ << "\n\n";
    }

    // =========================================================================
    // GAME LIFECYCLE
    // =========================================================================

    void on_game_start(const Unit& /*unit_a*/, const Unit& /*unit_b*/, i8 pos_a, i8 pos_b) override {
        out_ << ind() << "Game Start: Unit A at " << (int)pos_a << "\", Unit B at " << (int)pos_b << "\"\n";
        out_ << ind() << "Distance between units: " << (int)(pos_b - pos_a) << "\"\n\n";
    }

    void on_game_end(const GameResult& result, const GameState& /*state*/) override {
        out_ << "\n" << ind() << "Game End:\n";
        out_ << ind() << "  Rounds played: " << (int)result.rounds_played << "\n";
        out_ << ind() << "  Winner: ";
        switch (result.winner) {
            case GameWinner::UnitA: out_ << "Unit A"; break;
            case GameWinner::UnitB: out_ << "Unit B"; break;
            case GameWinner::Draw: out_ << "Draw"; break;
        }
        out_ << "\n";
        out_ << ind() << "  Unit A wounds dealt: " << result.stats.wounds_dealt_a
             << ", models killed: " << (int)result.stats.models_killed_a << "\n";
        out_ << ind() << "  Unit B wounds dealt: " << result.stats.wounds_dealt_b
             << ", models killed: " << (int)result.stats.models_killed_b << "\n";
        out_ << "\n";
    }

    // =========================================================================
    // ROUND LIFECYCLE
    // =========================================================================

    void on_round_start(u8 round_number, const GameState& /*state*/) override {
        out_ << "\n" << ind() << "=== ROUND " << (int)round_number << " ===\n";
        indent_++;
    }

    void on_initiative(u8 /*round_number*/, u8 roll, bool unit_a_first, const char* reason) override {
        if (roll > 0) {
            out_ << ind() << "Initiative roll: " << (int)roll << " - ";
        } else {
            out_ << ind() << "Initiative: ";
        }
        out_ << (unit_a_first ? "Unit A" : "Unit B") << " goes first (" << reason << ")\n";
    }

    void on_round_end(u8 round_number, const GameState& /*state*/) override {
        indent_--;
        out_ << ind() << "=== END ROUND " << (int)round_number << " ===\n";
    }

    // =========================================================================
    // OBJECTIVE CONTROL
    // =========================================================================

    void on_objective_control_check(i8 /*pos_a*/, i8 /*pos_b*/, bool /*a_in_range*/, bool /*b_in_range*/,
                                     bool /*a_shaken*/, bool /*b_shaken*/, bool /*a_ooa*/, bool /*b_ooa*/,
                                     bool a_controls, bool b_controls, const char* reason) override {
        out_ << ind() << "Objective Check: ";
        if (a_controls) out_ << "Unit A controls";
        else if (b_controls) out_ << "Unit B controls";
        else out_ << "No control (" << reason << ")";
        out_ << "\n";
    }

    // =========================================================================
    // ACTIVATION LIFECYCLE
    // =========================================================================

    void on_activation_start(bool is_unit_a, const GameState& /*state*/) override {
        out_ << "\n" << ind() << "--- " << (is_unit_a ? "UNIT A" : "UNIT B") << " ACTIVATION ---\n";
        indent_++;
    }

    void on_activation_end(bool /*is_unit_a*/, const GameState& /*state*/) override {
        indent_--;
    }

    // =========================================================================
    // AI DECISION
    // =========================================================================

    void on_ai_decision(bool /*is_unit_a*/, AIType /*ai_type*/, ActionType action, i8 position,
                        i8 distance_to_enemy, i8 /*distance_to_objective*/, bool /*controls_objective*/,
                        bool /*in_melee*/, bool /*is_shaken*/, bool /*is_fatigued*/, bool /*enemy_destroyed*/,
                        u8 /*max_weapon_range*/, u8 /*move_speed*/, const char* reasoning) override {
        out_ << ind() << "AI Decision: ";
        switch (action) {
            case ActionType::Hold: out_ << "HOLD"; break;
            case ActionType::Advance: out_ << "ADVANCE"; break;
            case ActionType::Rush: out_ << "RUSH"; break;
            case ActionType::Charge: out_ << "CHARGE"; break;
            case ActionType::Rally: out_ << "RALLY"; break;
            case ActionType::Idle: out_ << "IDLE"; break;
        }
        out_ << " (" << reasoning << ")\n";
        out_ << ind() << "  Position: " << (int)position << "\", Distance to enemy: " << (int)distance_to_enemy << "\"\n";
    }

    // =========================================================================
    // MOVEMENT
    // =========================================================================

    void on_movement(bool /*is_unit_a*/, const char* move_type, i8 from_pos, i8 to_pos,
                     i8 distance_moved, const char* /*reason*/) override {
        out_ << ind() << "Movement (" << move_type << "): " << (int)from_pos << "\" -> "
             << (int)to_pos << "\" (" << (int)distance_moved << "\" moved)\n";
    }

    // =========================================================================
    // MELEE STATE
    // =========================================================================

    void on_melee_state_changed(bool now_in_melee, const char* reason) override {
        out_ << ind() << "Melee State: " << (now_in_melee ? "ENGAGED" : "NOT ENGAGED")
             << " (" << reason << ")\n";
    }

    // =========================================================================
    // SHOOTING COMBAT
    // =========================================================================

    void on_shooting_start(bool /*attacker_is_a*/, const char* attacker_name, const char* defender_name,
                           i8 distance, u8 models_shooting) override {
        out_ << ind() << "SHOOTING: " << attacker_name << " -> " << defender_name
             << " at " << (int)distance << "\" with " << (int)models_shooting << " models\n";
        indent_++;
    }

    void on_shooting_end(bool /*attacker_is_a*/, u16 total_wounds, u8 total_models_killed,
                         bool defender_destroyed) override {
        indent_--;
        out_ << ind() << "Shooting Result: " << total_wounds << " wounds, "
             << (int)total_models_killed << " models killed"
             << (defender_destroyed ? " - DESTROYED" : "") << "\n";
    }

    // =========================================================================
    // MELEE COMBAT
    // =========================================================================

    void on_melee_start(bool /*attacker_is_a*/, const char* attacker_name, const char* defender_name,
                        bool is_charging, bool attacker_fatigued) override {
        out_ << ind() << "MELEE: " << attacker_name << " vs " << defender_name;
        if (is_charging) out_ << " (CHARGING)";
        if (attacker_fatigued) out_ << " (FATIGUED)";
        out_ << "\n";
        indent_++;
    }

    void on_melee_strike_order(bool defender_strikes_first, const char* reason) override {
        if (defender_strikes_first) {
            out_ << ind() << "Strike Order: DEFENDER FIRST (" << reason << ")\n";
        }
    }

    void on_melee_end(bool /*attacker_is_a*/, u16 attacker_wounds_dealt, u16 defender_wounds_dealt,
                      bool /*attacker_destroyed*/, bool /*defender_destroyed*/) override {
        indent_--;
        out_ << ind() << "Melee Result: Attacker dealt " << attacker_wounds_dealt
             << " wounds, Defender dealt " << defender_wounds_dealt << " wounds\n";
    }

    // =========================================================================
    // IMPACT ATTACKS
    // =========================================================================

    void on_impact_start(bool /*attacker_is_a*/, u8 base_impact_value, u8 counter_reduction,
                         u8 effective_impact) override {
        out_ << ind() << "Impact: " << (int)base_impact_value;
        if (counter_reduction > 0) {
            out_ << " - " << (int)counter_reduction << " (Counter) = " << (int)effective_impact;
        }
        out_ << " attacks\n";
    }

    void on_impact_rolls(const std::vector<u8>& rolls, u8 target, u32 hits) override {
        out_ << ind() << "  Impact rolls (need " << (int)target << "+): " << format_rolls(rolls)
             << " = " << hits << " hits\n";
    }

    void on_impact_defense(const std::vector<u8>& rolls, u8 /*defense*/, u8 effective_defense,
                           u32 saves, u32 wounds) override {
        out_ << ind() << "  Defense rolls (need " << (int)effective_defense << "+): "
             << format_rolls(rolls) << " = " << saves << " saves, " << wounds << " wounds\n";
    }

    void on_impact_end(u32 wounds_dealt, u8 models_killed) override {
        out_ << ind() << "  Impact Result: " << wounds_dealt << " wounds, "
             << (int)models_killed << " models killed\n";
    }

    // =========================================================================
    // WEAPON ATTACKS
    // =========================================================================

    void on_weapon_attack_start(const char* weapon_name, bool is_melee, u8 /*range*/, u8 distance,
                                u8 base_attacks, u8 ap, const char* weapon_rules) override {
        out_ << ind() << "Weapon: " << weapon_name << " (A" << (int)base_attacks
             << " AP" << (int)ap;
        if (weapon_rules && weapon_rules[0]) out_ << " " << weapon_rules;
        out_ << ") at " << (int)distance << "\" " << (is_melee ? "[melee]" : "[ranged]") << "\n";
    }

    void on_attack_count(u8 models_attacking, u8 attacks_per_model, u32 total_attacks) override {
        out_ << ind() << "  " << (int)models_attacking << " models x " << (int)attacks_per_model
             << " attacks = " << total_attacks << " total\n";
    }

    void on_weapon_attack_end(const char* /*weapon_name*/, u32 total_wounds, u8 models_killed,
                              u16 /*attacker_wounds_remaining*/, u16 /*defender_wounds_remaining*/) override {
        out_ << ind() << "  Result: " << total_wounds << " wounds, "
             << (int)models_killed << " models killed\n";
    }

    // =========================================================================
    // HIT ROLLS
    // =========================================================================

    void on_hit_modifier(const char* source, i8 modifier, const char* reason) override {
        if (modifier != 0) {
            out_ << ind() << "  Hit modifier: " << (modifier > 0 ? "+" : "") << (int)modifier
                 << " (" << source << ": " << reason << ")\n";
        } else {
            out_ << ind() << "  " << source << ": " << reason << "\n";
        }
    }

    void on_hit_rolls(u8 /*base_quality*/, i8 /*total_modifier*/, u8 effective_target,
                      const std::vector<u8>& rolls, u32 hits, u32 sixes, u32 fives = 0) override {
        out_ << ind() << "  Hit rolls (need " << (int)effective_target << "+): "
             << format_rolls(rolls) << " = " << hits << " hits";
        if (sixes > 0 || fives > 0) {
            out_ << " (";
            if (sixes > 0) out_ << sixes << " sixes";
            if (sixes > 0 && fives > 0) out_ << ", ";
            if (fives > 0) out_ << fives << " fives";
            out_ << ")";
        }
        out_ << "\n";
    }

    // =========================================================================
    // HIT MODIFIERS
    // =========================================================================

    void on_rule_triggered(const char* rule_name, const char* effect, u32 value) override {
        out_ << ind() << "  " << rule_name << ": " << effect;
        if (value > 0) out_ << " (+" << value << ")";
        out_ << "\n";
    }

    void on_hits_after_modifiers(u32 normal_hits, u32 rending_hits, u32 total_hits) override {
        if (rending_hits > 0) {
            out_ << ind() << "  Final hits: " << normal_hits << " normal + "
                 << rending_hits << " rending = " << total_hits << " total\n";
        }
    }

    // =========================================================================
    // DEFENSE ROLLS
    // =========================================================================

    void on_defense_modifier(const char* source, i8 modifier, const char* reason) override {
        out_ << ind() << "  Defense modifier: " << (modifier > 0 ? "+" : "") << (int)modifier
             << " (" << source << ": " << reason << ")\n";
    }

    void on_defense_rolls(u8 /*base_defense*/, u8 /*total_ap*/, u8 effective_target, bool reroll_sixes,
                          const std::vector<u8>& rolls, u32 saves, u32 wounds,
                          u32 /*sixes_rerolled*/, const std::vector<u8>& /*rerolls*/, u32 /*reroll_saves*/) override {
        out_ << ind() << "  Defense rolls (need " << (int)effective_target << "+): "
             << format_rolls(rolls) << " = " << saves << " saves, " << wounds << " wounds";
        if (reroll_sixes) out_ << " (poison/bane)";
        out_ << "\n";
    }

    void on_defense_rolls_rending(u8 /*base_defense*/, u8 /*rending_ap*/, u8 effective_target,
                                  const std::vector<u8>& rolls, u32 saves, u32 wounds) override {
        out_ << ind() << "  Rending defense (need " << (int)effective_target << "+): "
             << format_rolls(rolls) << " = " << saves << " saves, " << wounds << " wounds\n";
    }

    // =========================================================================
    // REGENERATION
    // =========================================================================

    void on_regeneration(u32 /*wounds_before*/, u8 target, const std::vector<u8>& rolls,
                         u32 wounds_saved, u32 wounds_after, bool was_bypassed,
                         const char* bypass_reason) override {
        if (was_bypassed) {
            out_ << ind() << "  Regeneration bypassed: " << bypass_reason << "\n";
        } else {
            out_ << ind() << "  Regeneration (need " << (int)target << "+): "
                 << format_rolls(rolls) << " = " << wounds_saved << " saved, "
                 << wounds_after << " wounds remain\n";
        }
    }

    // =========================================================================
    // WOUND ALLOCATION
    // =========================================================================

    void on_wound_allocation_start(u32 wounds_to_allocate, const char* /*allocation_order*/) override {
        out_ << ind() << "  Allocating " << wounds_to_allocate << " wounds\n";
    }

    void on_wound_allocated(u8 /*model_index*/, const char* model_name, u8 /*model_tough*/,
                            u8 /*wounds_before*/, u8 wounds_applied, u8 /*wounds_after*/,
                            bool model_killed) override {
        out_ << ind() << "    " << model_name << ": " << (int)wounds_applied << " wound(s)";
        if (model_killed) out_ << " - KILLED";
        out_ << "\n";
    }

    void on_deadly_wound(u8 /*model_index*/, const char* model_name, u8 deadly_value,
                         u8 wounds_applied, u8 wounds_wasted, bool model_killed) override {
        out_ << ind() << "    " << model_name << ": Deadly(" << (int)deadly_value << ") = "
             << (int)wounds_applied << " wounds";
        if (wounds_wasted > 0) out_ << " (" << (int)wounds_wasted << " wasted)";
        if (model_killed) out_ << " - KILLED";
        out_ << "\n";
    }

    void on_wound_allocation_end(u32 /*total_wounds_dealt*/, u8 /*total_models_killed*/,
                                 u32 /*overkill_wounds*/) override {
        // Summary already provided by weapon/combat end
    }

    // =========================================================================
    // MORALE
    // =========================================================================

    void on_morale_check_start(bool /*is_unit_a*/, const char* unit_name, const char* trigger_reason,
                               u8 /*models_remaining*/, u8 /*models_total*/, u16 /*wounds_taken*/,
                               u16 /*wounds_dealt*/) override {
        out_ << ind() << "MORALE CHECK: " << unit_name << " (" << trigger_reason << ")\n";
        indent_++;
    }

    void on_morale_roll(u8 roll, u8 quality_target, bool passed) override {
        out_ << ind() << "Roll: " << (int)roll << " vs " << (int)quality_target << "+ = "
             << (passed ? "PASS" : "FAIL") << "\n";
    }

    void on_fearless_roll(u8 roll, u8 target, bool passed) override {
        out_ << ind() << "Fearless reroll: " << (int)roll << " vs " << (int)target << "+ = "
             << (passed ? "PASS" : "FAIL") << "\n";
    }

    void on_morale_check_end(bool /*final_passed*/, UnitStatus /*old_status*/, UnitStatus /*new_status*/,
                             const char* result_description) override {
        indent_--;
        out_ << ind() << "Morale Result: " << result_description << "\n";
    }

    // =========================================================================
    // STATUS CHANGES
    // =========================================================================

    void on_status_changed(bool is_unit_a, UnitStatus old_status, UnitStatus new_status,
                           const char* reason) override {
        out_ << ind() << (is_unit_a ? "Unit A" : "Unit B") << " status: ";
        auto status_str = [](UnitStatus s) {
            switch (s) {
                case UnitStatus::Normal: return "Normal";
                case UnitStatus::Shaken: return "Shaken";
                case UnitStatus::Routed: return "Routed";
                default: return "Unknown";
            }
        };
        out_ << status_str(old_status) << " -> " << status_str(new_status)
             << " (" << reason << ")\n";
    }

    // =========================================================================
    // FATIGUE
    // =========================================================================

    void on_fatigue_changed(bool /*is_unit_a*/, bool /*now_fatigued*/, const char* /*reason*/) override {
        // Fatigue is common, only log when notable
    }

    // =========================================================================
    // DEPLOYMENT RULES
    // =========================================================================

    void on_deployment_rule(bool is_unit_a, const char* rule_name, i8 old_position,
                            i8 new_position, const char* reason) override {
        out_ << ind() << "  [DEPLOYMENT] " << (is_unit_a ? "Unit A" : "Unit B") << ": "
             << rule_name << " - " << (int)old_position << "\" -> " << (int)new_position
             << "\" (" << reason << ")\n";
    }

    // =========================================================================
    // MOVEMENT RULES
    // =========================================================================

    void on_movement_rule_applied(bool /*is_unit_a*/, const char* rule_name, i8 modifier,
                                  const char* effect) override {
        out_ << ind() << "  [MOVEMENT RULE] " << rule_name << ": ";
        if (modifier != 0) {
            out_ << (modifier > 0 ? "+" : "") << (int)modifier << "\" ";
        }
        out_ << "(" << effect << ")\n";
    }

    // =========================================================================
    // ROUND-START RULES
    // =========================================================================

    void on_round_start_rule(bool is_unit_a, const char* rule_name, const char* effect,
                             u32 value) override {
        out_ << ind() << "[ROUND-START] " << (is_unit_a ? "Unit A" : "Unit B") << ": "
             << rule_name << " - " << effect;
        if (value > 0) out_ << " (" << value << ")";
        out_ << "\n";
    }

    // =========================================================================
    // END-ROUND RULES
    // =========================================================================

    void on_end_round_rule(bool is_unit_a, const char* rule_name, const char* effect,
                           u32 value) override {
        out_ << ind() << "[END-ROUND] " << (is_unit_a ? "Unit A" : "Unit B") << ": "
             << rule_name << " - " << effect;
        if (value > 0) out_ << " (" << value << ")";
        out_ << "\n";
    }

    // =========================================================================
    // SPELL CASTING
    // =========================================================================

    void on_spell_tokens_granted(bool is_unit_a, u8 tokens_gained, u8 tokens_total,
                                 u8 caster_value) override {
        out_ << ind() << "[SPELLS] " << (is_unit_a ? "Unit A" : "Unit B")
             << ": Granted " << (int)tokens_gained << " spell tokens (Caster("
             << (int)caster_value << ")), total: " << (int)tokens_total << "\n";
    }

    void on_spell_cast_attempt(bool is_unit_a, const char* spell_name, u8 spell_cost,
                               u8 /*tokens_remaining*/, i8 range, const char* target_type) override {
        out_ << ind() << "[SPELL CAST] " << (is_unit_a ? "Unit A" : "Unit B")
             << ": Casting \"" << spell_name << "\" (cost " << (int)spell_cost
             << ", " << target_type << ", range " << (int)range << "\")\n";
        indent_++;
    }

    void on_spell_interference(bool interferer_is_a, u8 tokens_spent,
                               i8 modifier_applied) override {
        out_ << ind() << "Interference: " << (interferer_is_a ? "Unit A" : "Unit B")
             << " spends " << (int)tokens_spent << " token(s) for "
             << (int)modifier_applied << " to cast roll\n";
    }

    void on_spell_roll(u8 roll, u8 target_number, i8 /*modifier*/, bool success) override {
        out_ << ind() << "Cast roll: " << (int)roll << " vs " << (int)target_number << "+ = "
             << (success ? "SUCCESS" : "FAILED") << "\n";
        indent_--;
    }

    void on_spell_effect(const char* spell_name, const char* effect_type, u32 hits_dealt,
                         u32 wounds_dealt, u8 models_killed, const char* buff_applied) override {
        out_ << ind() << "[SPELL EFFECT] " << spell_name << " (" << effect_type << ")";
        if (hits_dealt > 0) out_ << ": " << hits_dealt << " hits";
        if (wounds_dealt > 0) out_ << ", " << wounds_dealt << " wounds";
        if (models_killed > 0) out_ << ", " << (int)models_killed << " killed";
        if (buff_applied && buff_applied[0]) out_ << " - " << buff_applied;
        out_ << "\n";
    }

    void on_spell_phase_end(bool is_unit_a, u8 spells_cast, u8 spells_succeeded,
                            u8 tokens_remaining) override {
        out_ << ind() << "[SPELL PHASE END] " << (is_unit_a ? "Unit A" : "Unit B")
             << ": " << (int)spells_succeeded << "/" << (int)spells_cast
             << " spells succeeded, " << (int)tokens_remaining << " tokens remaining\n";
    }

private:
    std::ostream& out_;
    int indent_;

    std::string ind() const {
        return std::string(indent_ * 2, ' ');
    }

    std::string format_rolls(const std::vector<u8>& rolls) const {
        if (rolls.empty()) return "[]";
        std::ostringstream ss;
        ss << "[";
        for (size_t i = 0; i < rolls.size(); ++i) {
            if (i > 0) ss << ",";
            ss << (int)rolls[i];
        }
        ss << "]";
        return ss.str();
    }
};

} // namespace battle
