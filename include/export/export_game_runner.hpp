#pragma once

#include "export/export_types.hpp"
#include "export/game_event_log.hpp"
#include "core/types.hpp"
#include "core/unit.hpp"
#include "engine/dice.hpp"
#include "engine/game_state.hpp"
#include "engine/ai_controller.hpp"
#include <algorithm>
#include <vector>

namespace battle {
namespace export_data {

// ==============================================================================
// Logging Dice Roller - Wraps DiceRoller and logs all rolls
// ==============================================================================

class LoggingDiceRoller {
public:
    explicit LoggingDiceRoller(u64 seed = 0) : dice_(seed), seed_(seed) {}

    u64 get_seed() const { return seed_; }

    // Roll a single D6 and log it
    u8 roll_d6() {
        u8 result = dice_.roll_d6();
        last_single_roll_ = result;
        return result;
    }

    // Roll quality test with full logging
    struct LoggedQualityResult {
        std::vector<u8> rolls;
        u32 hits;
        u32 sixes;
        u8 target;
        u8 effective_target;
        i8 modifier;
    };

    LoggedQualityResult roll_quality_test_logged(u32 attacks, u8 quality, i8 modifier = 0) {
        LoggedQualityResult result;
        result.target = quality;
        result.modifier = modifier;

        // Calculate effective target (clamped 2-6)
        i8 effective = static_cast<i8>(quality) - modifier;
        effective = std::max(i8(2), std::min(i8(6), effective));
        result.effective_target = static_cast<u8>(effective);

        result.hits = 0;
        result.sixes = 0;

        for (u32 i = 0; i < attacks; ++i) {
            u8 roll = dice_.roll_d6();
            result.rolls.push_back(roll);
            if (roll >= result.effective_target) {
                result.hits++;
            }
            if (roll == 6) {
                result.sixes++;
            }
        }

        return result;
    }

    // Roll defense test with full logging
    struct LoggedDefenseResult {
        std::vector<u8> rolls;
        u32 saves;
        u32 wounds;
        u32 sixes;
        u8 defense;
        u8 ap;
        u8 effective_target;
        bool reroll_sixes;
        std::vector<u8> rerolls;
        u32 reroll_saves;
    };

    LoggedDefenseResult roll_defense_test_logged(u32 hits, u8 defense, u8 ap, i8 modifier = 0, bool reroll_sixes = false) {
        LoggedDefenseResult result;
        result.defense = defense;
        result.ap = ap;
        result.reroll_sixes = reroll_sixes;

        // AP increases the target number needed
        i8 effective = static_cast<i8>(defense) + static_cast<i8>(ap) - modifier;
        effective = std::max(i8(2), std::min(i8(6), effective));
        result.effective_target = static_cast<u8>(effective);

        result.saves = 0;
        result.sixes = 0;
        result.reroll_saves = 0;

        for (u32 i = 0; i < hits; ++i) {
            u8 roll = dice_.roll_d6();
            result.rolls.push_back(roll);

            if (roll == 6 && reroll_sixes) {
                result.sixes++;
            } else if (roll >= result.effective_target) {
                result.saves++;
            }
        }

        // Reroll sixes if needed (Poison/Bane)
        if (reroll_sixes && result.sixes > 0) {
            for (u32 i = 0; i < result.sixes; ++i) {
                u8 roll = dice_.roll_d6();
                result.rerolls.push_back(roll);
                if (roll >= result.effective_target) {
                    result.reroll_saves++;
                }
            }
            result.saves += result.reroll_saves;
        }

        result.wounds = hits - result.saves;
        return result;
    }

    // Roll regeneration with logging
    struct LoggedRegenResult {
        std::vector<u8> rolls;
        u32 saved;
        u32 wounds_after;
        u8 target;
    };

    LoggedRegenResult roll_regeneration_logged(u32 wounds, u8 target = 5) {
        LoggedRegenResult result;
        result.target = target;
        result.saved = 0;

        for (u32 i = 0; i < wounds; ++i) {
            u8 roll = dice_.roll_d6();
            result.rolls.push_back(roll);
            if (roll >= target) {
                result.saved++;
            }
        }

        result.wounds_after = wounds - result.saved;
        return result;
    }

    // Roll impact with logging
    struct LoggedImpactResult {
        std::vector<u8> rolls;
        u32 hits;
    };

    LoggedImpactResult roll_impact_logged(u32 count) {
        LoggedImpactResult result;
        result.hits = 0;

        for (u32 i = 0; i < count; ++i) {
            u8 roll = dice_.roll_d6();
            result.rolls.push_back(roll);
            if (roll >= 2) {  // Impact hits on 2+
                result.hits++;
            }
        }

        return result;
    }

    // Roll morale test with logging
    struct LoggedMoraleResult {
        u8 roll;
        bool passed;
        bool fearless_used;
        u8 fearless_roll;
        bool fearless_passed;
    };

