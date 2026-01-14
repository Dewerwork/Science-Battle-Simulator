#pragma once

#include "core/types.hpp"
#include "core/unit.hpp"
#include "engine/game_state.hpp"
#include "rules/movement_rules.hpp"
#include "rules/endround_rules.hpp"

namespace battle {

// ==============================================================================
// Rule-Aware AI Controller - Uses rule hints for better decisions
// ==============================================================================

class RuleAwareAI {
public:
    // Main decision function
    static ActionType decide_action(const GameState& state, bool is_unit_a) {
        const Unit* unit = is_unit_a ? state.unit_a_ptr : state.unit_b_ptr;
        const UnitSimState& unit_state = is_unit_a ? state.state_a : state.state_b;
        const UnitSimState& enemy_state = is_unit_a ? state.state_b : state.state_a;

        // Destroyed or routed units are idle
        if (unit_state.is_out_of_action()) {
            return ActionType::Idle;
        }

        // Shaken units must rally
        if (unit_state.is_shaken()) {
            return ActionType::Rally;
        }

        // Immobile units can only Hold
        if (unit->has_rule(RuleId::Immobile)) {
            return ActionType::Hold;
        }

        // Already in melee - continue fighting
        if (state.in_melee) {
            return ActionType::Hold;
        }

        // Enemy destroyed - move to objective
        if (enemy_state.is_out_of_action()) {
            return decide_move_to_objective(state, is_unit_a);
        }

        // Evaluate options based on rules
        float charge_score = evaluate_charge(state, is_unit_a);
        float shoot_score = evaluate_shooting(state, is_unit_a);
        float hold_score = evaluate_hold(state, is_unit_a);
        float rush_score = evaluate_rush(state, is_unit_a);

        // Find best action
        float best_score = charge_score;
        ActionType best_action = ActionType::Charge;

        if (shoot_score > best_score) {
            best_score = shoot_score;
            best_action = ActionType::Advance;
        }
        if (hold_score > best_score) {
            best_score = hold_score;
            best_action = ActionType::Hold;
        }
        if (rush_score > best_score) {
            best_score = rush_score;
            best_action = ActionType::Rush;
        }

        // Validate the chosen action is possible
        if (best_action == ActionType::Charge && !state.can_charge(is_unit_a)) {
            // Can't charge - fall back to rush or advance
            if (rush_score > shoot_score) {
                return ActionType::Rush;
            }
            return state.can_shoot(is_unit_a) ? ActionType::Advance : ActionType::Rush;
        }

        if ((best_action == ActionType::Advance || best_action == ActionType::Hold) &&
            !state.can_shoot(is_unit_a)) {
            // Can't shoot - rush toward enemy
            return ActionType::Rush;
        }

        return best_action;
    }

private:
    // ==============================================================================
    // Evaluation Functions - Score each action based on rules
    // ==============================================================================

    static float evaluate_charge(const GameState& state, bool is_unit_a) {
        const Unit* unit = is_unit_a ? state.unit_a_ptr : state.unit_b_ptr;

        // Base score depends on AI type
        float score = 0.0f;
        switch (unit->ai_type) {
            case AIType::Melee: score = 0.9f; break;
            case AIType::Hybrid: score = 0.6f; break;
            case AIType::Shooting: score = 0.3f; break;
        }

        // Can't charge if out of range
        if (!state.can_charge(is_unit_a)) {
            return -1.0f;  // Invalid action
        }

        // Check movement rule AI hints for charge preference
        score += get_rule_charge_bonus(*unit);

        // Melee-focused rules boost charge score
        if (unit->has_rule(RuleId::Furious)) score += 0.2f;
        if (unit->has_rule(RuleId::Impact)) score += 0.15f;
        if (unit->has_rule(RuleId::Tough)) score += 0.1f;  // More survivable in melee
        if (unit->has_rule(RuleId::Shielded)) score += 0.1f;  // Better defense in melee

        // Anti-melee rules reduce charge score
        if (unit->has_rule(RuleId::Indirect)) score -= 0.3f;  // Prefers indirect fire

        return score;
    }

