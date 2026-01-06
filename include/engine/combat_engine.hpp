#pragma once

#include "core/types.hpp"
#include "core/unit.hpp"
#include "engine/dice.hpp"
#include "engine/match_logger.hpp"
#include "simulation/sim_state.hpp"
#include <algorithm>
#include <string>

namespace battle {

// Forward declare CombatResult (defined in game_state.hpp)
struct CombatResult;

// ==============================================================================
// Combat Engine - Handles shooting and melee resolution (optimized for UnitView)
// ==============================================================================

class CombatEngine {
public:
    explicit CombatEngine(DiceRoller& dice, MatchLogger* logger = nullptr)
        : dice_(dice), logger_(logger) {}

    // Resolve shooting attack
    CombatResult resolve_shooting(UnitView attacker, UnitView defender, i8 distance, bool moved) {
        CombatResult result;

        // Collect all ranged weapons in range
        u32 total_attacks = 0;
        u8 models_shooting = attacker.alive_count();
        for (u8 i = 0; i < attacker.weapon_count(); ++i) {
            const Weapon& w = attacker.get_weapon(i);
            if (w.is_ranged() && w.range >= static_cast<u8>(distance)) {
                total_attacks += w.attacks;
            }
        }

        if (total_attacks == 0) return result;

        if (logger_) {
            logger_->on_shooting_start(true, attacker.unit->name.c_str(),
                                       defender.unit->name.c_str(), distance, models_shooting);
        }

        // Process each weapon
        for (u8 i = 0; i < attacker.weapon_count(); ++i) {
            const Weapon& w = attacker.get_weapon(i);
            if (!w.is_ranged() || w.range < static_cast<u8>(distance)) continue;

            // Calculate total attacks: min(weapon_count, alive_models) * attacks_per_model
            u8 models_with_weapon = std::min(w.count, models_shooting);
            u32 attacks = static_cast<u32>(models_with_weapon) * w.attacks;
            if (attacks == 0) continue;

            // Log weapon attack start
            if (logger_) {
                std::string rules_str = get_weapon_rules_str(w);
                logger_->on_weapon_attack_start(w.name.c_str(), false, w.range, w.attacks, w.ap, rules_str.c_str());
                logger_->on_attack_count(models_with_weapon, w.attacks, attacks);
            }

            // Roll to hit
            u8 base_quality = attacker.quality();
            u8 quality = base_quality;
            i8 hit_modifier = 0;

            // Reliable: Quality becomes 2+
            if (w.has_rule(RuleId::Reliable)) {
                quality = 2;
                if (logger_) logger_->on_hit_modifier("Reliable", 0, "quality_becomes_2+");
            }

            // Stealth: -1 to hit from >9"
            if (defender.has_rule(RuleId::Stealth) && distance > 9) {
                hit_modifier -= 1;
                if (logger_) logger_->on_hit_modifier("Stealth", -1, "target_beyond_9\"");
            }

            // Enable roll recording for logging
            if (logger_) dice_.enable_roll_recording(true);

            auto hit_result = dice_.roll_quality_test(attacks, quality, hit_modifier);
            u32 hits = hit_result.hits;
            u32 sixes = hit_result.sixes;

            // Log hit rolls
            if (logger_) {
                std::vector<u8> hit_rolls = dice_.take_recorded_rolls();
                i8 effective = static_cast<i8>(quality) - hit_modifier;
                effective = std::max(i8(2), std::min(i8(6), effective));
                logger_->on_hit_rolls(base_quality, hit_modifier, static_cast<u8>(effective), hit_rolls, hits, sixes);
            }

            // Rending: 6s to hit get AP(+4) - track separately
            bool has_rending = w.has_rule(RuleId::Rending);
            u32 rending_hits = has_rending ? sixes : 0;
            u32 normal_hits = hits - rending_hits;
            u32 bonus_hits = 0;

            // Relentless: extra hits on 6s when shooting >9" (no movement restriction per rules)
            if (attacker.has_rule(RuleId::Relentless) && distance > 9) {
                bonus_hits += sixes;
                hits += sixes;
                if (logger_) logger_->on_rule_triggered("Relentless", "extra_hits_on_6s", sixes);
            }

            // Surge: extra hits on 6s to hit
            if (w.has_rule(RuleId::Surge)) {
                bonus_hits += sixes;
                hits += sixes;
                if (logger_) logger_->on_rule_triggered("Surge", "extra_hits_on_6s", sixes);
            }

            // Blast: multiply hits by X, where X is capped at target model count
            u8 blast_value = w.get_rule_value(RuleId::Blast);
            if (blast_value > 0) {
                u8 multiplier = std::min(blast_value, static_cast<u8>(defender.alive_count()));
                u32 old_hits = hits;
                hits *= multiplier;
                // Rending hits also multiply with Blast
                rending_hits *= multiplier;
                normal_hits = hits - rending_hits;
                if (logger_) logger_->on_rule_triggered("Blast", "multiplied_hits", hits - old_hits);
            }

            // Log hits after modifiers
            if (logger_) {
                logger_->on_hits_after_modifiers(normal_hits, rending_hits, hits);
            }

            // Roll defense for normal hits
            u8 ap = w.ap;
            bool poison = w.has_rule(RuleId::Poison);
            bool has_bane = w.has_rule(RuleId::Bane);
            // Bane: reroll defense 6s (like Poison)
            bool reroll_def_sixes = poison || has_bane;

            if (logger_) dice_.enable_roll_recording(true);
            u32 wounds_from_normal = dice_.roll_defense_test(normal_hits, defender.defense(), ap, 0, reroll_def_sixes);

            // Log defense rolls for normal hits
            if (logger_) {
                std::vector<u8> def_rolls = dice_.take_recorded_rolls();
                i8 effective = static_cast<i8>(defender.defense()) + static_cast<i8>(ap);
                effective = std::max(i8(2), std::min(i8(6), effective));
                u32 saves = normal_hits - wounds_from_normal;
                logger_->on_defense_rolls(defender.defense(), ap, static_cast<u8>(effective), reroll_def_sixes,
                                          def_rolls, saves, wounds_from_normal, 0, {}, 0);
            }

            // Roll defense for rending hits (AP+4) - Rending adds AP(4) to base
            u32 wounds_from_rending = 0;
            if (rending_hits > 0) {
                u8 rending_ap = ap + 4;  // Rending adds +4 AP to base
                if (logger_) dice_.enable_roll_recording(true);
                wounds_from_rending = dice_.roll_defense_test(rending_hits, defender.defense(), rending_ap, 0, reroll_def_sixes);

                if (logger_) {
                    std::vector<u8> rend_rolls = dice_.take_recorded_rolls();
                    i8 effective = static_cast<i8>(defender.defense()) + static_cast<i8>(rending_ap);
                    effective = std::max(i8(2), std::min(i8(6), effective));
                    u32 saves = rending_hits - wounds_from_rending;
                    logger_->on_defense_rolls_rending(defender.defense(), rending_ap, static_cast<u8>(effective),
                                                      rend_rolls, saves, wounds_from_rending);
                }
            }

            // Disable roll recording
            if (logger_) dice_.enable_roll_recording(false);

            u32 total_wounds = wounds_from_normal + wounds_from_rending;

            // Deadly: handled separately in apply_wounds_deadly
            u8 deadly_value = w.get_rule_value(RuleId::Deadly);

            // Determine if regeneration is bypassed (Bane, Rending, or Unstoppable)
            bool bypass_regen = has_bane || has_rending || w.has_rule(RuleId::Unstoppable);

            // Apply wounds to defender
            u8 weapon_models_killed = 0;
            if (total_wounds > 0) {
                WoundResult wound_result;
                if (deadly_value > 1) {
                    // Deadly wounds don't carry over - apply per-wound with multiplier
                    wound_result = apply_wounds_deadly(defender, total_wounds, deadly_value, bypass_regen);
                } else {
                    wound_result = apply_wounds(defender, total_wounds, bypass_regen);
                }
                result.wounds_dealt += wound_result.wounds_dealt;
                result.models_killed += wound_result.models_killed;
                weapon_models_killed = wound_result.models_killed;
            }

            if (logger_) {
                logger_->on_weapon_attack_end(w.name.c_str(), total_wounds, weapon_models_killed);
            }
        }

        result.target_destroyed = defender.is_destroyed();
        result.target_shaken = defender.is_shaken();
        result.target_routed = defender.is_routed();

        if (logger_) {
            logger_->on_shooting_end(true, result.wounds_dealt, result.models_killed, result.target_destroyed);
        }

        return result;
    }

