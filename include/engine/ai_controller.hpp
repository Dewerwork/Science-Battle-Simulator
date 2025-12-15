#pragma once

#include "core/types.hpp"
#include "core/unit.hpp"
#include "engine/game_state.hpp"

namespace battle {

// ==============================================================================
// AI Controller - Implements Solo Play decision trees
// ==============================================================================

class AIController {
public:
    // Decide action for a unit based on its AI type
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

        // Already in melee - continue fighting
        if (state.in_melee) {
            return ActionType::Hold;  // Fight in place
        }

        // Enemy destroyed - move to objective
        if (enemy_state.is_out_of_action()) {
            return decide_move_to_objective(state, is_unit_a);
        }

        // Choose based on AI type
        switch (unit->ai_type) {
            case AIType::Melee:
                return decide_melee_ai(state, is_unit_a);
            case AIType::Shooting:
                return decide_shooting_ai(state, is_unit_a);
            case AIType::Hybrid:
                return decide_hybrid_ai(state, is_unit_a);
            default:
                return ActionType::Advance;
        }
    }

private:
    // MELEE AI: Aggressive, charge-focused
    static ActionType decide_melee_ai(const GameState& state, bool is_unit_a) {
        const Unit* unit = is_unit_a ? state.unit_a_ptr : state.unit_b_ptr;
        i8 dist = state.distance_between();
        u8 move_speed = state.get_move_speed(*unit);

        // Can we charge?
        if (dist <= CHARGE_DISTANCE) {
            return ActionType::Charge;
        }

        // Not controlling objective? Move toward it
        bool controls = is_unit_a ? state.unit_a_controls_objective() : state.unit_b_controls_objective();
        if (!controls) {
            // Rush toward objective/enemy
            return ActionType::Rush;
        }

        // Controlling objective - still rush toward enemy to fight
        return ActionType::Rush;
    }

    // SHOOTING AI: Maintain distance, shoot
    static ActionType decide_shooting_ai(const GameState& state, bool is_unit_a) {
        const Unit* unit = is_unit_a ? state.unit_a_ptr : state.unit_b_ptr;
        i8 my_pos = is_unit_a ? state.pos_a : state.pos_b;
        i8 dist = state.distance_between();
        u8 move_speed = state.get_move_speed(*unit);

        bool controls = is_unit_a ? state.unit_a_controls_objective() : state.unit_b_controls_objective();

        // Not controlling objective?
        if (!controls) {
            // Can we advance toward objective and still shoot?
            i8 new_pos = my_pos + (is_unit_a ? move_speed : -move_speed);
            i8 new_dist = is_unit_a ? (state.pos_b - new_pos) : (new_pos - state.pos_a);

            if (unit->max_range >= static_cast<u8>(new_dist)) {
                return ActionType::Advance;  // Advance + shoot
            } else {
                return ActionType::Rush;  // Rush to get in range
            }
        }

        // Controlling objective - try to shoot
        if (unit->max_range >= static_cast<u8>(dist)) {
            return ActionType::Hold;  // Hold and shoot (Relentless bonus)
        }

        // Can't shoot from here, advance
        return ActionType::Advance;
    }

    // HYBRID AI: Opportunistic - shoot when possible, charge when close
    static ActionType decide_hybrid_ai(const GameState& state, bool is_unit_a) {
        const Unit* unit = is_unit_a ? state.unit_a_ptr : state.unit_b_ptr;
        i8 my_pos = is_unit_a ? state.pos_a : state.pos_b;
        i8 dist = state.distance_between();
        u8 move_speed = state.get_move_speed(*unit);

        bool controls = is_unit_a ? state.unit_a_controls_objective() : state.unit_b_controls_objective();

        // Can we charge?
        if (dist <= CHARGE_DISTANCE) {
            return ActionType::Charge;
        }

        // Not controlling objective?
        if (!controls) {
            // If we can reach objective quickly, rush
            i8 dist_to_obj = std::abs(my_pos);
            if (dist_to_obj <= move_speed * 2) {
                // Can we advance and shoot?
                i8 new_pos = my_pos + (is_unit_a ? move_speed : -move_speed);
                i8 new_dist = is_unit_a ? (state.pos_b - new_pos) : (new_pos - state.pos_a);

                if (unit->max_range >= static_cast<u8>(new_dist)) {
                    return ActionType::Advance;
                }
            }
            return ActionType::Rush;
        }

        // Controlling objective
        // Can we shoot?
        if (unit->max_range >= static_cast<u8>(dist)) {
            // Advance toward enemy to get closer for potential charge next turn
            i8 new_pos = my_pos + (is_unit_a ? move_speed : -move_speed);
            i8 new_dist = is_unit_a ? (state.pos_b - new_pos) : (new_pos - state.pos_a);

            if (unit->max_range >= static_cast<u8>(new_dist)) {
                return ActionType::Advance;  // Advance + shoot
            }
            return ActionType::Hold;  // Hold + shoot
        }

        // Can't shoot - rush toward enemy
        return ActionType::Rush;
    }

    // Move toward objective when enemy is gone
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
