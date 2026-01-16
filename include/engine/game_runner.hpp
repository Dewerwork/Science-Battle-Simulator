#pragma once

#include "core/types.hpp"
#include "core/unit.hpp"
#include "core/rule_registry.hpp"
#include "core/aura_processor.hpp"
#include "engine/dice.hpp"
#include "engine/game_state.hpp"
#include "engine/registry_combat_resolver.hpp"
#include "engine/ai_controller.hpp"
#include "engine/rule_aware_ai.hpp"
#include "engine/match_logger.hpp"
#include "engine/spell_caster.hpp"
#include "rules/deployment_rules.hpp"
#include <algorithm>

namespace battle {

// ==============================================================================
// Game Runner - Executes a complete game (optimized - no unit copying)
// ==============================================================================
// Uses the new registry-based combat resolver for rule processing.
// Integrates aura processing for faction rules and modifier effects.

class GameRunner {
public:
    explicit GameRunner(DiceRoller& dice, MatchLogger* logger = nullptr)
        : dice_(dice)
        , registry_(create_default_registry())
        , combat_(registry_, dice, logger)
        , spell_caster_(dice, logger)
        , logger_(logger)
    {
        // Register default auras
        register_default_auras(aura_processor_);
    }

    // Run a single game between two units
    GameResult run_game(const Unit& unit_a, const Unit& unit_b) {
        state_.init(unit_a, unit_b);

        // Store original positions before deployment
        i8 orig_pos_a = state_.pos_a;
        i8 orig_pos_b = state_.pos_b;

        // Apply deployment rules (Scout, Ambush, etc.)
        DeploymentProcessor::apply_deployment(state_, unit_a, unit_b);

        if (logger_) {
            logger_->on_game_start(unit_a, unit_b, state_.pos_a, state_.pos_b);

            // Log deployment rule changes
            log_deployment_rules(unit_a, true, orig_pos_a, state_.pos_a);
            log_deployment_rules(unit_b, false, orig_pos_b, state_.pos_b);

            // Log active movement rules for both units
            log_movement_rules(unit_a, true);
            log_movement_rules(unit_b, false);
        }

        // Run up to MAX_ROUNDS
        while (!state_.is_game_over() && state_.current_round <= MAX_ROUNDS) {
            run_round();
        }

        GameResult result = GameResult::determine(state_);

        if (logger_) {
            logger_->on_game_end(result, state_);
        }

        return result;
    }