    LoggedMoraleResult roll_morale_logged(u8 quality, bool has_fearless) {
        LoggedMoraleResult result;
        result.roll = dice_.roll_d6();
        result.passed = result.roll >= quality;
        result.fearless_used = false;
        result.fearless_roll = 0;
        result.fearless_passed = false;

        if (!result.passed && has_fearless) {
            result.fearless_used = true;
            result.fearless_roll = dice_.roll_d6();
            result.fearless_passed = result.fearless_roll >= 4;
            if (result.fearless_passed) {
                result.passed = true;
            }
        }

        return result;
    }

    u8 get_last_single_roll() const { return last_single_roll_; }

private:
    DiceRoller dice_;
    u64 seed_;
    u8 last_single_roll_ = 0;
};

// ==============================================================================
// Export Game Runner - Runs a game with full logging
// ==============================================================================

class ExportGameRunner {
public:
    explicit ExportGameRunner(u64 seed = 0)
        : dice_(seed), log_() {
        log_.set_seed(dice_.get_seed());
    }

    // Run a single game with full logging
    GameResult run_game(const Unit& unit_a, const Unit& unit_b) {
        state_.init(unit_a, unit_b);
        log_.log_game_start(unit_a, unit_b, state_.pos_a, state_.pos_b);

        // Run up to MAX_ROUNDS
        while (!state_.is_game_over() && state_.current_round <= MAX_ROUNDS) {
            run_round();
        }

        GameResult result = GameResult::determine(state_);
        log_.log_game_end(result, state_);

        return result;
    }

    const GameExport& get_export() const { return log_.get_export(); }
    GameExport& get_export() { return log_.get_export(); }

private:
    LoggingDiceRoller dice_;
    GameEventLog log_;
    GameState state_;

    void run_round() {
        log_.log_round_start(state_.current_round, state_);

        // Determine activation order
        bool a_goes_first;
        u8 initiative_roll = 0;
        if (state_.current_round == 1) {
            initiative_roll = dice_.roll_d6();
            a_goes_first = (initiative_roll >= 4);
        } else {
            a_goes_first = (state_.current_round % 2 == 1);
        }

        log_.log_initiative(state_.current_round == 1, a_goes_first, initiative_roll);

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
        log_.log_round_end(state_);
        state_.next_round();
    }

    void activate_unit(bool is_unit_a) {
        UnitView unit = state_.view(is_unit_a);
        const Unit* unit_ptr = is_unit_a ? state_.unit_a_ptr : state_.unit_b_ptr;
        i8& my_pos = is_unit_a ? state_.pos_a : state_.pos_b;
        i8 original_pos = my_pos;

        if (is_unit_a) state_.unit_a_activated = true;
        else state_.unit_b_activated = true;

        // Get AI decision with logging
        ActionType action = AIController::decide_action(state_, is_unit_a);

        // Log AI decision
        AIDecisionEvent ai_decision = build_ai_decision(is_unit_a, action);

        log_.begin_activation(is_unit_a, *unit_ptr, action);
        log_.log_ai_decision(ai_decision);

        // Execute action
        switch (action) {
            case ActionType::Hold:
                execute_hold(is_unit_a);
                break;

            case ActionType::Advance:
                execute_advance(is_unit_a, original_pos);
                break;

            case ActionType::Rush:
                execute_rush(is_unit_a, original_pos);
                break;

            case ActionType::Charge:
                execute_charge(is_unit_a, original_pos);
                break;

            case ActionType::Rally:
                unit.rally();
                break;

            case ActionType::Idle:
            default:
                break;
        }

        log_.end_activation(state_);
    }

