#pragma once

#include "core/types.hpp"
#include "core/unit.hpp"
#include "engine/dice.hpp"
#include "simulation/sim_state.hpp"
#include <algorithm>

namespace battle {

// Forward declare CombatResult (defined in game_state.hpp)
struct CombatResult;

// ==============================================================================
// Combat Engine - Handles shooting and melee resolution (optimized for UnitView)
// ==============================================================================

class CombatEngine {
public:
    explicit CombatEngine(DiceRoller& dice) : dice_(dice) {}

    // Resolve shooting attack
    CombatResult resolve_shooting(UnitView attacker, UnitView defender, i8 distance, bool moved) {
        CombatResult result;

        // Collect all ranged weapons in range
        u32 total_attacks = 0;
        for (u8 i = 0; i < attacker.weapon_count(); ++i) {
            const Weapon& w = attacker.get_weapon(i);
            if (w.is_ranged() && w.range >= static_cast<u8>(distance)) {
                total_attacks += w.attacks;
            }
        }

        if (total_attacks == 0) return result;

        // Process each weapon
        for (u8 i = 0; i < attacker.weapon_count(); ++i) {
            const Weapon& w = attacker.get_weapon(i);
            if (!w.is_ranged() || w.range < static_cast<u8>(distance)) continue;

            u32 attacks = w.attacks;
            if (attacks == 0) continue;

            // Roll to hit
            u8 quality = attacker.quality();
            i8 hit_modifier = 0;

            // Reliable: Quality becomes 2+
            if (w.has_rule(RuleId::Reliable)) {
                quality = 2;
            }

            // Stealth: -1 to hit from >9"
            if (defender.has_rule(RuleId::Stealth) && distance > 9) {
                hit_modifier -= 1;
            }

            auto hit_result = dice_.roll_quality_test(attacks, quality, hit_modifier);
            u32 hits = hit_result.hits;
            u32 sixes = hit_result.sixes;

            // Rending: 6s to hit get AP(+4) - track separately
            bool has_rending = w.has_rule(RuleId::Rending);
            u32 rending_hits = has_rending ? sixes : 0;
            u32 normal_hits = hits - rending_hits;

            // Relentless: extra hits on 6s when shooting >9" (no movement restriction per rules)
            if (attacker.has_rule(RuleId::Relentless) && distance > 9) {
                hits += sixes;
            }

            // Surge: extra hits on 6s to hit
            if (w.has_rule(RuleId::Surge)) {
                hits += sixes;
            }

            // Blast: multiply hits by X, where X is capped at target model count
            u8 blast_value = w.get_rule_value(RuleId::Blast);
            if (blast_value > 0) {
                u8 multiplier = std::min(blast_value, static_cast<u8>(defender.alive_count()));
                hits *= multiplier;
                // Rending hits also multiply with Blast
                rending_hits *= multiplier;
                normal_hits = hits - rending_hits;
            }

            // Roll defense for normal hits
            u8 ap = w.ap;
            bool poison = w.has_rule(RuleId::Poison);
            bool has_bane = w.has_rule(RuleId::Bane);
            // Bane: reroll defense 6s (like Poison)
            bool reroll_def_sixes = poison || has_bane;
            u32 wounds_from_normal = dice_.roll_defense_test(normal_hits, defender.defense(), ap, 0, reroll_def_sixes);

            // Roll defense for rending hits (AP+4) - Rending adds AP(4) to base
            u32 wounds_from_rending = 0;
            if (rending_hits > 0) {
                u8 rending_ap = ap + 4;  // Rending adds +4 AP to base
                wounds_from_rending = dice_.roll_defense_test(rending_hits, defender.defense(), rending_ap, 0, reroll_def_sixes);
            }

            u32 total_wounds = wounds_from_normal + wounds_from_rending;

            // Deadly: handled separately in apply_wounds_deadly
            u8 deadly_value = w.get_rule_value(RuleId::Deadly);

            // Determine if regeneration is bypassed (Bane, Rending, or Unstoppable)
            bool bypass_regen = has_bane || has_rending || w.has_rule(RuleId::Unstoppable);

            // Apply wounds to defender
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
            }
        }

        result.target_destroyed = defender.is_destroyed();
        result.target_shaken = defender.is_shaken();
        result.target_routed = defender.is_routed();

        return result;
    }

