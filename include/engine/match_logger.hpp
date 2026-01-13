#pragma once

#include "core/types.hpp"
#include "core/unit.hpp"
#include <vector>

namespace battle {

// Forward declarations
struct GameState;
struct GameResult;
struct MatchResult;

/**
 * Abstract interface for match logging.
 *
 * GameRunner and CombatEngine call these methods at key points during simulation.
 * When logger is nullptr (batch mode), no logging occurs.
 * When attached (debug mode), captures everything for tabletop reproduction.
 */
class MatchLogger {
public:
    virtual ~MatchLogger() = default;

    // =========================================================================
    // MATCH LIFECYCLE (Best-of-3)
    // =========================================================================

    virtual void on_match_start(const Unit& unit_a, const Unit& unit_b, u64 seed) = 0;
    virtual void on_match_game_start(u8 game_number, bool positions_swapped) = 0;
    virtual void on_match_end(const MatchResult& result) = 0;

    // =========================================================================
    // GAME LIFECYCLE
    // =========================================================================

    virtual void on_game_start(const Unit& unit_a, const Unit& unit_b,
                                i8 pos_a, i8 pos_b) = 0;
    virtual void on_game_end(const GameResult& result, const GameState& state) = 0;

    // =========================================================================
    // ROUND LIFECYCLE
    // =========================================================================

    virtual void on_round_start(u8 round_number, const GameState& state) = 0;
    virtual void on_initiative(u8 round_number, u8 roll, bool unit_a_first,
                                const char* reason) = 0;
    virtual void on_round_end(u8 round_number, const GameState& state) = 0;

    // =========================================================================
    // OBJECTIVE CONTROL
    // =========================================================================

    virtual void on_objective_control_check(
        i8 pos_a, i8 pos_b,
        bool a_in_range, bool b_in_range,
        bool a_shaken, bool b_shaken,
        bool a_out_of_action, bool b_out_of_action,
        bool a_controls, bool b_controls,
        const char* reason) = 0;

    // =========================================================================
    // ACTIVATION LIFECYCLE
    // =========================================================================

    virtual void on_activation_start(bool is_unit_a, const GameState& state) = 0;
    virtual void on_activation_end(bool is_unit_a, const GameState& state) = 0;

    // =========================================================================
    // AI DECISION
    // =========================================================================

    virtual void on_ai_decision(
        bool is_unit_a,
        AIType ai_type,
        ActionType action,
        i8 position,
        i8 distance_to_enemy,
        i8 distance_to_objective,
        bool controls_objective,
        bool in_melee,
        bool is_shaken,
        bool is_fatigued,
        bool enemy_destroyed,
        u8 max_weapon_range,
        u8 move_speed,
        const char* reasoning) = 0;

    // =========================================================================
    // MOVEMENT
    // =========================================================================

    virtual void on_movement(
        bool is_unit_a,
        const char* move_type,
        i8 from_pos,
        i8 to_pos,
        i8 distance_moved,
        const char* reason) = 0;

    // =========================================================================
    // MELEE STATE
    // =========================================================================

    virtual void on_melee_state_changed(bool now_in_melee, const char* reason) = 0;

    // =========================================================================
    // SHOOTING COMBAT
    // =========================================================================

    virtual void on_shooting_start(
        bool attacker_is_a,
        const char* attacker_name,
        const char* defender_name,
        i8 distance,
        u8 models_shooting) = 0;

    virtual void on_shooting_end(
        bool attacker_is_a,
        u16 total_wounds,
        u8 total_models_killed,
        bool defender_destroyed) = 0;

    // =========================================================================
    // MELEE COMBAT
    // =========================================================================

    virtual void on_melee_start(
        bool attacker_is_a,
        const char* attacker_name,
        const char* defender_name,
        bool is_charging,
        bool attacker_fatigued) = 0;

    virtual void on_melee_strike_order(
        bool defender_strikes_first,
        const char* reason) = 0;

    virtual void on_melee_end(
        bool attacker_is_a,
        u16 attacker_wounds_dealt,
        u16 defender_wounds_dealt,
        bool attacker_destroyed,
        bool defender_destroyed) = 0;

    // =========================================================================
    // IMPACT ATTACKS
    // =========================================================================

    virtual void on_impact_start(
        bool attacker_is_a,
        u8 base_impact_value,
        u8 counter_reduction,
        u8 effective_impact) = 0;

    virtual void on_impact_rolls(
        const std::vector<u8>& rolls,
        u8 target,
        u32 hits) = 0;

    virtual void on_impact_defense(
        const std::vector<u8>& rolls,
        u8 defense,
        u8 effective_defense,
        u32 saves,
        u32 wounds) = 0;

    virtual void on_impact_end(u32 wounds_dealt, u8 models_killed) = 0;

    // =========================================================================
    // WEAPON ATTACKS
    // =========================================================================

    virtual void on_weapon_attack_start(
        const char* weapon_name,
        bool is_melee,
        u8 range,
        u8 distance,
        u8 base_attacks,
        u8 ap,
        const char* weapon_rules) = 0;

    virtual void on_attack_count(
        u8 models_attacking,
        u8 attacks_per_model,
        u32 total_attacks) = 0;

    virtual void on_weapon_attack_end(
        const char* weapon_name,
        u32 total_wounds,
        u8 models_killed) = 0;

    // =========================================================================
    // HIT ROLLS
    // =========================================================================