    // Resolve melee attack
    // counter_models: number of models with Counter in defender (reduces Impact)
    CombatResult resolve_melee(UnitView attacker, UnitView defender, bool is_charging, u8 counter_models = 0) {
        CombatResult result;

        // Impact: separate roll hitting on 2+ when charging (before normal attacks)
        if (is_charging && !attacker.is_fatigued()) {
            u8 base_impact = attacker.get_rule_value(RuleId::Impact);
            u8 impact = base_impact;
            // Counter reduces Impact by 1 per model with Counter
            if (impact > counter_models) {
                impact -= counter_models;
            } else {
                impact = 0;
            }

            if (logger_ && base_impact > 0) {
                logger_->on_impact_start(true, base_impact, counter_models, impact);
            }

            if (impact > 0) {
                if (logger_) dice_.enable_roll_recording(true);
                u32 impact_hits = dice_.roll_impact(impact);

                if (logger_) {
                    std::vector<u8> impact_rolls = dice_.take_recorded_rolls();
                    logger_->on_impact_rolls(impact_rolls, 2, impact_hits);
                }

                if (impact_hits > 0) {
                    // Impact hits use base defense (no AP)
                    u8 effective_defense = defender.defense();
                    if (defender.has_rule(RuleId::ShieldWall)) {
                        effective_defense = std::max(u8(2), static_cast<u8>(effective_defense - 1));
                    }

                    if (logger_) dice_.enable_roll_recording(true);
                    u32 impact_wounds = dice_.roll_defense_test(impact_hits, effective_defense, 0, 0, false);

                    if (logger_) {
                        std::vector<u8> def_rolls = dice_.take_recorded_rolls();
                        u32 saves = impact_hits - impact_wounds;
                        logger_->on_impact_defense(def_rolls, defender.defense(), effective_defense, saves, impact_wounds);
                    }

                    if (impact_wounds > 0) {
                        auto wound_result = apply_wounds(defender, impact_wounds, false);
                        result.wounds_dealt += wound_result.wounds_dealt;
                        result.models_killed += wound_result.models_killed;

                        if (logger_) {
                            logger_->on_impact_end(wound_result.wounds_dealt, wound_result.models_killed);
                        }
                    } else if (logger_) {
                        logger_->on_impact_end(0, 0);
                    }
                } else if (logger_) {
                    logger_->on_impact_end(0, 0);
                }
            }

            if (logger_) dice_.enable_roll_recording(false);
        }

        // Collect all melee weapons
        u8 models_attacking = attacker.alive_count();
        for (u8 i = 0; i < attacker.weapon_count(); ++i) {
            const Weapon& w = attacker.get_weapon(i);
            if (!w.is_melee()) continue;

            // Calculate total attacks: min(weapon_count, alive_models) * attacks_per_model
            u8 models_with_weapon = std::min(w.count, models_attacking);
            u32 attacks = static_cast<u32>(models_with_weapon) * w.attacks;
            if (attacks == 0) continue;

            // Log weapon attack start
            if (logger_) {
                std::string rules_str = get_weapon_rules_str(w);
                logger_->on_weapon_attack_start(w.name.c_str(), true, 0, w.attacks, w.ap, rules_str.c_str());
                logger_->on_attack_count(models_with_weapon, w.attacks, attacks);
            }

            // Roll to hit
            u8 base_quality = attacker.quality();
            u8 quality = base_quality;
            i8 hit_modifier = 0;

            // Reliable: Quality becomes 2+
            if (w.has_rule(RuleId::Reliable)) {
                quality = 2;
                if (logger_) logger_->on_hit_modifier("Reliable", 0, "quality_becomes_2+");
            }

            // Thrust: +1 to hit when charging
            if (is_charging && w.has_rule(RuleId::Thrust)) {
                hit_modifier += 1;
                if (logger_) logger_->on_hit_modifier("Thrust", +1, "charging_bonus");
            }

            // Shaken/Fatigued: Only hit on 6s (unmodified)
            bool only_sixes = attacker.is_shaken() || attacker.is_fatigued();
            if (only_sixes) {
                quality = 6;
                hit_modifier = 0;  // No modifiers when fatigued
                if (logger_) logger_->on_hit_modifier("Fatigued/Shaken", 0, "only_hit_on_6s");
            }

            // Enable roll recording for logging
            if (logger_) dice_.enable_roll_recording(true);

            auto hit_result = dice_.roll_quality_test(attacks, quality, hit_modifier);
            u32 hits = hit_result.hits;
            u32 sixes = hit_result.sixes;

            // Log hit rolls
            if (logger_) {
                std::vector<u8> hit_rolls = dice_.take_recorded_rolls();
                i8 effective = static_cast<i8>(quality) - hit_modifier;
                effective = std::max(i8(2), std::min(i8(6), effective));
                logger_->on_hit_rolls(base_quality, hit_modifier, static_cast<u8>(effective), hit_rolls, hits, sixes);
            }

            // Rending: 6s to hit get AP(+4)
            bool has_rending = w.has_rule(RuleId::Rending);
            u32 rending_hits = has_rending ? sixes : 0;
            u32 normal_hits = hits - rending_hits;

            // Furious: extra hits on 6s when charging (bonus hits don't get Rending)
            if (is_charging && attacker.has_rule(RuleId::Furious)) {
                hits += sixes;
                normal_hits = hits - rending_hits;
                if (logger_) logger_->on_rule_triggered("Furious", "extra_hits_on_6s_when_charging", sixes);
            }

            // Surge: extra hits on 6s to hit
            if (w.has_rule(RuleId::Surge)) {
                hits += sixes;
                normal_hits = hits - rending_hits;
                if (logger_) logger_->on_rule_triggered("Surge", "extra_hits_on_6s", sixes);
            }

            // Calculate AP
            u8 ap = w.ap;

            // Lance: +2 AP when charging
            if (is_charging && w.has_rule(RuleId::Lance)) {
                ap += 2;
                if (logger_) logger_->on_rule_triggered("Lance", "ap_+2_when_charging", 2);
            }

            // Thrust: AP(+1) when charging
            if (is_charging && w.has_rule(RuleId::Thrust)) {
                ap += 1;
                if (logger_) logger_->on_rule_triggered("Thrust", "ap_+1_when_charging", 1);
            }

            // Piercing Assault: AP(1) on melee when charging
            if (is_charging && attacker.has_rule(RuleId::PiercingAssault)) {
                u8 old_ap = ap;
                ap = std::max(ap, u8(1));
                if (logger_ && ap > old_ap) logger_->on_rule_triggered("PiercingAssault", "minimum_ap_1", 1);
            }

            // Blast: multiply hits by X, where X is capped at target model count
            u8 blast_value = w.get_rule_value(RuleId::Blast);
            if (blast_value > 0) {
                u8 multiplier = std::min(blast_value, static_cast<u8>(defender.alive_count()));
                u32 old_hits = hits;
                hits *= multiplier;
                rending_hits *= multiplier;
                normal_hits = hits - rending_hits;
                if (logger_) logger_->on_rule_triggered("Blast", "multiplied_hits", hits - old_hits);
            }

            // Log hits after modifiers
            if (logger_) {
                logger_->on_hits_after_modifiers(normal_hits, rending_hits, hits);
            }

            // Roll defense
            bool poison = w.has_rule(RuleId::Poison);
            bool has_bane = w.has_rule(RuleId::Bane);
            // Bane in Melee: unit rule that gives all melee attacks Bane effect
            bool has_bane_in_melee = attacker.has_rule(RuleId::BaneInMelee);
            bool reroll_def_sixes = poison || has_bane || has_bane_in_melee;

            // Shield Wall: +1 to Defense rolls in melee (easier to save)
            u8 effective_defense = defender.defense();
            if (defender.has_rule(RuleId::ShieldWall)) {
                effective_defense = std::max(u8(2), static_cast<u8>(effective_defense - 1));
                if (logger_) logger_->on_defense_modifier("ShieldWall", -1, "easier_saves_in_melee");
            }

            if (logger_) dice_.enable_roll_recording(true);
            u32 wounds_from_normal = dice_.roll_defense_test(normal_hits, effective_defense, ap, 0, reroll_def_sixes);

            // Log defense rolls for normal hits
            if (logger_) {
                std::vector<u8> def_rolls = dice_.take_recorded_rolls();
                i8 effective = static_cast<i8>(effective_defense) + static_cast<i8>(ap);
                effective = std::max(i8(2), std::min(i8(6), effective));
                u32 saves = normal_hits - wounds_from_normal;
                logger_->on_defense_rolls(defender.defense(), ap, static_cast<u8>(effective), reroll_def_sixes,
                                          def_rolls, saves, wounds_from_normal, 0, {}, 0);
            }

            u32 wounds_from_rending = 0;
            if (rending_hits > 0) {
                u8 rending_ap = ap + 4;  // Rending adds +4 AP to base
                if (logger_) dice_.enable_roll_recording(true);
                wounds_from_rending = dice_.roll_defense_test(rending_hits, effective_defense, rending_ap, 0, reroll_def_sixes);

                if (logger_) {
                    std::vector<u8> rend_rolls = dice_.take_recorded_rolls();
                    i8 effective = static_cast<i8>(effective_defense) + static_cast<i8>(rending_ap);
                    effective = std::max(i8(2), std::min(i8(6), effective));
                    u32 saves = rending_hits - wounds_from_rending;
                    logger_->on_defense_rolls_rending(defender.defense(), rending_ap, static_cast<u8>(effective),
                                                      rend_rolls, saves, wounds_from_rending);
                }
            }

            // Disable roll recording
            if (logger_) dice_.enable_roll_recording(false);

            u32 total_wounds = wounds_from_normal + wounds_from_rending;

            // Deadly: handled separately in apply_wounds_deadly
            u8 deadly_value = w.get_rule_value(RuleId::Deadly);

            // Determine if regeneration is bypassed (Bane, Bane in Melee, Rending, or Unstoppable)
            bool bypass_regen = has_bane || has_bane_in_melee || has_rending || w.has_rule(RuleId::Unstoppable);

            // Apply wounds
            u8 weapon_models_killed = 0;
            if (total_wounds > 0) {
                WoundResult wound_result;
                if (deadly_value > 1) {
                    wound_result = apply_wounds_deadly(defender, total_wounds, deadly_value, bypass_regen);
                } else {
                    wound_result = apply_wounds(defender, total_wounds, bypass_regen);
                }
                result.wounds_dealt += wound_result.wounds_dealt;
                result.models_killed += wound_result.models_killed;
                weapon_models_killed = wound_result.models_killed;
            }

            if (logger_) {
                logger_->on_weapon_attack_end(w.name.c_str(), total_wounds, weapon_models_killed);
            }
        }

        result.target_destroyed = defender.is_destroyed();
        result.target_shaken = defender.is_shaken();
        result.target_routed = defender.is_routed();

        return result;
    }