    // Resolve melee attack
    // counter_models: number of models with Counter in defender (reduces Impact)
    CombatResult resolve_melee(UnitView attacker, UnitView defender, bool is_charging, u8 counter_models = 0) {
        CombatResult result;

        // Impact: separate roll hitting on 2+ when charging (before normal attacks)
        if (is_charging && !attacker.is_fatigued()) {
            u8 impact = attacker.get_rule_value(RuleId::Impact);
            // Counter reduces Impact by 1 per model with Counter
            if (impact > counter_models) {
                impact -= counter_models;
            } else {
                impact = 0;
            }
            if (impact > 0) {
                u32 impact_hits = dice_.roll_impact(impact);
                if (impact_hits > 0) {
                    // Impact hits use base defense (no AP)
                    u8 effective_defense = defender.defense();
                    if (defender.has_rule(RuleId::ShieldWall)) {
                        effective_defense = std::max(u8(2), static_cast<u8>(effective_defense - 1));
                    }
                    u32 impact_wounds = dice_.roll_defense_test(impact_hits, effective_defense, 0, 0, false);
                    if (impact_wounds > 0) {
                        auto wound_result = apply_wounds(defender, impact_wounds, false);
                        result.wounds_dealt += wound_result.wounds_dealt;
                        result.models_killed += wound_result.models_killed;
                    }
                }
            }
        }

        // Collect all melee weapons
        for (u8 i = 0; i < attacker.weapon_count(); ++i) {
            const Weapon& w = attacker.get_weapon(i);
            if (!w.is_melee()) continue;

            u32 attacks = w.attacks;
            if (attacks == 0) continue;

            // Roll to hit
            u8 quality = attacker.quality();
            i8 hit_modifier = 0;

            // Reliable: Quality becomes 2+
            if (w.has_rule(RuleId::Reliable)) {
                quality = 2;
            }

            // Thrust: +1 to hit when charging
            if (is_charging && w.has_rule(RuleId::Thrust)) {
                hit_modifier += 1;
            }

            // Shaken/Fatigued: Only hit on 6s (unmodified)
            bool only_sixes = attacker.is_shaken() || attacker.is_fatigued();
            if (only_sixes) {
                quality = 6;
                hit_modifier = 0;  // No modifiers when fatigued
            }

            auto hit_result = dice_.roll_quality_test(attacks, quality, hit_modifier);
            u32 hits = hit_result.hits;
            u32 sixes = hit_result.sixes;

            // Rending: 6s to hit get AP(+4)
            bool has_rending = w.has_rule(RuleId::Rending);
            u32 rending_hits = has_rending ? sixes : 0;
            u32 normal_hits = hits - rending_hits;

            // Furious: extra hits on 6s when charging (bonus hits don't get Rending)
            if (is_charging && attacker.has_rule(RuleId::Furious)) {
                hits += sixes;
                normal_hits = hits - rending_hits;
            }

            // Surge: extra hits on 6s to hit
            if (w.has_rule(RuleId::Surge)) {
                hits += sixes;
                normal_hits = hits - rending_hits;
            }

            // Calculate AP
            u8 ap = w.ap;

            // Lance: +2 AP when charging
            if (is_charging && w.has_rule(RuleId::Lance)) {
                ap += 2;
            }

            // Thrust: AP(+1) when charging
            if (is_charging && w.has_rule(RuleId::Thrust)) {
                ap += 1;
            }

            // Piercing Assault: AP(1) on melee when charging
            if (is_charging && attacker.has_rule(RuleId::PiercingAssault)) {
                ap = std::max(ap, u8(1));
            }

            // Blast: multiply hits by X, where X is capped at target model count
            u8 blast_value = w.get_rule_value(RuleId::Blast);
            if (blast_value > 0) {
                u8 multiplier = std::min(blast_value, static_cast<u8>(defender.alive_count()));
                hits *= multiplier;
                rending_hits *= multiplier;
                normal_hits = hits - rending_hits;
            }

            // Roll defense
            bool poison = w.has_rule(RuleId::Poison);
            bool has_bane = w.has_rule(RuleId::Bane);
            bool reroll_def_sixes = poison || has_bane;

            // Shield Wall: +1 to Defense rolls in melee (easier to save)
            u8 effective_defense = defender.defense();
            if (defender.has_rule(RuleId::ShieldWall)) {
                effective_defense = std::max(u8(2), static_cast<u8>(effective_defense - 1));
            }

            u32 wounds_from_normal = dice_.roll_defense_test(normal_hits, effective_defense, ap, 0, reroll_def_sixes);
            u32 wounds_from_rending = 0;
            if (rending_hits > 0) {
                u8 rending_ap = ap + 4;  // Rending adds +4 AP to base
                wounds_from_rending = dice_.roll_defense_test(rending_hits, effective_defense, rending_ap, 0, reroll_def_sixes);
            }

            u32 total_wounds = wounds_from_normal + wounds_from_rending;

            // Deadly: handled separately in apply_wounds_deadly
            u8 deadly_value = w.get_rule_value(RuleId::Deadly);

            // Determine if regeneration is bypassed (Bane, Rending, or Unstoppable)
            bool bypass_regen = has_bane || has_rending || w.has_rule(RuleId::Unstoppable);

            // Apply wounds
            if (total_wounds > 0) {
                WoundResult wound_result;
                if (deadly_value > 1) {
                    wound_result = apply_wounds_deadly(defender, total_wounds, deadly_value, bypass_regen);
                } else {
                    wound_result = apply_wounds(defender, total_wounds, bypass_regen);
                }
                result.wounds_dealt += wound_result.wounds_dealt;
                result.models_killed += wound_result.models_killed;
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
            wounds = dice_.roll_regeneration(wounds, 5);
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
            wounds = dice_.roll_regeneration(wounds, 5);
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
    bool check_morale(UnitView unit, bool is_from_melee = false, u32 melee_wounds_taken = 0, u32 melee_wounds_dealt = 0) {
        // Check if morale test is needed
        bool needs_test = false;

        // At half strength (wounds or models)
        if (unit.is_at_half_strength() && !unit.is_shaken() && !unit.is_routed()) {
            needs_test = true;
        }

        // Lost melee (dealt fewer wounds)
        if (is_from_melee && melee_wounds_taken > melee_wounds_dealt) {
            needs_test = true;
        }

        if (!needs_test) return true;  // Passed (no test needed)

        // Roll morale test
        u8 roll = dice_.roll_d6();
        bool passed = roll >= unit.quality();

        // Fearless: reroll failed test, pass on 4+
        if (!passed && unit.has_rule(RuleId::Fearless)) {
            roll = dice_.roll_d6();
            passed = roll >= 4;
        }

        if (passed) return true;

        // Failed morale - different outcomes for melee vs shooting
        if (is_from_melee) {
            // Melee morale: Rout if at half strength, Shaken otherwise
            if (unit.is_at_half_strength()) {
                unit.rout();
            } else {
                unit.become_shaken();
            }
        } else {
            // General morale (from shooting): Always Shaken, never immediate Rout
            unit.become_shaken();
        }

        return false;
    }

private:
    DiceRoller& dice_;
};

} // namespace battle