    AIDecisionEvent build_ai_decision(bool is_unit_a, ActionType action) {
        const Unit* unit = is_unit_a ? state_.unit_a_ptr : state_.unit_b_ptr;
        const UnitSimState& unit_state = is_unit_a ? state_.state_a : state_.state_b;
        const UnitSimState& enemy_state = is_unit_a ? state_.state_b : state_.state_a;

        AIDecisionEvent event;
        event.unit_name = std::string(unit->name.view());
        event.ai_type = ai_type_to_string(unit->ai_type);
        event.current_status = unit_status_to_string(unit_state.status);
        event.position = is_unit_a ? state_.pos_a : state_.pos_b;
        event.distance_to_enemy = state_.distance_between();
        event.distance_to_objective = std::abs(event.position);
        event.controls_objective = is_unit_a ? state_.unit_a_controls_objective() : state_.unit_b_controls_objective();
        event.enemy_destroyed = enemy_state.is_out_of_action();
        event.in_melee = state_.in_melee;
        event.decision = action_type_to_string(action);

        // Build reasoning
        std::string reasoning;
        if (unit_state.is_out_of_action()) {
            reasoning = "Unit is destroyed/routed - forced Idle";
        } else if (unit_state.is_shaken()) {
            reasoning = "Unit is Shaken - must Rally";
        } else if (state_.in_melee) {
            reasoning = "In melee combat - Hold and fight";
        } else if (enemy_state.is_out_of_action()) {
            reasoning = "Enemy destroyed - move toward objective";
        } else {
            switch (unit->ai_type) {
                case AIType::Melee:
                    if (state_.distance_between() <= state_.get_charge_distance(*unit)) {
                        reasoning = "Melee AI: In charge range - Charge!";
                    } else {
                        reasoning = "Melee AI: Out of charge range - Rush toward enemy";
                    }
                    break;
                case AIType::Shooting:
                    if (event.controls_objective && unit->max_range >= static_cast<u8>(state_.distance_between())) {
                        reasoning = "Shooting AI: Controls objective and in range - Hold and shoot";
                    } else {
                        reasoning = "Shooting AI: Move to get in range or reach objective";
                    }
                    break;
                case AIType::Hybrid:
                    if (state_.distance_between() <= state_.get_charge_distance(*unit)) {
                        reasoning = "Hybrid AI: In charge range - Charge!";
                    } else if (unit->max_range >= static_cast<u8>(state_.distance_between())) {
                        reasoning = "Hybrid AI: In shooting range - Advance and shoot";
                    } else {
                        reasoning = "Hybrid AI: Rush toward enemy";
                    }
                    break;
            }
        }
        event.reasoning = reasoning;

        // Add factors
        event.factors_considered.push_back({"distance_to_enemy", std::to_string(event.distance_to_enemy) + "\"", ""});
        event.factors_considered.push_back({"max_weapon_range", std::to_string(unit->max_range) + "\"", ""});
        event.factors_considered.push_back({"controls_objective", event.controls_objective ? "yes" : "no", ""});
        event.factors_considered.push_back({"in_melee", event.in_melee ? "yes" : "no", ""});

        return event;
    }

    void execute_hold(bool is_unit_a) {
        UnitView unit = state_.view(is_unit_a);
        UnitView enemy = state_.view(!is_unit_a);

        if (state_.in_melee) {
            execute_melee_round(is_unit_a, false);
        } else {
            i8 dist = state_.distance_between();
            if (unit.max_range() >= static_cast<u8>(dist) && !enemy.is_out_of_action()) {
                CombatEvent combat = resolve_shooting_logged(unit, enemy, dist, false, is_unit_a);
                log_.log_combat(combat);
                state_.stats.record_wounds(is_unit_a, combat.total_wounds_dealt, combat.total_models_killed);

                if (combat.total_wounds_dealt > 0) {
                    check_morale_logged(enemy, false, is_unit_a);
                }
            }
        }
    }

    void execute_advance(bool is_unit_a, i8 original_pos) {
        UnitView unit = state_.view(is_unit_a);
        UnitView enemy = state_.view(!is_unit_a);
        i8& my_pos = is_unit_a ? state_.pos_a : state_.pos_b;

        if (state_.in_melee) {
            execute_melee_round(is_unit_a, false);
            return;
        }

        u8 move_speed = state_.get_move_speed(*unit.unit);
        if (is_unit_a) {
            my_pos = std::min(static_cast<i8>(my_pos + move_speed), state_.pos_b);
        } else {
            my_pos = std::max(static_cast<i8>(my_pos - move_speed), state_.pos_a);
        }

        log_.log_movement("advance", original_pos, my_pos, "Moving toward objective/enemy");

        i8 dist = state_.distance_between();
        if (unit.max_range() >= static_cast<u8>(dist) && !enemy.is_out_of_action()) {
            CombatEvent combat = resolve_shooting_logged(unit, enemy, dist, true, is_unit_a);
            log_.log_combat(combat);
            state_.stats.record_wounds(is_unit_a, combat.total_wounds_dealt, combat.total_models_killed);

            if (combat.total_wounds_dealt > 0) {
                check_morale_logged(enemy, false, is_unit_a);
            }
        }
    }

    void execute_rush(bool is_unit_a, i8 original_pos) {
        UnitView unit = state_.view(is_unit_a);
        i8& my_pos = is_unit_a ? state_.pos_a : state_.pos_b;
        i8& enemy_pos = is_unit_a ? state_.pos_b : state_.pos_a;

        if (state_.in_melee) {
            execute_melee_round(is_unit_a, false);
            return;
        }

        u8 move_speed = state_.get_move_speed(*unit.unit) * RUSH_MULTIPLIER;
        if (is_unit_a) {
            my_pos = std::min(static_cast<i8>(my_pos + move_speed), enemy_pos);
        } else {
            my_pos = std::max(static_cast<i8>(my_pos - move_speed), state_.pos_a);
        }

        log_.log_movement("rush", original_pos, my_pos, "Double move, no shooting");
    }

