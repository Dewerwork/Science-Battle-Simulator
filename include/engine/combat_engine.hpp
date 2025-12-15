#pragma once

#include "core/types.hpp"
#include "core/unit.hpp"
#include "engine/dice.hpp"
#include "engine/game_state.hpp"
#include <algorithm>

namespace battle {

// ==============================================================================
// Combat Engine - Handles shooting and melee resolution
// ==============================================================================

class CombatEngine {
public:
    explicit CombatEngine(DiceRoller& dice) : dice_(dice) {}

    // Resolve shooting attack
    CombatResult resolve_shooting(Unit& attacker, Unit& defender, i8 distance, bool moved) {
        CombatResult result;

        // Collect all ranged weapons in range
        u32 total_attacks = 0;
        for (u8 i = 0; i < attacker.weapon_count; ++i) {
            const Weapon& w = attacker.weapons[i];
            if (w.is_ranged() && w.range >= static_cast<u8>(distance)) {
                total_attacks += w.attacks;
            }
        }

        if (total_attacks == 0) return result;

        // Process each weapon
        for (u8 i = 0; i < attacker.weapon_count; ++i) {
            const Weapon& w = attacker.weapons[i];
            if (!w.is_ranged() || w.range < static_cast<u8>(distance)) continue;

            u32 attacks = w.attacks;
            if (attacks == 0) continue;

            // Roll to hit
            u8 quality = attacker.quality;
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

            // Rending: 6s to hit get AP(4) - track separately
            u32 rending_hits = w.has_rule(RuleId::Rending) ? sixes : 0;
            u32 normal_hits = hits - rending_hits;

            // Relentless: extra hits on 6s if didn't move
            if (!moved && attacker.has_rule(RuleId::Relentless) && distance > 9) {
                hits += sixes;
            }

            // Blast: multiply hits
            u8 blast_value = w.get_rule_value(RuleId::Blast);
            if (blast_value > 0) {
                // Cap at target model count
                u32 max_hits = defender.alive_count * blast_value;
                hits = std::min(hits * blast_value, max_hits);
                normal_hits = hits - rending_hits;  // Rending hits don't multiply
            }

            // Roll defense for normal hits
            u8 ap = w.ap;
            bool poison = w.has_rule(RuleId::Poison);
            u32 wounds_from_normal = dice_.roll_defense_test(normal_hits, defender.defense, ap, 0, poison);

            // Roll defense for rending hits (AP4)
            u32 wounds_from_rending = 0;
            if (rending_hits > 0) {
                wounds_from_rending = dice_.roll_defense_test(rending_hits, defender.defense, 4, 0, poison);
            }

            u32 total_wounds = wounds_from_normal + wounds_from_rending;

            // Deadly: multiply wounds
            u8 deadly_value = w.get_rule_value(RuleId::Deadly);
            if (deadly_value > 1) {
                total_wounds *= deadly_value;
            }

            // Apply wounds to defender
            if (total_wounds > 0) {
                auto wound_result = apply_wounds(defender, total_wounds, w.has_rule(RuleId::Bane));
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
    CombatResult resolve_melee(Unit& attacker, Unit& defender, bool is_charging) {
        CombatResult result;

        // Collect all melee weapons
        for (u8 i = 0; i < attacker.weapon_count; ++i) {
            const Weapon& w = attacker.weapons[i];
            if (!w.is_melee()) continue;

            u32 attacks = w.attacks;
            if (attacks == 0) continue;

            // Impact: extra attacks on charge
            if (is_charging) {
                u8 impact = attacker.get_rule_value(RuleId::Impact);
                attacks += impact;
            }

            // Roll to hit
            u8 quality = attacker.quality;
            i8 hit_modifier = 0;

            // Reliable: Quality becomes 2+
            if (w.has_rule(RuleId::Reliable)) {
                quality = 2;
            }

            // Shaken/Fatigued: Only hit on 6s
            if (attacker.is_shaken() || attacker.is_fatigued) {
                quality = 6;
            }

            auto hit_result = dice_.roll_quality_test(attacks, quality, hit_modifier);
            u32 hits = hit_result.hits;
            u32 sixes = hit_result.sixes;

            // Rending: 6s to hit get AP(4)
            u32 rending_hits = w.has_rule(RuleId::Rending) ? sixes : 0;
            u32 normal_hits = hits - rending_hits;

            // Furious: extra hits on 6s when charging
            if (is_charging && attacker.has_rule(RuleId::Furious)) {
                hits += sixes;
                normal_hits = hits - rending_hits;
            }

            // Calculate AP
            u8 ap = w.ap;

            // Lance: +2 AP when charging
            if (is_charging && w.has_rule(RuleId::Lance)) {
                ap += 2;
            }

            // Piercing Assault: AP(1) on melee when charging
            if (is_charging && attacker.has_rule(RuleId::PiercingAssault)) {
                ap = std::max(ap, u8(1));
            }

            // Blast for melee
            u8 blast_value = w.get_rule_value(RuleId::Blast);
            if (blast_value > 0) {
                u32 max_hits = defender.alive_count * blast_value;
                hits = std::min(hits * blast_value, max_hits);
                normal_hits = hits - rending_hits;
            }

            // Roll defense
            bool poison = w.has_rule(RuleId::Poison);

            // Shield Wall: +1 Defense in melee
            u8 effective_defense = defender.defense;
            if (defender.has_rule(RuleId::ShieldWall)) {
                effective_defense = std::max(u8(2), static_cast<u8>(effective_defense - 1));
            }

            u32 wounds_from_normal = dice_.roll_defense_test(normal_hits, effective_defense, ap, 0, poison);
            u32 wounds_from_rending = 0;
            if (rending_hits > 0) {
                wounds_from_rending = dice_.roll_defense_test(rending_hits, effective_defense, 4, 0, poison);
            }

            u32 total_wounds = wounds_from_normal + wounds_from_rending;

            // Deadly: multiply wounds
            u8 deadly_value = w.get_rule_value(RuleId::Deadly);
            if (deadly_value > 1) {
                total_wounds *= deadly_value;
            }

            // Apply wounds
            if (total_wounds > 0) {
                auto wound_result = apply_wounds(defender, total_wounds, w.has_rule(RuleId::Bane));
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

    WoundResult apply_wounds(Unit& unit, u32 wounds, bool bypass_regeneration = false) {
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
            Model& model = unit.models[order[i]];
            if (!model.is_alive()) continue;

            u8 wounds_to_kill = model.remaining_wounds();
            u8 wounds_applied = static_cast<u8>(std::min(remaining_wounds, static_cast<u32>(wounds_to_kill)));

            for (u8 w = 0; w < wounds_applied; ++w) {
                if (model.apply_wound()) {
                    result.models_killed++;
                    break;  // Model died, move to next
                }
            }

            remaining_wounds -= wounds_applied;
        }

        unit.update_alive_count();
        return result;
    }

    // Morale check
    bool check_morale(Unit& unit, bool lost_melee = false, u32 melee_wounds_taken = 0, u32 melee_wounds_dealt = 0) {
        // Check if morale test is needed
        bool needs_test = false;

        // At half strength
        if (unit.is_at_half_strength() && unit.status == UnitStatus::Normal) {
            needs_test = true;
        }

        // Lost melee (dealt fewer wounds)
        if (lost_melee && melee_wounds_taken > melee_wounds_dealt) {
            needs_test = true;
        }

        if (!needs_test) return true;  // Passed (no test needed)

        // Roll morale test
        u8 roll = dice_.roll_d6();
        bool passed = roll >= unit.quality;

        // Fearless: reroll failed test, pass on 4+
        if (!passed && unit.has_rule(RuleId::Fearless)) {
            roll = dice_.roll_d6();
            passed = roll >= 4;
        }

        if (passed) return true;

        // Failed morale
        if (unit.is_at_half_strength()) {
            unit.rout();  // Rout if at half strength
        } else {
            unit.become_shaken();  // Become shaken otherwise
        }

        return false;
    }

private:
    DiceRoller& dice_;
};

} // namespace battle