    virtual void on_hit_modifier(
        const char* source,
        i8 modifier,
        const char* reason) = 0;

    virtual void on_hit_rolls(
        u8 base_quality,
        i8 total_modifier,
        u8 effective_target,
        const std::vector<u8>& rolls,
        u32 hits,
        u32 sixes) = 0;

    // =========================================================================
    // HIT MODIFIERS (bonus hits, rending, etc.)
    // =========================================================================

    virtual void on_rule_triggered(
        const char* rule_name,
        const char* effect,
        u32 value) = 0;

    virtual void on_hits_after_modifiers(
        u32 normal_hits,
        u32 rending_hits,
        u32 total_hits) = 0;

    // =========================================================================
    // DEFENSE ROLLS
    // =========================================================================

    virtual void on_defense_modifier(
        const char* source,
        i8 modifier,
        const char* reason) = 0;

    virtual void on_defense_rolls(
        u8 base_defense,
        u8 total_ap,
        u8 effective_target,
        bool reroll_sixes,
        const std::vector<u8>& rolls,
        u32 saves,
        u32 wounds,
        u32 sixes_rerolled,
        const std::vector<u8>& rerolls,
        u32 reroll_saves) = 0;

    virtual void on_defense_rolls_rending(
        u8 base_defense,
        u8 rending_ap,
        u8 effective_target,
        const std::vector<u8>& rolls,
        u32 saves,
        u32 wounds) = 0;

    // =========================================================================
    // REGENERATION
    // =========================================================================

    virtual void on_regeneration(
        u32 wounds_before,
        u8 target,
        const std::vector<u8>& rolls,
        u32 wounds_saved,
        u32 wounds_after,
        bool was_bypassed,
        const char* bypass_reason) = 0;

    // =========================================================================
    // WOUND ALLOCATION
    // =========================================================================

    virtual void on_wound_allocation_start(
        u32 wounds_to_allocate,
        const char* allocation_order) = 0;

    virtual void on_wound_allocated(
        u8 model_index,
        const char* model_name,
        u8 model_tough,
        u8 wounds_before,
        u8 wounds_applied,
        u8 wounds_after,
        bool model_killed) = 0;

    virtual void on_deadly_wound(
        u8 model_index,
        const char* model_name,
        u8 deadly_value,
        u8 wounds_applied,
        u8 wounds_wasted,
        bool model_killed) = 0;

    virtual void on_wound_allocation_end(
        u32 total_wounds_dealt,
        u8 total_models_killed,
        u32 overkill_wounds) = 0;

    // =========================================================================
    // MORALE
    // =========================================================================

    virtual void on_morale_check_start(
        bool is_unit_a,
        const char* unit_name,
        const char* trigger_reason,
        u8 models_remaining,
        u8 models_total,
        u16 wounds_taken,
        u16 wounds_dealt) = 0;

    virtual void on_morale_roll(
        u8 roll,
        u8 quality_target,
        bool passed) = 0;

    virtual void on_fearless_roll(
        u8 roll,
        u8 target,
        bool passed) = 0;

    virtual void on_morale_check_end(
        bool final_passed,
        UnitStatus old_status,
        UnitStatus new_status,
        const char* result_description) = 0;

    // =========================================================================
    // STATUS CHANGES
    // =========================================================================

    virtual void on_status_changed(
        bool is_unit_a,
        UnitStatus old_status,
        UnitStatus new_status,
        const char* reason) = 0;

    // =========================================================================
    // FATIGUE
    // =========================================================================

    virtual void on_fatigue_changed(
        bool is_unit_a,
        bool now_fatigued,
        const char* reason) = 0;

    // =========================================================================
    // DEPLOYMENT RULES
    // =========================================================================

    virtual void on_deployment_rule(
        bool is_unit_a,
        const char* rule_name,
        i8 old_position,
        i8 new_position,
        const char* reason) = 0;

    // =========================================================================
    // MOVEMENT RULES
    // =========================================================================

    virtual void on_movement_rule_applied(
        bool is_unit_a,
        const char* rule_name,
        i8 modifier,
        const char* effect) = 0;

    // =========================================================================
    // END-ROUND RULES
    // =========================================================================

    virtual void on_end_round_rule(
        bool is_unit_a,
        const char* rule_name,
        const char* effect,
        u32 value) = 0;

    // =========================================================================
    // SPELL CASTING
    // =========================================================================

    virtual void on_spell_tokens_granted(
        bool is_unit_a,
        u8 tokens_gained,
        u8 tokens_total,
        u8 caster_value) = 0;

    virtual void on_spell_cast_attempt(
        bool is_unit_a,
        const char* spell_name,
        u8 spell_cost,
        u8 tokens_remaining,
        i8 range,
        const char* target_type) = 0;

    virtual void on_spell_interference(
        bool interferer_is_a,
        u8 tokens_spent,
        i8 modifier_applied) = 0;

    virtual void on_spell_roll(
        u8 roll,
        u8 target_number,
        i8 modifier,
        bool success) = 0;

    virtual void on_spell_effect(
        const char* spell_name,
        const char* effect_type,
        u32 hits_dealt,
        u32 wounds_dealt,
        u8 models_killed,
        const char* buff_applied) = 0;

    virtual void on_spell_phase_end(
        bool is_unit_a,
        u8 spells_cast,
        u8 spells_succeeded,
        u8 tokens_remaining) = 0;
};

} // namespace battle