    void execute_charge(bool is_unit_a, i8 original_pos) {
        UnitView unit = state_.view(is_unit_a);
        UnitView enemy = state_.view(!is_unit_a);
        i8& my_pos = is_unit_a ? state_.pos_a : state_.pos_b;
        i8& enemy_pos = is_unit_a ? state_.pos_b : state_.pos_a;

        if (is_unit_a) {
            my_pos = enemy_pos;
        } else {
            my_pos = state_.pos_a;
        }

        log_.log_movement("charge", original_pos, my_pos, "Charging into melee");

        state_.in_melee = true;
        execute_melee_round(is_unit_a, true);
    }

    void execute_melee_round(bool is_unit_a, bool is_charging) {
        UnitView attacker = state_.view(is_unit_a);
        UnitView defender = state_.view(!is_unit_a);

        if (attacker.is_out_of_action() || defender.is_out_of_action()) {
            state_.in_melee = false;
            return;
        }

        u8 counter_models = 0;
        if (defender.has_rule(RuleId::Counter)) {
            counter_models = defender.alive_count();
        }

        bool defender_strikes_first = is_charging && defender.has_rule(RuleId::Counter) && !defender.is_shaken();

        u16 attacker_wounds = 0;
        u16 defender_wounds = 0;

        if (defender_strikes_first) {
            // Defender with Counter strikes first
            CombatEvent def_combat = resolve_melee_logged(defender, attacker, false, 0, !is_unit_a);
            log_.log_combat(def_combat);
            state_.stats.record_wounds(!is_unit_a, def_combat.total_wounds_dealt, def_combat.total_models_killed);
            attacker_wounds = def_combat.total_wounds_dealt;
            defender.set_fatigued(true);

            if (!attacker.is_out_of_action()) {
                CombatEvent atk_combat = resolve_melee_logged(attacker, defender, is_charging, counter_models, is_unit_a);
                log_.log_counter_attack(atk_combat);
                state_.stats.record_wounds(is_unit_a, atk_combat.total_wounds_dealt, atk_combat.total_models_killed);
                defender_wounds = atk_combat.total_wounds_dealt;
                attacker.set_fatigued(true);
            }
        } else {
            // Normal order: attacker first
            CombatEvent atk_combat = resolve_melee_logged(attacker, defender, is_charging, counter_models, is_unit_a);
            log_.log_combat(atk_combat);
            state_.stats.record_wounds(is_unit_a, atk_combat.total_wounds_dealt, atk_combat.total_models_killed);
            defender_wounds = atk_combat.total_wounds_dealt;
            attacker.set_fatigued(true);

            if (!defender.is_out_of_action()) {
                if (defender.is_shaken()) {
                    defender.set_fatigued(true);
                }
                CombatEvent def_combat = resolve_melee_logged(defender, attacker, false, 0, !is_unit_a);
                log_.log_counter_attack(def_combat);
                state_.stats.record_wounds(!is_unit_a, def_combat.total_wounds_dealt, def_combat.total_models_killed);
                attacker_wounds = def_combat.total_wounds_dealt;
                defender.set_fatigued(true);
            }
        }

        // Apply Fear to wound totals for morale comparison
        u16 attacker_effective_wounds = attacker_wounds + attacker.get_rule_value(RuleId::Fear);
        u16 defender_effective_wounds = defender_wounds + defender.get_rule_value(RuleId::Fear);

        // Morale checks for melee loser
        if (defender_effective_wounds > attacker_effective_wounds && !attacker.is_out_of_action()) {
            check_morale_logged(attacker, true, is_unit_a, attacker_wounds, defender_wounds);
        } else if (attacker_effective_wounds > defender_effective_wounds && !defender.is_out_of_action()) {
            check_morale_logged(defender, true, !is_unit_a, defender_wounds, attacker_wounds);
        }

        // Consolidation moves
        bool attacker_destroyed = attacker.is_out_of_action();
        bool defender_destroyed = defender.is_out_of_action();

        if (attacker_destroyed || defender_destroyed) {
            state_.in_melee = false;

            if (attacker_destroyed && !defender_destroyed) {
                i8& survivor_pos = is_unit_a ? state_.pos_b : state_.pos_a;
                i8 old_pos = survivor_pos;
                if (survivor_pos > 0) {
                    survivor_pos = std::max(static_cast<i8>(0), static_cast<i8>(survivor_pos - 3));
                } else if (survivor_pos < 0) {
                    survivor_pos = std::min(static_cast<i8>(0), static_cast<i8>(survivor_pos + 3));
                }
                if (old_pos != survivor_pos) {
                    log_.log_movement("consolidation", old_pos, survivor_pos, "Moving toward objective after combat");
                }
            } else if (defender_destroyed && !attacker_destroyed) {
                i8& survivor_pos = is_unit_a ? state_.pos_a : state_.pos_b;
                i8 old_pos = survivor_pos;
                if (survivor_pos > 0) {
                    survivor_pos = std::max(static_cast<i8>(0), static_cast<i8>(survivor_pos - 3));
                } else if (survivor_pos < 0) {
                    survivor_pos = std::min(static_cast<i8>(0), static_cast<i8>(survivor_pos + 3));
                }
                if (old_pos != survivor_pos) {
                    log_.log_movement("consolidation", old_pos, survivor_pos, "Moving toward objective after combat");
                }
            }
        } else {
            if (is_charging) {
                i8& charger_pos = is_unit_a ? state_.pos_a : state_.pos_b;
                i8& defender_pos = is_unit_a ? state_.pos_b : state_.pos_a;
                i8 old_pos = charger_pos;

                if (charger_pos < defender_pos) {
                    charger_pos -= 1;
                } else {
                    charger_pos += 1;
                }
                state_.in_melee = false;
                log_.log_movement("separation", old_pos, charger_pos, "Charger separates 1\" after combat");
            }
        }
    }

