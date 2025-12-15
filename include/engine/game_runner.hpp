#pragma once

#include "core/types.hpp"
#include "core/unit.hpp"
#include "engine/dice.hpp"
#include "engine/game_state.hpp"
#include "engine/combat_engine.hpp"
#include "engine/ai_controller.hpp"
#include <algorithm>

namespace battle {

// ==============================================================================
// Game Runner - Executes a complete game (optimized - no unit copying)
// ==============================================================================

class GameRunner {
public:
    explicit GameRunner(DiceRoller& dice)
        : dice_(dice), combat_(dice) {}

    // Run a single game between two units
    GameResult run_game(const Unit& unit_a, const Unit& unit_b) {
        state_.init(unit_a, unit_b);

        // Run up to MAX_ROUNDS
        while (!state_.is_game_over() && state_.current_round <= MAX_ROUNDS) {
            run_round();
        }

        return GameResult::determine(state_);
    }

    // Run a best-of-3 match
    MatchResult run_match(const Unit& unit_a, const Unit& unit_b) {
        MatchResult result;
        result.unit_a_id = unit_a.unit_id;
        result.unit_b_id = unit_b.unit_id;

        for (int game = 0; game < 3; ++game) {
            // Alternate starting positions each game
            GameResult game_result;
            if (game % 2 == 0) {
                game_result = run_game(unit_a, unit_b);
            } else {
                // Swap positions for game 2
                game_result = run_game(unit_b, unit_a);
                // Flip the result perspective
                if (game_result.winner == GameWinner::UnitA) {
                    game_result.winner = GameWinner::UnitB;
                } else if (game_result.winner == GameWinner::UnitB) {
                    game_result.winner = GameWinner::UnitA;
                }
                // Swap stats
                std::swap(game_result.stats.wounds_dealt_a, game_result.stats.wounds_dealt_b);
                std::swap(game_result.stats.models_killed_a, game_result.stats.models_killed_b);
                std::swap(game_result.stats.rounds_holding_a, game_result.stats.rounds_holding_b);
            }

            result.add_game(game_result);

            // Early exit if match is decided
            if (result.games_won_a == 2 || result.games_won_b == 2) {
                break;
            }
        }

        result.determine_winner();
        return result;
    }

private:
    DiceRoller& dice_;
    CombatEngine combat_;
    GameState state_;  // Reusable game state

    void run_round() {
        // Determine activation order (random for round 1, alternating after)
        bool a_goes_first;
        if (state_.current_round == 1) {
            a_goes_first = (dice_.roll_d6() >= 4);
        } else {
            // Loser of initiative last round goes first
            // For simplicity, alternate based on round number
            a_goes_first = (state_.current_round % 2 == 1);
        }

        // First activation
        if (a_goes_first) {
            activate_unit(true);
            activate_unit(false);
        } else {
            activate_unit(false);
            activate_unit(true);
        }

        // End of round
        state_.update_objective_control();
        state_.next_round();
    }

    void activate_unit(bool is_unit_a) {
        UnitView unit = state_.view(is_unit_a);
        i8& my_pos = is_unit_a ? state_.pos_a : state_.pos_b;

        if (is_unit_a) state_.unit_a_activated = true;
        else state_.unit_b_activated = true;

        // Get AI decision
        ActionType action = AIController::decide_action(state_, is_unit_a);

        // Execute action
        switch (action) {
            case ActionType::Hold:
                execute_hold(is_unit_a);
                break;

            case ActionType::Advance:
                execute_advance(is_unit_a);
                break;

            case ActionType::Rush:
                execute_rush(is_unit_a);
                break;

            case ActionType::Charge:
                execute_charge(is_unit_a);
                break;

            case ActionType::Rally:
                unit.rally();
                break;

            case ActionType::Idle:
            default:
                // Do nothing
                break;
        }
    }

    void execute_hold(bool is_unit_a) {
        UnitView unit = state_.view(is_unit_a);
        UnitView enemy = state_.view(!is_unit_a);

        if (state_.in_melee) {
            // Fight in melee
            execute_melee_round(is_unit_a, false);
        } else {
            // Shoot if possible
            i8 dist = state_.distance_between();
            if (unit.max_range() >= static_cast<u8>(dist) && !enemy.is_out_of_action()) {
                CombatResult result = combat_.resolve_shooting(unit, enemy, dist, false);
                state_.stats.record_wounds(is_unit_a, result.wounds_dealt, result.models_killed);

                // Check morale for defender if took wounds (not just kills)
                // Morale test when unit is at half strength after taking wounds
                if (result.wounds_dealt > 0) {
                    combat_.check_morale(enemy, false);  // false = not from melee
                }
            }
        }
    }

    void execute_advance(bool is_unit_a) {
        UnitView unit = state_.view(is_unit_a);
        UnitView enemy = state_.view(!is_unit_a);
        i8& my_pos = is_unit_a ? state_.pos_a : state_.pos_b;

        if (state_.in_melee) {
            execute_melee_round(is_unit_a, false);
            return;
        }

        // Move toward center/enemy
        u8 move_speed = state_.get_move_speed(*unit.unit);
        if (is_unit_a) {
            my_pos = std::min(static_cast<i8>(my_pos + move_speed), state_.pos_b);
        } else {
            my_pos = std::max(static_cast<i8>(my_pos - move_speed), state_.pos_a);
        }

        // Shoot if possible
        i8 dist = state_.distance_between();
        if (unit.max_range() >= static_cast<u8>(dist) && !enemy.is_out_of_action()) {
            CombatResult result = combat_.resolve_shooting(unit, enemy, dist, true);
            state_.stats.record_wounds(is_unit_a, result.wounds_dealt, result.models_killed);

            // Check morale for defender if took wounds
            if (result.wounds_dealt > 0) {
                combat_.check_morale(enemy, false);  // false = not from melee
            }
        }
    }