    // Run a best-of-3 match
    MatchResult run_match(const Unit& unit_a, const Unit& unit_b) {
        MatchResult result;
        result.unit_a_id = unit_a.unit_id;
        result.unit_b_id = unit_b.unit_id;

        if (logger_) {
            logger_->on_match_start(unit_a, unit_b, dice_.get_seed());
        }

        for (int game = 0; game < 3; ++game) {
            // Alternate starting positions each game
            bool positions_swapped = (game % 2 == 1);

            if (logger_) {
                logger_->on_match_game_start(static_cast<u8>(game + 1), positions_swapped);
            }

            GameResult game_result;
            if (!positions_swapped) {
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

        if (logger_) {
            logger_->on_match_end(result);
        }

        return result;
    }

private:
    DiceRoller& dice_;
    RuleRegistry registry_;
    RegistryCombatResolver combat_;
    SpellCaster spell_caster_;  // Handles spell casting phase
    MatchLogger* logger_;
    GameState state_;  // Reusable game state
    AuraProcessor aura_processor_;  // Handles aura effects

    void run_round() {
        if (logger_) {
            logger_->on_round_start(state_.current_round, state_);
        }

        // Clear "deployed this round" flags from previous round
        state_.unit_a_deployed_this_round = false;
        state_.unit_b_deployed_this_round = false;

        // Ambush deployment: Units in reserve can deploy at start of round 2+
        if (state_.current_round >= 2) {
            deploy_ambush_units();
        }

        // Process auras at round start
        // Check if units are from same faction (for friendly auras)
        bool same_faction = state_.unit_a_ptr->faction.view() == state_.unit_b_ptr->faction.view();
        aura_processor_.process_auras_1v1(
            *const_cast<Unit*>(state_.unit_a_ptr),
            *const_cast<Unit*>(state_.unit_b_ptr),
            state_.state_a.modifiers,
            state_.state_b.modifiers,
            state_.pos_a,
            state_.pos_b,
            same_faction
        );

        // Battleborn: 4+ to rally from Shaken at round start
        check_battleborn(true);   // Unit A
        check_battleborn(false);  // Unit B

        // Grant spell tokens at round start (casters get tokens even if in reserve)
        spell_caster_.grant_round_tokens(state_.state_a, true);
        spell_caster_.grant_round_tokens(state_.state_b, false);

        // Determine activation order (random for round 1, alternating after)
        bool a_goes_first;
        u8 initiative_roll = 0;
        const char* initiative_reason;

        if (state_.current_round == 1) {
            initiative_roll = dice_.roll_d6();
            a_goes_first = (initiative_roll >= 4);
            initiative_reason = "random_roll";
        } else {
            // Loser of initiative last round goes first
            // For simplicity, alternate based on round number
            a_goes_first = (state_.current_round % 2 == 1);
            initiative_reason = "alternating";
        }

        if (logger_) {
            logger_->on_initiative(state_.current_round, initiative_roll, a_goes_first, initiative_reason);
        }

        // First activation
        if (a_goes_first) {
            activate_unit(true);
            activate_unit(false);
        } else {
            activate_unit(false);
            activate_unit(true);
        }

        // End-round rule processing (Regeneration, etc.)
        process_end_round_rules(true);   // Unit A
        process_end_round_rules(false);  // Unit B

        // End of round - check objective control
        bool a_in_range = std::abs(state_.pos_a) <= OBJECTIVE_CONTROL_RANGE;
        bool b_in_range = std::abs(state_.pos_b) <= OBJECTIVE_CONTROL_RANGE;
        bool a_shaken = state_.state_a.is_shaken();
        bool b_shaken = state_.state_b.is_shaken();
        bool a_ooa = state_.state_a.is_out_of_action();
        bool b_ooa = state_.state_b.is_out_of_action();

        state_.update_objective_control();

        bool a_controls = state_.unit_a_controls_objective();
        bool b_controls = state_.unit_b_controls_objective();

        const char* control_reason = "none";
        if (a_controls && !b_controls) control_reason = "unit_a_controls";
        else if (b_controls && !a_controls) control_reason = "unit_b_controls";
        else if (a_in_range && b_in_range && !a_shaken && !b_shaken) control_reason = "contested";
        else if (!a_in_range && !b_in_range) control_reason = "no_unit_in_range";

        if (logger_) {
            logger_->on_objective_control_check(
                state_.pos_a, state_.pos_b,
                a_in_range, b_in_range,
                a_shaken, b_shaken,
                a_ooa, b_ooa,
                a_controls, b_controls,
                control_reason);

            logger_->on_round_end(state_.current_round, state_);
        }

        state_.next_round();
    }

    void activate_unit(bool is_unit_a) {
        UnitView unit = state_.view(is_unit_a);
        UnitView enemy = state_.view(!is_unit_a);
        i8& my_pos = is_unit_a ? state_.pos_a : state_.pos_b;

        // Skip units in reserve (Ambush - haven't deployed yet)
        bool in_reserve = is_unit_a ? state_.unit_a_in_reserve : state_.unit_b_in_reserve;
        if (in_reserve) {
            if (is_unit_a) state_.unit_a_activated = true;
            else state_.unit_b_activated = true;
            return;
        }

        // Skip destroyed or routed units entirely (still mark as activated for round progression)
        if (unit.is_out_of_action()) {
            if (is_unit_a) state_.unit_a_activated = true;
            else state_.unit_b_activated = true;
            return;
        }

        if (logger_) {
            logger_->on_activation_start(is_unit_a, state_);
        }

        if (is_unit_a) state_.unit_a_activated = true;
        else state_.unit_b_activated = true;

        // Get AI decision using rule-aware AI
        ActionType action = RuleAwareAI::decide_action(state_, is_unit_a);

        // Log AI decision with context
        if (logger_) {
            i8 dist_to_enemy = state_.distance_between();
            i8 dist_to_objective = static_cast<i8>(std::abs(my_pos));
            bool controls = is_unit_a ? state_.unit_a_controls_objective() : state_.unit_b_controls_objective();

            const char* reasoning = get_ai_reasoning(action, state_, is_unit_a);

            logger_->on_ai_decision(
                is_unit_a,
                unit.unit->ai_type,
                action,
                my_pos,
                dist_to_enemy,
                dist_to_objective,
                controls,
                state_.in_melee,
                unit.is_shaken(),
                unit.is_fatigued(),
                enemy.is_out_of_action(),
                unit.max_range(),
                state_.get_move_speed(*unit.unit),
                reasoning);
        }

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
                if (logger_) {
                    logger_->on_status_changed(is_unit_a, UnitStatus::Shaken, UnitStatus::Normal, "rallied");
                }
                unit.rally();
                break;

            case ActionType::Idle:
            default:
                // Do nothing
                break;
        }

        if (logger_) {
            logger_->on_activation_end(is_unit_a, state_);
        }
    }

    const char* get_ai_reasoning(ActionType action, const GameState& gs, bool is_unit_a) {
        UnitView unit = const_cast<GameState&>(gs).view(is_unit_a);
        UnitView enemy = const_cast<GameState&>(gs).view(!is_unit_a);

        if (unit.is_out_of_action()) return "unit_destroyed_or_routed";
        if (unit.is_shaken() && action == ActionType::Rally) return "shaken_must_rally";
        if (enemy.is_out_of_action()) return "enemy_destroyed_advance_to_objective";
        if (gs.in_melee) return "locked_in_melee";

        i8 dist = gs.distance_between();
        bool can_charge = dist <= gs.get_charge_distance(*unit.unit);

        switch (unit.unit->ai_type) {
            case AIType::Melee:
                if (can_charge) return "melee_ai_in_charge_range";
                return "melee_ai_closing_distance";
            case AIType::Shooting:
                if (unit.max_range() >= static_cast<u8>(dist)) return "shooting_ai_in_range_hold";
                return "shooting_ai_advancing_to_range";
            case AIType::Hybrid:
                if (can_charge) return "hybrid_ai_charge_preferred";
                if (unit.max_range() >= static_cast<u8>(dist)) return "hybrid_ai_shooting";
                return "hybrid_ai_closing_distance";
            default:
                return "default_behavior";
        }
    }

    void execute_hold(bool is_unit_a) {
        UnitView unit = state_.view(is_unit_a);
        UnitView enemy = state_.view(!is_unit_a);

        // Spell phase: Cast spells before attacking
        i8 dist = state_.distance_between();
        spell_caster_.process_spell_phase(unit, enemy, dist, is_unit_a);

        if (state_.in_melee) {
            // Fight in melee
            execute_melee_round(is_unit_a, false);
        } else {
            // Shoot if possible
            if (unit.max_range() >= static_cast<u8>(dist) && !enemy.is_out_of_action()) {
                CombatResult result = combat_.resolve_shooting_phased(unit, enemy, dist, false);
                state_.stats.record_wounds(is_unit_a, result.wounds_dealt, result.models_killed);

                // Check morale for defender if took wounds (not just kills)
                // Morale test when unit is at half strength after taking wounds
                if (result.wounds_dealt > 0) {
                    combat_.check_morale(enemy, false, 0, 0, !is_unit_a);  // false = not from melee
                }
            }
        }
    }

    void execute_advance(bool is_unit_a) {
        UnitView unit = state_.view(is_unit_a);
        UnitView enemy = state_.view(!is_unit_a);
        i8& my_pos = is_unit_a ? state_.pos_a : state_.pos_b;

        if (state_.in_melee) {
            // Spell phase: Cast spells before melee
            i8 dist = state_.distance_between();
            spell_caster_.process_spell_phase(unit, enemy, dist, is_unit_a);
            execute_melee_round(is_unit_a, false);
            return;
        }

        // Move toward center/enemy
        i8 from_pos = my_pos;
        u8 move_speed = state_.get_move_speed(*unit.unit);
        if (is_unit_a) {
            my_pos = std::min(static_cast<i8>(my_pos + move_speed), state_.pos_b);
        } else {
            my_pos = std::max(static_cast<i8>(my_pos - move_speed), state_.pos_a);
        }

        if (logger_) {
            logger_->on_movement(is_unit_a, "advance", from_pos, my_pos,
                                 static_cast<i8>(std::abs(my_pos - from_pos)), "advancing_toward_enemy");
        }

        // Spell phase: Cast spells after movement, before shooting
        i8 dist = state_.distance_between();
        spell_caster_.process_spell_phase(unit, enemy, dist, is_unit_a);

        // Shoot if possible
        if (unit.max_range() >= static_cast<u8>(dist) && !enemy.is_out_of_action()) {
            CombatResult result = combat_.resolve_shooting_phased(unit, enemy, dist, true);
            state_.stats.record_wounds(is_unit_a, result.wounds_dealt, result.models_killed);

            // Check morale for defender if took wounds
            if (result.wounds_dealt > 0) {
                combat_.check_morale(enemy, false, 0, 0, !is_unit_a);  // false = not from melee
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
        i8 from_pos = my_pos;
        u8 move_speed = state_.get_move_speed(*unit.unit) * RUSH_MULTIPLIER;
        if (is_unit_a) {
            my_pos = std::min(static_cast<i8>(my_pos + move_speed), enemy_pos);
        } else {
            my_pos = std::max(static_cast<i8>(my_pos - move_speed), state_.pos_a);
        }

        if (logger_) {
            logger_->on_movement(is_unit_a, "rush", from_pos, my_pos,
                                 static_cast<i8>(std::abs(my_pos - from_pos)), "rushing_toward_enemy");
        }
    }

    void execute_charge(bool is_unit_a) {
        UnitView unit = state_.view(is_unit_a);
        UnitView enemy = state_.view(!is_unit_a);
        i8& my_pos = is_unit_a ? state_.pos_a : state_.pos_b;
        i8& enemy_pos = is_unit_a ? state_.pos_b : state_.pos_a;

        // Move into contact
        i8 from_pos = my_pos;
        if (is_unit_a) {
            my_pos = enemy_pos;
        } else {
            my_pos = state_.pos_a;
        }

        if (logger_) {
            logger_->on_movement(is_unit_a, "charge", from_pos, my_pos,
                                 static_cast<i8>(std::abs(my_pos - from_pos)), "charging_into_melee");
        }

        state_.in_melee = true;

        if (logger_) {
            logger_->on_melee_state_changed(true, "charge_into_contact");
        }

        // Spell phase: Cast spells after movement, before melee
        i8 dist = state_.distance_between();
        spell_caster_.process_spell_phase(unit, enemy, dist, is_unit_a);

        // Resolve charge (attacker strikes first)
        execute_melee_round(is_unit_a, true);
    }

    void execute_melee_round(bool is_unit_a, bool is_charging) {
        UnitView attacker = state_.view(is_unit_a);
        UnitView defender = state_.view(!is_unit_a);

        if (attacker.is_out_of_action() || defender.is_out_of_action()) {
            state_.in_melee = false;
            if (logger_) {
                logger_->on_melee_state_changed(false, "unit_out_of_action");
            }
            return;
        }

        if (logger_) {
            logger_->on_melee_start(is_unit_a, attacker.unit->name.c_str(),
                                    defender.unit->name.c_str(), is_charging, attacker.is_fatigued());
        }

        // Count models with Counter rule in defender (for Impact reduction)
        u8 counter_models = 0;
        if (defender.has_rule(RuleId::Counter)) {
            // If the unit has Counter, all alive models count
            counter_models = defender.alive_count();
        }

        // Counter: defender strikes first when charged (and not shaken)
        bool defender_strikes_first = is_charging && defender.has_rule(RuleId::Counter) && !defender.is_shaken();

        if (logger_) {
            const char* strike_reason = defender_strikes_first ? "counter_rule_active" : "normal_strike_order";
            logger_->on_melee_strike_order(defender_strikes_first, strike_reason);
        }

        u16 attacker_wounds = 0;
        u16 defender_wounds = 0;

        u32 self_destruct_hits_to_attacker = 0;
        u32 self_destruct_hits_to_defender = 0;

        if (defender_strikes_first) {
            // Defender with Counter strikes first
            CombatResult def_result = combat_.resolve_melee_phased(defender, attacker, false, 0);
            state_.stats.record_wounds(!is_unit_a, def_result.wounds_dealt, def_result.models_killed);
            attacker_wounds = def_result.wounds_dealt;
            self_destruct_hits_to_defender += def_result.self_destruct_hits;

            // Mark defender as fatigued after striking
            defender.set_fatigued(true);
            if (logger_) {
                logger_->on_fatigue_changed(!is_unit_a, true, "struck_in_melee");
            }

            if (!attacker.is_out_of_action()) {
                // Attacker strikes back (pass counter_models for Impact reduction)
                CombatResult atk_result = combat_.resolve_melee_phased(attacker, defender, is_charging, counter_models);
                state_.stats.record_wounds(is_unit_a, atk_result.wounds_dealt, atk_result.models_killed);
                defender_wounds = atk_result.wounds_dealt;
                self_destruct_hits_to_attacker += atk_result.self_destruct_hits;

                // Mark attacker as fatigued after striking
                attacker.set_fatigued(true);
                if (logger_) {
                    logger_->on_fatigue_changed(is_unit_a, true, "struck_in_melee");
                }
            }
        } else {
            // Normal order: attacker first (pass counter_models for Impact reduction)
            CombatResult atk_result = combat_.resolve_melee_phased(attacker, defender, is_charging, counter_models);
            state_.stats.record_wounds(is_unit_a, atk_result.wounds_dealt, atk_result.models_killed);
            defender_wounds = atk_result.wounds_dealt;
            self_destruct_hits_to_attacker += atk_result.self_destruct_hits;

            // Mark attacker as fatigued after striking
            attacker.set_fatigued(true);
            if (logger_) {
                logger_->on_fatigue_changed(is_unit_a, true, "struck_in_melee");
            }

            // Defender may strike back if not destroyed
            // Shaken units CAN strike back, but count as fatigued (only hit on 6s)
            if (!defender.is_out_of_action()) {
                // Shaken units strike back counting as fatigued
                if (defender.is_shaken()) {
                    defender.set_fatigued(true);
                    if (logger_) {
                        logger_->on_fatigue_changed(!is_unit_a, true, "shaken_unit_strikes_back");
                    }
                }
                CombatResult def_result = combat_.resolve_melee_phased(defender, attacker, false, 0);
                state_.stats.record_wounds(!is_unit_a, def_result.wounds_dealt, def_result.models_killed);
                attacker_wounds = def_result.wounds_dealt;
                self_destruct_hits_to_defender += def_result.self_destruct_hits;

                // Mark defender as fatigued after striking
                defender.set_fatigued(true);
                if (logger_) {
                    logger_->on_fatigue_changed(!is_unit_a, true, "struck_in_melee");
                }
            }
        }

        // Apply SelfDestruct hits (defender models that died deal hits to attacker, and vice versa)
        if (self_destruct_hits_to_attacker > 0 && !attacker.is_out_of_action()) {
            if (logger_) logger_->on_rule_triggered("SelfDestruct", "applying_hits_to_attacker", self_destruct_hits_to_attacker);
            // SelfDestruct hits roll against quality (hit on 2+)
            u32 destruct_wounds = dice_.roll_d6_target(self_destruct_hits_to_attacker, 2);
            // Roll defense for the wounds
            destruct_wounds = dice_.roll_defense_test(destruct_wounds, attacker.defense(), 0, 0, false);
            if (destruct_wounds > 0) {
                auto wound_result = combat_.apply_wounds(attacker, destruct_wounds, true);  // bypass regen
                attacker_wounds += wound_result.wounds_dealt;
                state_.stats.record_wounds(is_unit_a, wound_result.wounds_dealt, wound_result.models_killed);
            }
        }
        if (self_destruct_hits_to_defender > 0 && !defender.is_out_of_action()) {
            if (logger_) logger_->on_rule_triggered("SelfDestruct", "applying_hits_to_defender", self_destruct_hits_to_defender);
            // SelfDestruct hits roll against quality (hit on 2+)
            u32 destruct_wounds = dice_.roll_d6_target(self_destruct_hits_to_defender, 2);
            // Roll defense for the wounds
            destruct_wounds = dice_.roll_defense_test(destruct_wounds, defender.defense(), 0, 0, false);
            if (destruct_wounds > 0) {
                auto wound_result = combat_.apply_wounds(defender, destruct_wounds, true);  // bypass regen
                defender_wounds += wound_result.wounds_dealt;
                state_.stats.record_wounds(!is_unit_a, wound_result.wounds_dealt, wound_result.models_killed);
            }
        }

        if (logger_) {
            logger_->on_melee_end(is_unit_a, defender_wounds, attacker_wounds,
                                  attacker.is_out_of_action(), defender.is_out_of_action());
        }

        // Apply Fear(X) to wound totals for morale comparison
        // Fear(X) counts as dealing X extra wounds (added to wounds dealt TO opponent)
        // attacker_wounds = wounds attacker received = wounds defender dealt
        // defender_wounds = wounds defender received = wounds attacker dealt
        u16 attacker_effective_wounds = attacker_wounds + defender.get_rule_value(RuleId::Fear);
        u16 defender_effective_wounds = defender_wounds + attacker.get_rule_value(RuleId::Fear);

        // Morale checks for melee loser (compare effective wounds with Fear)
        // The unit that took more effective damage is the LOSER and must take morale
        if (defender_effective_wounds > attacker_effective_wounds && !defender.is_out_of_action()) {
            // Defender took more damage = defender lost = defender takes morale
            combat_.check_morale(defender, true, defender_wounds, attacker_wounds, !is_unit_a);
        } else if (attacker_effective_wounds > defender_effective_wounds && !attacker.is_out_of_action()) {
            // Attacker took more damage = attacker lost = attacker takes morale
            combat_.check_morale(attacker, true, attacker_wounds, defender_wounds, is_unit_a);
        }

        // Consolidation moves (after morale resolution)
        bool attacker_destroyed = attacker.is_out_of_action();
        bool defender_destroyed = defender.is_out_of_action();

        if (attacker_destroyed || defender_destroyed) {
            // One unit destroyed/routed - survivor may move up to 3"
            state_.in_melee = false;
            if (logger_) {
                logger_->on_melee_state_changed(false, "unit_destroyed_or_routed");
            }

            if (attacker_destroyed && !defender_destroyed) {
                // Defender survives - move up to 3" toward objective (center)
                i8& survivor_pos = is_unit_a ? state_.pos_b : state_.pos_a;
                i8 from_pos = survivor_pos;
                if (survivor_pos > 0) {
                    survivor_pos = std::max(static_cast<i8>(0), static_cast<i8>(survivor_pos - 3));
                } else if (survivor_pos < 0) {
                    survivor_pos = std::min(static_cast<i8>(0), static_cast<i8>(survivor_pos + 3));
                }
                if (logger_ && from_pos != survivor_pos) {
                    logger_->on_movement(!is_unit_a, "consolidation", from_pos, survivor_pos,
                                         static_cast<i8>(std::abs(survivor_pos - from_pos)),
                                         "consolidating_toward_objective");
                }
            } else if (defender_destroyed && !attacker_destroyed) {
                // Attacker survives - move up to 3" toward objective (center)
                i8& survivor_pos = is_unit_a ? state_.pos_a : state_.pos_b;
                i8 from_pos = survivor_pos;
                if (survivor_pos > 0) {
                    survivor_pos = std::max(static_cast<i8>(0), static_cast<i8>(survivor_pos - 3));
                } else if (survivor_pos < 0) {
                    survivor_pos = std::min(static_cast<i8>(0), static_cast<i8>(survivor_pos + 3));
                }
                if (logger_ && from_pos != survivor_pos) {
                    logger_->on_movement(is_unit_a, "consolidation", from_pos, survivor_pos,
                                         static_cast<i8>(std::abs(survivor_pos - from_pos)),
                                         "consolidating_toward_objective");
                }
            }
            // Both destroyed - no consolidation needed
        } else {
            // Both units survive - charging unit must move back 1" to separate
            if (is_charging) {
                i8& charger_pos = is_unit_a ? state_.pos_a : state_.pos_b;
                i8 from_pos = charger_pos;

                // Move charger back 1" toward their starting side
                // Unit A starts on negative side, Unit B starts on positive side
                if (is_unit_a) {
                    charger_pos -= 1;  // Unit A moves back toward negative side (left)
                } else {
                    charger_pos += 1;  // Unit B moves back toward positive side (right)
                }
                state_.in_melee = false;

                if (logger_) {
                    logger_->on_movement(is_unit_a, "separation", from_pos, charger_pos, 1,
                                         "charger_separating_after_melee");
                    logger_->on_melee_state_changed(false, "charger_separated");
                }
            } else {
                // HitAndRun: if either unit has this rule and is still in melee,
                // they can disengage and move away their full move distance
                bool attacker_has_hit_and_run = attacker.has_rule(RuleId::HitAndRun);
                bool defender_has_hit_and_run = defender.has_rule(RuleId::HitAndRun);

                if (attacker_has_hit_and_run && !attacker.is_out_of_action()) {
                    // Attacker disengages using HitAndRun
                    i8& pos = is_unit_a ? state_.pos_a : state_.pos_b;
                    i8 from_pos = pos;
                    u8 move_dist = state_.get_move_speed(*attacker.unit);

                    // Move away from defender (toward their starting side)
                    if (is_unit_a) {
                        pos = static_cast<i8>(pos - move_dist);  // Unit A moves back toward negative
                    } else {
                        pos = static_cast<i8>(pos + move_dist);  // Unit B moves back toward positive
                    }

                    state_.in_melee = false;
                    if (logger_) {
                        logger_->on_rule_triggered("HitAndRun", "disengaging", move_dist);
                        logger_->on_movement(is_unit_a, "hit_and_run", from_pos, pos,
                                             static_cast<i8>(move_dist), "hit_and_run_disengage");
                        logger_->on_melee_state_changed(false, "hit_and_run");
                    }
                } else if (defender_has_hit_and_run && !defender.is_out_of_action()) {
                    // Defender disengages using HitAndRun
                    i8& pos = is_unit_a ? state_.pos_b : state_.pos_a;
                    i8 from_pos = pos;
                    u8 move_dist = state_.get_move_speed(*defender.unit);

                    // Move away from attacker (toward their starting side)
                    if (!is_unit_a) {
                        pos = static_cast<i8>(pos - move_dist);  // Unit A moves back toward negative
                    } else {
                        pos = static_cast<i8>(pos + move_dist);  // Unit B moves back toward positive
                    }

                    state_.in_melee = false;
                    if (logger_) {
                        logger_->on_rule_triggered("HitAndRun", "disengaging", move_dist);
                        logger_->on_movement(!is_unit_a, "hit_and_run", from_pos, pos,
                                             static_cast<i8>(move_dist), "hit_and_run_disengage");
                        logger_->on_melee_state_changed(false, "hit_and_run");
                    }
                }
                // If neither has HitAndRun and not charging, units stay engaged
            }
        }
    }

    // Battleborn: 4+ to rally from Shaken at round start
    void check_battleborn(bool is_unit_a) {
        UnitView unit = state_.view(is_unit_a);

        if (!unit.is_shaken()) return;
        if (!unit.has_rule(RuleId::Battleborn)) return;

        u8 roll = dice_.roll_d6();
        bool rallied = roll >= 4;

        if (logger_) {
            logger_->on_rule_triggered("Battleborn", "rally_attempt", roll);
        }

        if (rallied) {
            unit.rally();
            if (logger_) {
                logger_->on_status_changed(is_unit_a, UnitStatus::Shaken, UnitStatus::Normal, "battleborn_rallied");
            }
        }
    }

    // Deploy units from Ambush reserve (at start of round 2+)
    void deploy_ambush_units() {
        // Deploy Unit A from reserve if applicable
        if (state_.unit_a_in_reserve) {
            i8 old_pos = state_.pos_a;
            state_.pos_a = DeploymentProcessor::calculate_ambush_position(
                *state_.unit_a_ptr, true, state_.pos_b);
            state_.unit_a_in_reserve = false;
            state_.unit_a_deployed_this_round = true;

            if (logger_) {
                logger_->on_deployment_rule(true, "Ambush", old_pos, state_.pos_a,
                                            "deployed_from_reserve_9+_from_enemy");
            }
        }

        // Deploy Unit B from reserve if applicable
        if (state_.unit_b_in_reserve) {
            i8 old_pos = state_.pos_b;
            state_.pos_b = DeploymentProcessor::calculate_ambush_position(
                *state_.unit_b_ptr, false, state_.pos_a);
            state_.unit_b_in_reserve = false;
            state_.unit_b_deployed_this_round = true;

            if (logger_) {
                logger_->on_deployment_rule(false, "Ambush", old_pos, state_.pos_b,
                                            "deployed_from_reserve_9+_from_enemy");
            }
        }
    }

    // Log deployment rule effects
    void log_deployment_rules(const Unit& unit, bool is_unit_a, i8 old_pos, i8 new_pos) {
        // Check rules in SAME ORDER as calculate_starting_position (Ambush first!)
        if (unit.has_rule(RuleId::Ambush)) {
            // Ambush units start in reserve - log that they're set aside
            logger_->on_deployment_rule(is_unit_a, "Ambush", old_pos, new_pos,
                                        "set_aside_in_reserve_deploys_round_2+");
        } else if (unit.has_rule(RuleId::Scout)) {
            if (old_pos != new_pos) {
                logger_->on_deployment_rule(is_unit_a, "Scout", old_pos, new_pos,
                                            "forward_deploy_6_inches");
            }
        } else if (old_pos != new_pos) {
            // Unknown deployment rule
            logger_->on_deployment_rule(is_unit_a, "Deployment", old_pos, new_pos,
                                        "position_modified");
        }
    }

    // Log movement rules being applied to a unit
    void log_movement_rules(const Unit& unit, bool is_unit_a) {
        if (unit.has_rule(RuleId::Fast)) {
            logger_->on_movement_rule_applied(is_unit_a, "Fast", 0, "+2_advance_+4_rush_charge");
        }
        if (unit.has_rule(RuleId::Slow)) {
            logger_->on_movement_rule_applied(is_unit_a, "Slow", -2, "-2_inch_move");
        }
        if (unit.has_rule(RuleId::Flying)) {
            logger_->on_movement_rule_applied(is_unit_a, "Flying", 0, "ignores_terrain");
        }
        if (unit.has_rule(RuleId::Strider)) {
            logger_->on_movement_rule_applied(is_unit_a, "Strider", 0, "ignores_difficult_terrain");
        }
        if (unit.has_rule(RuleId::Agile)) {
            logger_->on_movement_rule_applied(is_unit_a, "Agile", 0, "bonus_charge_range");
        }
        if (unit.has_rule(RuleId::RapidCharge)) {
            logger_->on_movement_rule_applied(is_unit_a, "RapidCharge", 0, "charge_after_rush");
        }
        if (unit.has_rule(RuleId::HitAndRun)) {
            logger_->on_movement_rule_applied(is_unit_a, "HitAndRun", 0, "disengage_after_melee");
        }
    }

    // Process end-round rules for a unit
    void process_end_round_rules(bool is_unit_a) {
        UnitView unit = state_.view(is_unit_a);

        // Skip destroyed/routed units
        if (unit.is_out_of_action()) return;

        // Regeneration: Roll for each wound on alive models, 5+ to recover
        if (unit.has_rule(RuleId::Regeneration)) {
            UnitSimState& sim_state = is_unit_a ? state_.state_a : state_.state_b;
            const Unit* unit_ptr = is_unit_a ? state_.unit_a_ptr : state_.unit_b_ptr;

            // Count total wounds on alive models
            u8 total_wounds = 0;
            for (u8 i = 0; i < unit_ptr->model_count; ++i) {
                if (sim_state.models[i].is_alive()) {
                    total_wounds += sim_state.models[i].wounds_taken;
                }
            }

            if (total_wounds > 0) {
                u8 regen_target = unit.get_rule_value(RuleId::Regeneration);
                if (regen_target == 0) regen_target = 5;  // Default 5+

                u8 wounds_healed = 0;
                for (u8 i = 0; i < total_wounds; ++i) {
                    if (dice_.roll_d6() >= regen_target) {
                        wounds_healed++;
                    }
                }

                if (wounds_healed > 0) {
                    // Apply healing to wounded models (most wounded first)
                    u8 remaining_heal = wounds_healed;
                    for (u8 i = 0; i < unit_ptr->model_count && remaining_heal > 0; ++i) {
                        if (sim_state.models[i].is_alive() && sim_state.models[i].wounds_taken > 0) {
                            u8 heal_amount = std::min(remaining_heal, sim_state.models[i].wounds_taken);
                            sim_state.models[i].wounds_taken -= heal_amount;
                            remaining_heal -= heal_amount;
                            if (sim_state.models[i].wounds_taken == 0 &&
                                sim_state.models[i].state == ModelState::Wounded) {
                                sim_state.models[i].state = ModelState::Healthy;
                            }
                        }
                    }

                    if (logger_) {
                        logger_->on_end_round_rule(is_unit_a, "Regeneration",
                                                   "wounds_recovered", wounds_healed);
                    }
                }
            }
        }

        // Fear: Log presence of fear (effect applied during morale checks in combat)
        // (Only log once at start of game, not every round)

        // Fearless, MoraleBoost, HoldTheLine, NoRetreat: Already applied in combat resolver
    }
};

} // namespace battle