    // Resolve shooting with full logging
    CombatEvent resolve_shooting_logged(UnitView attacker, UnitView defender, i8 distance, bool moved, bool attacker_is_a) {
        CombatEvent event;
        event.phase = "shooting";
        event.attacker_name = std::string(attacker.name().view());
        event.defender_name = std::string(defender.name().view());
        event.is_charging = false;
        event.has_impact = false;

        u32 total_wounds = 0;
        u32 total_models_killed = 0;

        // Process each weapon
        for (u8 w_idx = 0; w_idx < attacker.weapon_count(); ++w_idx) {
            const Weapon& weapon = attacker.get_weapon(w_idx);
            if (!weapon.is_ranged() || weapon.range < static_cast<u8>(distance)) continue;

            AttackSequence attack = resolve_weapon_attack_logged(
                attacker, defender, weapon, w_idx, distance, false, false, 0
            );

            if (attack.total_attacks > 0) {
                event.attacks.push_back(std::move(attack));
            }
        }

        // Sum up total wounds and apply them
        for (const auto& attack : event.attacks) {
            total_wounds += attack.total_wounds;
        }

        if (total_wounds > 0) {
            event.wound_allocation = apply_wounds_logged(defender, total_wounds, false);
            total_models_killed = event.wound_allocation.total_models_killed;
        }

        event.total_wounds_dealt = total_wounds;
        event.total_models_killed = total_models_killed;
        event.target_destroyed = defender.is_destroyed();
        event.target_shaken = defender.is_shaken();
        event.target_routed = defender.is_routed();

        return event;
    }

    // Resolve melee with full logging
    CombatEvent resolve_melee_logged(UnitView attacker, UnitView defender, bool is_charging, u8 counter_models, bool attacker_is_a) {
        CombatEvent event;
        event.phase = "melee";
        event.attacker_name = std::string(attacker.name().view());
        event.defender_name = std::string(defender.name().view());
        event.is_charging = is_charging;
        event.defender_strikes_first = false;

        u32 total_wounds = 0;
        u32 total_models_killed = 0;

        // Impact attacks
        if (is_charging && !attacker.is_fatigued()) {
            u8 impact = attacker.get_rule_value(RuleId::Impact);
            if (impact > counter_models) {
                impact -= counter_models;
            } else {
                impact = 0;
            }

            if (impact > 0) {
                event.has_impact = true;
                event.impact.impact_value = attacker.get_rule_value(RuleId::Impact);
                event.impact.counter_reduction = counter_models;
                event.impact.effective_impact = impact;

                auto impact_result = dice_.roll_impact_logged(impact);
                event.impact.impact_roll.context = "impact";
                event.impact.impact_roll.target = 2;
                event.impact.impact_roll.effective_target = 2;
                for (u8 roll : impact_result.rolls) {
                    DieRoll die;
                    die.value = roll;
                    die.is_success = roll >= 2;
                    die.is_six = roll == 6;
                    event.impact.impact_roll.dice.push_back(die);
                }
                event.impact.impact_roll.total_successes = impact_result.hits;
                event.impact.impact_hits = impact_result.hits;

                if (impact_result.hits > 0) {
                    u8 effective_defense = defender.defense();
                    if (defender.has_rule(RuleId::ShieldWall)) {
                        effective_defense = std::max(u8(2), static_cast<u8>(effective_defense - 1));
                    }

                    auto def_result = dice_.roll_defense_test_logged(impact_result.hits, effective_defense, 0, 0, false);
                    event.impact.impact_defense_roll.context = "defense";
                    event.impact.impact_defense_roll.target = effective_defense;
                    event.impact.impact_defense_roll.effective_target = def_result.effective_target;
                    event.impact.impact_defense_roll.ap_value = 0;
                    for (u8 roll : def_result.rolls) {
                        DieRoll die;
                        die.value = roll;
                        die.is_success = roll >= def_result.effective_target;
                        die.is_six = roll == 6;
                        event.impact.impact_defense_roll.dice.push_back(die);
                    }
                    event.impact.impact_wounds = def_result.wounds;

                    if (def_result.wounds > 0) {
                        event.impact.wound_allocation = apply_wounds_logged(defender, def_result.wounds, false);
                        total_wounds += def_result.wounds;
                        total_models_killed += event.impact.wound_allocation.total_models_killed;
                    }
                }
            }
        }

        // Melee weapon attacks
        for (u8 w_idx = 0; w_idx < attacker.weapon_count(); ++w_idx) {
            const Weapon& weapon = attacker.get_weapon(w_idx);
            if (!weapon.is_melee()) continue;

            AttackSequence attack = resolve_weapon_attack_logged(
                attacker, defender, weapon, w_idx, 0, true, is_charging, counter_models
            );

            if (attack.total_attacks > 0) {
                event.attacks.push_back(std::move(attack));
            }
        }

        // Sum up wounds from weapon attacks
        u32 weapon_wounds = 0;
        for (const auto& attack : event.attacks) {
            weapon_wounds += attack.total_wounds;
        }

        if (weapon_wounds > 0) {
            event.wound_allocation = apply_wounds_logged(defender, weapon_wounds, false);
            total_wounds += weapon_wounds;
            total_models_killed += event.wound_allocation.total_models_killed;
        }

        event.total_wounds_dealt = total_wounds;
        event.total_models_killed = total_models_killed;
        event.target_destroyed = defender.is_destroyed();
        event.target_shaken = defender.is_shaken();
        event.target_routed = defender.is_routed();

        return event;
    }