    void execute_rush(bool is_unit_a) {
        UnitView unit = state_.view(is_unit_a);
        i8& my_pos = is_unit_a ? state_.pos_a : state_.pos_b;
        i8& enemy_pos = is_unit_a ? state_.pos_b : state_.pos_a;

        if (state_.in_melee) {
            execute_melee_round(is_unit_a, false);
            return;
        }

        // Rush (double move, no shooting)
        u8 move_speed = state_.get_move_speed(*unit.unit) * RUSH_MULTIPLIER;
        if (is_unit_a) {
            my_pos = std::min(static_cast<i8>(my_pos + move_speed), enemy_pos);
        } else {
            my_pos = std::max(static_cast<i8>(my_pos - move_speed), state_.pos_a);
        }
    }

    void execute_charge(bool is_unit_a) {
        UnitView unit = state_.view(is_unit_a);
        UnitView enemy = state_.view(!is_unit_a);
        i8& my_pos = is_unit_a ? state_.pos_a : state_.pos_b;
        i8& enemy_pos = is_unit_a ? state_.pos_b : state_.pos_a;

        // Move into contact
        if (is_unit_a) {
            my_pos = enemy_pos;
        } else {
            my_pos = state_.pos_a;
        }

        state_.in_melee = true;

        // Resolve charge (attacker strikes first)
        execute_melee_round(is_unit_a, true);
    }

    void execute_melee_round(bool is_unit_a, bool is_charging) {
        UnitView attacker = state_.view(is_unit_a);
        UnitView defender = state_.view(!is_unit_a);

        if (attacker.is_out_of_action() || defender.is_out_of_action()) {
            state_.in_melee = false;
            return;
        }

        // Count models with Counter rule in defender (for Impact reduction)
        u8 counter_models = 0;
        if (defender.has_rule(RuleId::Counter)) {
            // If the unit has Counter, all alive models count
            counter_models = defender.alive_count();
        }

        // Counter: defender strikes first when charged (and not shaken)
        bool defender_strikes_first = is_charging && defender.has_rule(RuleId::Counter) && !defender.is_shaken();

        u16 attacker_wounds = 0;
        u16 defender_wounds = 0;

        if (defender_strikes_first) {
            // Defender with Counter strikes first
            CombatResult def_result = combat_.resolve_melee(defender, attacker, false, 0);
            state_.stats.record_wounds(!is_unit_a, def_result.wounds_dealt, def_result.models_killed);
            attacker_wounds = def_result.wounds_dealt;

            // Mark defender as fatigued after striking
            defender.set_fatigued(true);

            if (!attacker.is_out_of_action()) {
                // Attacker strikes back (pass counter_models for Impact reduction)
                CombatResult atk_result = combat_.resolve_melee(attacker, defender, is_charging, counter_models);
                state_.stats.record_wounds(is_unit_a, atk_result.wounds_dealt, atk_result.models_killed);
                defender_wounds = atk_result.wounds_dealt;

                // Mark attacker as fatigued after striking
                attacker.set_fatigued(true);
            }
        } else {
            // Normal order: attacker first (pass counter_models for Impact reduction)
            CombatResult atk_result = combat_.resolve_melee(attacker, defender, is_charging, counter_models);
            state_.stats.record_wounds(is_unit_a, atk_result.wounds_dealt, atk_result.models_killed);
            defender_wounds = atk_result.wounds_dealt;

            // Mark attacker as fatigued after striking
            attacker.set_fatigued(true);

            // Defender may strike back if not destroyed
            // Shaken units CAN strike back, but count as fatigued (only hit on 6s)
            if (!defender.is_out_of_action()) {
                // Shaken units strike back counting as fatigued
                if (defender.is_shaken()) {
                    defender.set_fatigued(true);
                }
                CombatResult def_result = combat_.resolve_melee(defender, attacker, false, 0);
                state_.stats.record_wounds(!is_unit_a, def_result.wounds_dealt, def_result.models_killed);
                attacker_wounds = def_result.wounds_dealt;

                // Mark defender as fatigued after striking
                defender.set_fatigued(true);
            }
        }

        // Apply Fear(X) to wound totals for morale comparison
        u16 attacker_effective_wounds = attacker_wounds + attacker.get_rule_value(RuleId::Fear);
        u16 defender_effective_wounds = defender_wounds + defender.get_rule_value(RuleId::Fear);

        // Morale checks for melee loser (compare effective wounds with Fear)
        if (defender_effective_wounds > attacker_effective_wounds && !attacker.is_out_of_action()) {
            combat_.check_morale(attacker, true, attacker_wounds, defender_wounds);
        } else if (attacker_effective_wounds > defender_effective_wounds && !defender.is_out_of_action()) {
            combat_.check_morale(defender, true, defender_wounds, attacker_wounds);
        }

        // Check if melee is over
        if (attacker.is_out_of_action() || defender.is_out_of_action()) {
            state_.in_melee = false;
        }
    }
};

} // namespace battle