    // Apply wounds to a unit with proper wound allocation
    struct WoundResult {
        u16 wounds_dealt = 0;
        u8 models_killed = 0;
    };

    WoundResult apply_wounds(UnitView unit, u32 wounds, bool bypass_regeneration = false) {
        WoundResult result;

        // Get wound allocation order
        std::array<u8, MAX_MODELS_PER_UNIT> order;
        u8 order_count = 0;
        unit.get_wound_allocation_order(order, order_count);

        // Regeneration check
        if (!bypass_regeneration && unit.has_rule(RuleId::Regeneration)) {
            u32 original_wounds = wounds;
            wounds = dice_.roll_regeneration(wounds, 5);
            if (logger_) {
                u32 blocked = original_wounds - wounds;
                logger_->on_rule_triggered("Regeneration", "blocked_wounds", blocked);
            }
        }

        result.wounds_dealt = static_cast<u16>(wounds);

        // Apply wounds in order
        u32 remaining_wounds = wounds;
        for (u8 i = 0; i < order_count && remaining_wounds > 0; ++i) {
            u8 model_idx = order[i];
            if (!unit.model_is_alive(model_idx)) continue;

            u8 wounds_to_kill = unit.model_remaining_wounds(model_idx);
            u8 wounds_applied = static_cast<u8>(std::min(remaining_wounds, static_cast<u32>(wounds_to_kill)));

            for (u8 w = 0; w < wounds_applied; ++w) {
                if (unit.apply_wound_to_model(model_idx)) {
                    result.models_killed++;
                    break;  // Model died, move to next
                }
            }

            remaining_wounds -= wounds_applied;
        }

        return result;
    }