    // Resolve a single weapon's attacks
    AttackSequence resolve_weapon_attack_logged(
        UnitView attacker, UnitView defender, const Weapon& weapon,
        u8 weapon_idx, i8 distance, bool is_melee, bool is_charging, u8 counter_models
    ) {
        AttackSequence attack;
        attack.attacker_model_name = std::string(attacker.name().view());
        attack.attacker_model_index = 0;

        // Weapon info
        attack.weapon.name = std::string(weapon.name.view());
        attack.weapon.range = weapon.range;
        attack.weapon.attacks = weapon.attacks;
        attack.weapon.ap = weapon.ap;
        attack.weapon.is_melee = weapon.is_melee();
        for (u8 i = 0; i < weapon.rule_count; ++i) {
            if (weapon.rules[i].is_valid()) {
                attack.weapon.rules.push_back(format_rule(weapon.rules[i]));
            }
        }

        u32 attacks = weapon.attacks;
        attack.base_attacks = attacks;
        attack.total_attacks = attacks;

        if (attacks == 0) return attack;

        // Calculate hit modifiers
        u8 quality = attacker.quality();
        i8 hit_modifier = 0;

        if (weapon.has_rule(RuleId::Reliable)) {
            quality = 2;
            attack.hit_modifiers.push_back({"Reliable", 0});
        }

        if (!is_melee && defender.has_rule(RuleId::Stealth) && distance > 9) {
            hit_modifier -= 1;
            attack.hit_modifiers.push_back({"Stealth (>9\")", -1});
        }

        if (is_melee && is_charging && weapon.has_rule(RuleId::Thrust)) {
            hit_modifier += 1;
            attack.hit_modifiers.push_back({"Thrust (charging)", 1});
        }

        bool only_sixes = is_melee && (attacker.is_shaken() || attacker.is_fatigued());
        if (only_sixes) {
            quality = 6;
            hit_modifier = 0;
            attack.hit_modifiers.push_back({"Fatigued/Shaken", 0});
        }

        // Roll to hit
        auto hit_result = dice_.roll_quality_test_logged(attacks, quality, hit_modifier);

        attack.hit_roll.context = "to_hit";
        attack.hit_roll.target = quality;
        attack.hit_roll.modifier = hit_modifier;
        attack.hit_roll.effective_target = hit_result.effective_target;
        for (u8 roll : hit_result.rolls) {
            DieRoll die;
            die.value = roll;
            die.is_success = roll >= hit_result.effective_target;
            die.is_six = roll == 6;
            attack.hit_roll.dice.push_back(die);
        }
        attack.hit_roll.total_successes = hit_result.hits;
        attack.hit_roll.total_sixes = hit_result.sixes;

        u32 hits = hit_result.hits;
        u32 sixes = hit_result.sixes;
        attack.hits_before_modifiers = hits;

        // Rending
        bool has_rending = weapon.has_rule(RuleId::Rending);
        u32 rending_hits = has_rending ? sixes : 0;
        u32 normal_hits = hits - rending_hits;
        attack.rending_hits = rending_hits;

        if (has_rending && rending_hits > 0) {
            attack.triggered_rules.push_back("Rending");
        }

        // Bonus hits from special rules
        if (!is_melee && attacker.has_rule(RuleId::Relentless) && distance > 9) {
            hits += sixes;
            attack.bonus_hits.push_back({"Relentless", sixes});
            attack.triggered_rules.push_back("Relentless");
        }

        if (weapon.has_rule(RuleId::Surge)) {
            hits += sixes;
            attack.bonus_hits.push_back({"Surge", sixes});
            attack.triggered_rules.push_back("Surge");
        }

        if (is_melee && is_charging && attacker.has_rule(RuleId::Furious)) {
            hits += sixes;
            normal_hits = hits - rending_hits;
            attack.bonus_hits.push_back({"Furious", sixes});
            attack.triggered_rules.push_back("Furious");
        }

        // Blast
        u8 blast_value = weapon.get_rule_value(RuleId::Blast);
        if (blast_value > 0) {
            u8 multiplier = std::min(blast_value, static_cast<u8>(defender.alive_count()));
            hits *= multiplier;
            rending_hits *= multiplier;
            normal_hits = hits - rending_hits;
            attack.triggered_rules.push_back("Blast(" + std::to_string(blast_value) + ")");
        }

        attack.hits_after_modifiers = hits;

        // Calculate AP
        u8 ap = weapon.ap;
        attack.base_ap = ap;

        if (is_melee && is_charging && weapon.has_rule(RuleId::Lance)) {
            ap += 2;
            attack.ap_modifiers.push_back({"Lance (charging)", 2});
        }

        if (is_melee && is_charging && weapon.has_rule(RuleId::Thrust)) {
            ap += 1;
            attack.ap_modifiers.push_back({"Thrust (charging)", 1});
        }

        if (is_melee && is_charging && attacker.has_rule(RuleId::PiercingAssault)) {
            ap = std::max(ap, u8(1));
            attack.ap_modifiers.push_back({"Piercing Assault", 0});
        }

        attack.effective_ap = ap;

        // Defense rolls
        bool poison = weapon.has_rule(RuleId::Poison);
        bool has_bane = weapon.has_rule(RuleId::Bane);
        bool reroll_def_sixes = poison || has_bane;

        u8 effective_defense = defender.defense();
        if (is_melee && defender.has_rule(RuleId::ShieldWall)) {
            effective_defense = std::max(u8(2), static_cast<u8>(effective_defense - 1));
        }

        // Roll defense for normal hits
        attack.wounds_from_normal_hits = 0;
        if (normal_hits > 0) {
            auto def_result = dice_.roll_defense_test_logged(normal_hits, effective_defense, ap, 0, reroll_def_sixes);
            attack.defense_roll.context = "defense";
            attack.defense_roll.target = effective_defense;
            attack.defense_roll.effective_target = def_result.effective_target;
            attack.defense_roll.ap_value = ap;
            attack.defense_roll.reroll_sixes = reroll_def_sixes;
            for (u8 roll : def_result.rolls) {
                DieRoll die;
                die.value = roll;
                die.is_success = roll >= def_result.effective_target && (!reroll_def_sixes || roll != 6);
                die.is_six = roll == 6;
                attack.defense_roll.dice.push_back(die);
            }
            attack.defense_roll.sixes_rerolled = def_result.sixes;
            for (u8 roll : def_result.rerolls) {
                DieRoll die;
                die.value = roll;
                die.is_success = roll >= def_result.effective_target;
                die.is_six = roll == 6;
                attack.defense_roll.dice.push_back(die);
            }
            attack.defense_roll.reroll_successes = def_result.reroll_saves;
            attack.wounds_from_normal_hits = def_result.wounds;
        }

        // Roll defense for rending hits (AP+4)
        attack.wounds_from_rending_hits = 0;
        if (rending_hits > 0) {
            u8 rending_ap = ap + 4;
            auto def_result = dice_.roll_defense_test_logged(rending_hits, effective_defense, rending_ap, 0, reroll_def_sixes);
            attack.rending_defense_roll.context = "defense_rending";
            attack.rending_defense_roll.target = effective_defense;
            attack.rending_defense_roll.effective_target = def_result.effective_target;
            attack.rending_defense_roll.ap_value = rending_ap;
            attack.rending_defense_roll.reroll_sixes = reroll_def_sixes;
            for (u8 roll : def_result.rolls) {
                DieRoll die;
                die.value = roll;
                die.is_success = roll >= def_result.effective_target && (!reroll_def_sixes || roll != 6);
                die.is_six = roll == 6;
                attack.rending_defense_roll.dice.push_back(die);
            }
            attack.wounds_from_rending_hits = def_result.wounds;
        }

        attack.total_wounds = attack.wounds_from_normal_hits + attack.wounds_from_rending_hits;

        return attack;
    }