    static float evaluate_shooting(const GameState& state, bool is_unit_a) {
        const Unit* unit = is_unit_a ? state.unit_a_ptr : state.unit_b_ptr;

        // Base score depends on AI type
        float score = 0.0f;
        switch (unit->ai_type) {
            case AIType::Shooting: score = 0.8f; break;
            case AIType::Hybrid: score = 0.6f; break;
            case AIType::Melee: score = 0.2f; break;
        }

        // Can't shoot if out of range
        if (!state.can_shoot(is_unit_a)) {
            return -1.0f;  // Invalid action
        }

        // Shooting-focused rules boost score
        if (unit->has_rule(RuleId::Relentless)) score += 0.2f;
        if (unit->has_rule(RuleId::GoodShot)) score += 0.15f;
        if (unit->has_rule(RuleId::Precise)) score += 0.1f;
        if (unit->has_rule(RuleId::Deadly)) score += 0.15f;
        if (unit->has_rule(RuleId::Indirect)) score += 0.2f;

        // Long range weapons prefer shooting
        i8 dist = state.distance_between();
        if (unit->max_range >= 24 && dist >= 18) {
            score += 0.2f;  // Good range advantage
        }

        return score;
    }

    static float evaluate_hold(const GameState& state, bool is_unit_a) {
        const Unit* unit = is_unit_a ? state.unit_a_ptr : state.unit_b_ptr;
        bool controls = is_unit_a ? state.unit_a_controls_objective() : state.unit_b_controls_objective();

        float score = 0.0f;

        // Huge bonus for holding objective
        if (controls) {
            score += 0.8f;
        }

        // Relentless units get bonus for holding (shoot without penalty)
        if (unit->has_rule(RuleId::Relentless) && state.can_shoot(is_unit_a)) {
            score += 0.3f;
        }

        // Can't hold and shoot if out of range
        if (!state.can_shoot(is_unit_a) && !controls) {
            return -1.0f;  // No point holding
        }

        // Defensive rules make holding better
        if (unit->has_rule(RuleId::Regeneration)) score += 0.1f;
        if (unit->has_rule(RuleId::Shielded)) score += 0.1f;

        return score;
    }

    static float evaluate_rush(const GameState& state, bool is_unit_a) {
        const Unit* unit = is_unit_a ? state.unit_a_ptr : state.unit_b_ptr;
        i8 my_pos = is_unit_a ? state.pos_a : state.pos_b;
        bool controls = is_unit_a ? state.unit_a_controls_objective() : state.unit_b_controls_objective();

        float score = 0.3f;  // Base rush score

        // Need to get to objective?
        if (!controls && std::abs(my_pos) > OBJECTIVE_CONTROL_RANGE) {
            score += 0.4f;  // Priority to reach objective
        }

        // Check movement rule AI hints
        const auto* fast_rule = get_movement_rule(RuleId::Fast);
        if (fast_rule && unit->has_rule(RuleId::Fast)) {
            score += fast_rule->ai_hints.charge_preference * 0.2f;
        }

        // Melee units want to close distance
        if (unit->ai_type == AIType::Melee) {
            score += 0.2f;
        }

        // Can't charge this turn - rush to get in range
        if (!state.can_charge(is_unit_a)) {
            u8 charge_range = state.get_charge_distance(*unit);
            i8 dist = state.distance_between();
            if (dist <= charge_range + 12) {  // Rush will get us in charge range
                score += 0.3f;
            }
        }

        return score;
    }

    // ==============================================================================
    // Helper Functions
    // ==============================================================================

    static float get_rule_charge_bonus(const Unit& unit) {
        float bonus = 0.0f;

        // Fast units are better at charging
        if (unit.has_rule(RuleId::Fast)) {
            const auto* rule = get_movement_rule(RuleId::Fast);
            if (rule) bonus += rule->ai_hints.charge_preference * 0.3f;
        }

        // RapidCharge explicitly boosts charge
        if (unit.has_rule(RuleId::RapidCharge)) {
            const auto* rule = get_movement_rule(RuleId::RapidCharge);
            if (rule) bonus += rule->ai_hints.charge_preference * 0.3f;
        }

        // Agile units are good at charging
        if (unit.has_rule(RuleId::Agile)) {
            const auto* rule = get_movement_rule(RuleId::Agile);
            if (rule) bonus += rule->ai_hints.charge_preference * 0.2f;
        }

        // Slow units are worse at charging
        if (unit.has_rule(RuleId::Slow)) {
            const auto* rule = get_movement_rule(RuleId::Slow);
            if (rule) bonus -= (1.0f - rule->ai_hints.charge_preference) * 0.3f;
        }

        return bonus;
    }

    static ActionType decide_move_to_objective(const GameState& state, bool is_unit_a) {
        i8 my_pos = is_unit_a ? state.pos_a : state.pos_b;

        // Already at objective?
        if (std::abs(my_pos) <= OBJECTIVE_CONTROL_RANGE) {
            return ActionType::Hold;
        }

        // Rush to objective
        return ActionType::Rush;
    }
};

} // namespace battle