    // Apply wounds with Deadly - wounds don't carry over to other models
    // Each wound is multiplied by deadly_value and assigned to one model
    WoundResult apply_wounds_deadly(UnitView unit, u32 wounds, u8 deadly_value, bool bypass_regeneration = false) {
        WoundResult result;

        // Get wound allocation order
        std::array<u8, MAX_MODELS_PER_UNIT> order;
        u8 order_count = 0;
        unit.get_wound_allocation_order(order, order_count);

        // Regeneration check (before multiplying for Deadly)
        if (!bypass_regeneration && unit.has_rule(RuleId::Regeneration)) {
            u32 original_wounds = wounds;
            wounds = dice_.roll_regeneration(wounds, 5);
            if (logger_) {
                u32 blocked = original_wounds - wounds;
                logger_->on_rule_triggered("Regeneration", "blocked_wounds", blocked);
            }
        }

        // Each wound is multiplied by deadly_value but doesn't carry over
        u8 order_idx = 0;
        for (u32 w = 0; w < wounds && order_idx < order_count; ++w) {
            // Get next alive model
            while (order_idx < order_count && !unit.model_is_alive(order[order_idx])) {
                order_idx++;
            }
            if (order_idx >= order_count) break;

            u8 model_idx = order[order_idx];
            u8 model_wounds_remaining = unit.model_remaining_wounds(model_idx);

            // Apply deadly_value wounds to this model (capped at what would kill it)
            u8 wounds_to_apply = std::min(deadly_value, model_wounds_remaining);
            result.wounds_dealt += wounds_to_apply;

            for (u8 d = 0; d < wounds_to_apply; ++d) {
                if (unit.apply_wound_to_model(model_idx)) {
                    result.models_killed++;
                    order_idx++;  // Move to next model for next wound
                    break;
                }
            }
            // Note: excess wounds from deadly are lost (don't carry over)
        }

        return result;
    }