    // Apply wounds with logging
    WoundAllocationSequence apply_wounds_logged(UnitView unit, u32 wounds, bool bypass_regeneration) {
        WoundAllocationSequence seq;
        seq.total_wounds_to_allocate = wounds;

        // Get wound allocation order
        std::array<u8, MAX_MODELS_PER_UNIT> order;
        u8 order_count = 0;
        unit.get_wound_allocation_order(order, order_count);

        for (u8 i = 0; i < order_count; ++i) {
            seq.allocation_order.push_back(std::string(unit.get_model(order[i]).name.view()));
        }

        // Regeneration check
        u32 wounds_after_regen = wounds;
        if (!bypass_regeneration && unit.has_rule(RuleId::Regeneration)) {
            auto regen_result = dice_.roll_regeneration_logged(wounds, 5);
            wounds_after_regen = regen_result.wounds_after;
            seq.total_wounds_regenerated = regen_result.saved;
        }

        seq.total_wounds_dealt = wounds_after_regen;

        // Apply wounds in order
        u32 remaining_wounds = wounds_after_regen;
        for (u8 i = 0; i < order_count && remaining_wounds > 0; ++i) {
            u8 model_idx = order[i];
            if (!unit.model_is_alive(model_idx)) continue;

            ModelWoundEvent evt;
            evt.model_name = std::string(unit.get_model(model_idx).name.view());
            evt.model_index = model_idx;
            evt.tough_value = unit.get_model(model_idx).tough;
            evt.wounds_before = unit.model_wounds_taken(model_idx);

            u8 wounds_to_kill = unit.model_remaining_wounds(model_idx);
            u8 wounds_applied = static_cast<u8>(std::min(remaining_wounds, static_cast<u32>(wounds_to_kill)));
            evt.wounds_allocated = wounds_applied;

            for (u8 w = 0; w < wounds_applied; ++w) {
                if (unit.apply_wound_to_model(model_idx)) {
                    evt.model_killed = true;
                    seq.total_models_killed++;
                    break;
                }
            }

            evt.wounds_after = unit.model_wounds_taken(model_idx);
            seq.allocations.push_back(std::move(evt));

            remaining_wounds -= wounds_applied;
        }

        seq.overkill_wounds = remaining_wounds;
        return seq;
    }

    // Check morale with logging
    void check_morale_logged(UnitView unit, bool is_from_melee, bool is_unit_a, u32 wounds_taken = 0, u32 wounds_dealt = 0) {
        bool needs_test = false;

        if (unit.is_at_half_strength() && !unit.is_shaken() && !unit.is_routed()) {
            needs_test = true;
        }

        if (is_from_melee && wounds_taken > wounds_dealt) {
            needs_test = true;
        }

        if (!needs_test) return;

        MoraleEvent event;
        event.unit_name = std::string(unit.name().view());
        event.is_from_melee = is_from_melee;
        event.wounds_taken = wounds_taken;
        event.wounds_dealt = wounds_dealt;
        event.previous_status = unit_status_to_string(unit.is_shaken() ? UnitStatus::Shaken : UnitStatus::Normal);

        if (is_from_melee && wounds_taken > wounds_dealt) {
            event.trigger_reason = "lost_melee";
        } else {
            event.trigger_reason = "half_strength";
        }

        auto morale_result = dice_.roll_morale_logged(unit.quality(), unit.has_rule(RuleId::Fearless));

        event.quality_target = unit.quality();
        event.morale_roll.context = "morale";
        event.morale_roll.target = unit.quality();
        event.morale_roll.effective_target = unit.quality();
        DieRoll die;
        die.value = morale_result.roll;
        die.is_success = morale_result.roll >= unit.quality();
        die.is_six = morale_result.roll == 6;
        event.morale_roll.dice.push_back(die);
        event.initial_pass = morale_result.roll >= unit.quality();

        event.has_fearless = unit.has_rule(RuleId::Fearless);
        if (morale_result.fearless_used) {
            DieRoll fearless_die;
            fearless_die.value = morale_result.fearless_roll;
            fearless_die.is_success = morale_result.fearless_roll >= 4;
            fearless_die.is_six = morale_result.fearless_roll == 6;
            event.fearless_reroll.context = "fearless_reroll";
            event.fearless_reroll.target = 4;
            event.fearless_reroll.effective_target = 4;
            event.fearless_reroll.dice.push_back(fearless_die);
            event.fearless_pass = morale_result.fearless_passed;
        }

        event.final_pass = morale_result.passed;

        if (morale_result.passed) {
            event.result = "passed";
            event.new_status = unit_status_to_string(unit.is_shaken() ? UnitStatus::Shaken : UnitStatus::Normal);
        } else {
            if (is_from_melee) {
                if (unit.is_at_half_strength()) {
                    unit.rout();
                    event.result = "routed";
                    event.new_status = "Routed";
                } else {
                    unit.become_shaken();
                    event.result = "shaken";
                    event.new_status = "Shaken";
                }
            } else {
                unit.become_shaken();
                event.result = "shaken";
                event.new_status = "Shaken";
            }
        }

        log_.log_morale_check(event);
    }
};

} // namespace export_data
} // namespace battle