    // Morale check
    // is_from_melee: true if this check is from losing melee combat
    // is_unit_a: for logging purposes
    bool check_morale(UnitView unit, bool is_from_melee = false, u32 melee_wounds_taken = 0, u32 melee_wounds_dealt = 0, bool is_unit_a = true) {
        // Check if morale test is needed
        bool needs_test = false;
        const char* trigger_reason = "";

        // At half strength (wounds or models)
        if (unit.is_at_half_strength() && !unit.is_shaken() && !unit.is_routed()) {
            needs_test = true;
            trigger_reason = "half_strength";
        }

        // Lost melee (dealt fewer wounds)
        if (is_from_melee && melee_wounds_taken > melee_wounds_dealt) {
            needs_test = true;
            trigger_reason = "lost_melee";
        }

        if (!needs_test) return true;  // Passed (no test needed)

        UnitStatus old_status = unit.state->status;

        if (logger_) {
            logger_->on_morale_check_start(is_unit_a, unit.unit->name.c_str(), trigger_reason,
                                           unit.alive_count(), unit.unit->model_count,
                                           static_cast<u16>(melee_wounds_taken), static_cast<u16>(melee_wounds_dealt));
        }

        // Roll morale test
        u8 roll = dice_.roll_d6();
        bool passed = roll >= unit.quality();

        if (logger_) {
            logger_->on_morale_roll(roll, unit.quality(), passed);
        }

        // Fearless: reroll failed test, pass on 4+
        u8 fearless_roll = 0;
        bool fearless_passed = false;
        if (!passed && unit.has_rule(RuleId::Fearless)) {
            fearless_roll = dice_.roll_d6();
            fearless_passed = fearless_roll >= 4;
            passed = fearless_passed;

            if (logger_) {
                logger_->on_fearless_roll(fearless_roll, 4, fearless_passed);
            }
        }

        // Hold the Line: reroll failed morale test using quality
        u8 hold_line_roll = 0;
        bool hold_line_passed = false;
        if (!passed && unit.has_rule(RuleId::HoldTheLine)) {
            hold_line_roll = dice_.roll_d6();
            hold_line_passed = hold_line_roll >= unit.quality();
            passed = hold_line_passed;

            if (logger_) {
                // Reuse fearless roll logging - similar mechanic
                logger_->on_fearless_roll(hold_line_roll, unit.quality(), hold_line_passed);
            }
        }

        if (passed) {
            if (logger_) {
                logger_->on_morale_check_end(true, old_status, old_status, "passed");
            }
            return true;
        }

        // Failed morale - different outcomes for melee vs shooting
        UnitStatus new_status = old_status;
        const char* result_desc = "";

        if (is_from_melee) {
            // Melee morale: Rout if at half strength, Shaken otherwise
            if (unit.is_at_half_strength()) {
                unit.rout();
                new_status = UnitStatus::Routed;
                result_desc = "routed_at_half_strength";
            } else {
                unit.become_shaken();
                new_status = UnitStatus::Shaken;
                result_desc = "shaken_from_melee";
            }
        } else {
            // General morale (from shooting): Always Shaken, never immediate Rout
            unit.become_shaken();
            new_status = UnitStatus::Shaken;
            result_desc = "shaken_from_shooting";
        }

        if (logger_) {
            logger_->on_morale_check_end(false, old_status, new_status, result_desc);
            logger_->on_status_changed(is_unit_a, old_status, new_status, result_desc);
        }

        return false;
    }

private:
    DiceRoller& dice_;
    MatchLogger* logger_;

    // Helper to build weapon rules string
    std::string get_weapon_rules_str(const Weapon& w) {
        std::string rules;
        if (w.has_rule(RuleId::Rending)) rules += "Rending,";
        if (w.has_rule(RuleId::Deadly)) {
            rules += "Deadly(" + std::to_string(w.get_rule_value(RuleId::Deadly)) + "),";
        }
        if (w.has_rule(RuleId::Blast)) {
            rules += "Blast(" + std::to_string(w.get_rule_value(RuleId::Blast)) + "),";
        }
        if (w.has_rule(RuleId::Poison)) rules += "Poison,";
        if (w.has_rule(RuleId::Bane)) rules += "Bane,";
        if (w.has_rule(RuleId::Reliable)) rules += "Reliable,";
        if (w.has_rule(RuleId::Surge)) rules += "Surge,";
        if (w.has_rule(RuleId::Lance)) rules += "Lance,";
        if (w.has_rule(RuleId::Thrust)) rules += "Thrust,";
        if (w.has_rule(RuleId::Unstoppable)) rules += "Unstoppable,";
        if (!rules.empty()) rules.pop_back(); // Remove trailing comma
        return rules;
    }
};

} // namespace battle
