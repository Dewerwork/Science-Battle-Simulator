#pragma once

#include "core/types.hpp"
#include "core/unit.hpp"
#include "core/faction_rules.hpp"
#include "engine/dice.hpp"
#include <algorithm>

namespace battle {

// ==============================================================================
// Combat Modifiers from Faction Rules
// ==============================================================================

struct FactionCombatModifiers {
    i8 hit_modifier = 0;           // +/- to hit rolls
    i8 defense_modifier = 0;       // +/- to defense rolls
    i8 morale_modifier = 0;        // +/- to morale tests
    i8 ap_modifier = 0;            // +/- to AP
    u8 extra_attacks = 0;          // Additional attacks
    u8 extra_hits = 0;             // Additional hits
    u8 extra_wounds = 0;           // Additional wounds
    bool ignores_regeneration = false;  // Bypass regeneration

    // Granted rules from auras
    RuleMask granted_rules = 0;

    void add_rule(RuleId id) {
        granted_rules |= rule_bit(id);
    }

    bool has_granted_rule(RuleId id) const {
        return (granted_rules & rule_bit(id)) != 0;
    }

    void reset() {
        hit_modifier = 0;
        defense_modifier = 0;
        morale_modifier = 0;
        ap_modifier = 0;
        extra_attacks = 0;
        extra_hits = 0;
        extra_wounds = 0;
        ignores_regeneration = false;
        granted_rules = 0;
    }
};

// ==============================================================================
// Faction Rule Applicator - Applies faction rules to combat
// ==============================================================================

class FactionRuleApplicator {
public:
    FactionRuleApplicator() = default;

    // Calculate modifiers for an attacker based on faction rules
    FactionCombatModifiers calculate_attack_modifiers(
        const Unit& attacker,
        const Unit& defender,
        CombatPhase phase,
        bool is_charging
    ) {
        FactionCombatModifiers mods;

        // Get faction rules for attacker
        const auto* faction_rules = get_faction_registry().get_faction(attacker.faction.view());
        if (!faction_rules) return mods;

        // Apply army-wide rules
        apply_army_wide_rules(*faction_rules, attacker, defender, phase, is_charging, mods);

        // Apply unit's special rules (based on rules the unit has)
        apply_unit_special_rules(*faction_rules, attacker, defender, phase, is_charging, mods);

        // Apply aura rules (from leaders/heroes in the unit)
        apply_aura_rules(*faction_rules, attacker, defender, phase, is_charging, mods);

        return mods;
    }

    // Calculate modifiers for a defender based on faction rules
    FactionCombatModifiers calculate_defense_modifiers(
        const Unit& defender,
        const Unit& attacker,
        CombatPhase phase
    ) {
        FactionCombatModifiers mods;

        // Get faction rules for defender
        const auto* faction_rules = get_faction_registry().get_faction(defender.faction.view());
        if (!faction_rules) return mods;

        // Apply defensive army-wide rules
        for (u8 i = 0; i < faction_rules->army_wide_count; ++i) {
            const auto& rule = faction_rules->army_wide_rules[i];
            apply_defensive_effect(rule, defender, mods);
        }

        // Apply defensive aura rules
        for (u8 i = 0; i < faction_rules->aura_count; ++i) {
            const auto& rule = faction_rules->aura_rules[i];
            if (rule.category == FactionRuleCategory::Defense ||
                rule.category == FactionRuleCategory::AuraEffect) {
                apply_defensive_effect(rule, defender, mods);
            }
        }

        return mods;
    }

    // Apply combat modifiers to an attack
    void apply_to_attack(
        const FactionCombatModifiers& mods,
        u8& attacks,
        i8& quality_modifier,
        u8& ap_bonus
    ) {
        attacks += mods.extra_attacks;
        quality_modifier += mods.hit_modifier;
        ap_bonus += static_cast<u8>(std::max(0, static_cast<int>(mods.ap_modifier)));
    }

    // Apply defense modifiers
    void apply_to_defense(
        const FactionCombatModifiers& mods,
        i8& defense_modifier
    ) {
        defense_modifier += mods.defense_modifier;
    }

    // Check if unit has a faction rule by name
    bool unit_has_faction_rule(const Unit& unit, std::string_view rule_name) const {
        const auto* faction_rules = get_faction_registry().get_faction(unit.faction.view());
        if (!faction_rules) return false;
        return faction_rules->find_rule(rule_name) != nullptr;
    }

private:
    void apply_army_wide_rules(
        const FactionArmyRules& faction_rules,
        const Unit& attacker,
        const Unit& defender,
        CombatPhase phase,
        bool is_charging,
        FactionCombatModifiers& mods
    ) {
        for (u8 i = 0; i < faction_rules.army_wide_count; ++i) {
            const auto& rule = faction_rules.army_wide_rules[i];

            for (u8 e = 0; e < rule.effect_count; ++e) {
                const auto& effect = rule.effects[e];

                // Check phase restrictions
                if (effect.melee_only && phase != CombatPhase::Melee) continue;
                if (effect.shooting_only && phase != CombatPhase::Shooting) continue;

                // Apply modifiers
                mods.hit_modifier += effect.hit_modifier;
                mods.ap_modifier += effect.ap_modifier;
                mods.morale_modifier += effect.morale_modifier;
                mods.extra_attacks += effect.extra_attacks;
                mods.extra_hits += effect.extra_hits;

                if (effect.ignores_regeneration) {
                    mods.ignores_regeneration = true;
                }

                // Grant rule if specified
                if (effect.grants_rule != RuleId::None) {
                    mods.add_rule(effect.grants_rule);
                }
            }
        }
    }

    void apply_unit_special_rules(
        const FactionArmyRules& faction_rules,
        const Unit& attacker,
        const Unit& defender,
        CombatPhase phase,
        bool is_charging,
        FactionCombatModifiers& mods
    ) {
        // Check for specific faction rules the unit might have
        // This maps unit RuleId to faction rule effects

        // Shielded - +1 defense vs non-spell hits
        if (attacker.has_rule(RuleId::Shielded)) {
            // Applied to defense, not attack
        }

        // Rupture - ignore regen, extra wound on 6
        if (attacker.has_rule(RuleId::Rupture)) {
            mods.ignores_regeneration = true;
        }

        // Predator Fighter - 6s in melee generate extra attacks
        if (attacker.has_rule(RuleId::PredatorFighter) && phase == CombatPhase::Melee) {
            // This is handled in dice rolling, flag it
            mods.add_rule(RuleId::PredatorFighter);
        }

        // Good Shot - +1 to hit when shooting
        if (attacker.has_rule(RuleId::GoodShot) && phase == CombatPhase::Shooting) {
            mods.hit_modifier += 1;
        }

        // Bad Shot - -1 to hit when shooting
        if (attacker.has_rule(RuleId::BadShot) && phase == CombatPhase::Shooting) {
            mods.hit_modifier -= 1;
        }

        // Versatile Attack - Choose AP+1 or +1 to hit (default to AP for simplicity)
        if (attacker.has_rule(RuleId::VersatileAttack)) {
            // Could be randomized or based on defender stats
            // For now, choose based on defender defense
            if (defender.get_base_defense() >= 4) {
                mods.ap_modifier += 1;
            } else {
                mods.hit_modifier += 1;
            }
        }

        // Battleborn - 4+ to stop being shaken (handled in morale phase)
        // MoraleBoost - +1 to morale tests
        if (attacker.has_rule(RuleId::MoraleBoost)) {
            mods.morale_modifier += 1;
        }
    }

    void apply_aura_rules(
        const FactionArmyRules& faction_rules,
        const Unit& attacker,
        const Unit& defender,
        CombatPhase phase,
        bool is_charging,
        FactionCombatModifiers& mods
    ) {
        // Check for aura rules that affect the unit
        for (u8 i = 0; i < faction_rules.aura_count; ++i) {
            const auto& rule = faction_rules.aura_rules[i];

            // Only apply auras with effects that target self/unit
            for (u8 e = 0; e < rule.effect_count; ++e) {
                const auto& effect = rule.effects[e];

                if (effect.target != TargetType::Self &&
                    effect.target != TargetType::Unit) {
                    continue;
                }

                // Check phase restrictions
                if (effect.melee_only && phase != CombatPhase::Melee) continue;
                if (effect.shooting_only && phase != CombatPhase::Shooting) continue;

                // Apply modifiers from aura
                mods.hit_modifier += effect.hit_modifier;
                mods.ap_modifier += effect.ap_modifier;
                mods.defense_modifier += effect.defense_modifier;

                if (effect.grants_rule != RuleId::None) {
                    mods.add_rule(effect.grants_rule);
                }
            }
        }
    }

    void apply_defensive_effect(
        const FactionRule& rule,
        const Unit& defender,
        FactionCombatModifiers& mods
    ) {
        for (u8 e = 0; e < rule.effect_count; ++e) {
            const auto& effect = rule.effects[e];

            if (effect.target == TargetType::Self ||
                effect.target == TargetType::Unit) {

                mods.defense_modifier += effect.defense_modifier;
                mods.morale_modifier += effect.morale_modifier;

                if (effect.grants_rule != RuleId::None) {
                    mods.add_rule(effect.grants_rule);
                }
            }
        }
    }
};

// ==============================================================================
// Enhanced Combat Resolution with Faction Rules
// ==============================================================================

// Apply Shielded defense bonus (+1 to defense vs non-spell hits)
inline i8 apply_shielded(const Unit& defender, bool is_spell_damage = false) {
    if (!is_spell_damage && defender.has_rule(RuleId::Shielded)) {
        return 1;
    }
    return 0;
}

// Apply Resistance (6+ to ignore wounds, 2+ vs spells)
inline u16 apply_resistance(
    DiceRoller& dice,
    u16 wounds,
    const Unit& defender,
    bool is_spell_damage = false
) {
    if (!defender.has_rule(RuleId::Resistance)) {
        return wounds;
    }

    if (wounds == 0) return 0;

    // 6+ normally, 2+ vs spells
    u8 target = is_spell_damage ? 2 : 6;

    u32 ignored = 0;
    for (u16 i = 0; i < wounds; ++i) {
        if (dice.roll_d6() >= target) {
            ++ignored;
        }
    }

    return wounds - static_cast<u16>(ignored);
}

// Apply NoRetreat (take wounds instead of being shaken/routed)
inline bool apply_no_retreat(
    DiceRoller& dice,
    Unit& unit,
    u16 wounds_for_morale
) {
    if (!unit.has_rule(RuleId::NoRetreat)) {
        return false;
    }

    // Instead of shaken/routed, roll dice equal to wounds
    // On 1-3, take a wound
    u16 wounds_taken = 0;
    for (u16 i = 0; i < wounds_for_morale; ++i) {
        u8 roll = dice.roll_d6();
        if (roll <= 3) {
            ++wounds_taken;
        }
    }

    // Apply wounds to the unit
    // TODO: Implement wound allocation for NoRetreat

    return true; // Morale test auto-passed
}

// Apply Battleborn (4+ to stop being shaken at round start)
inline void apply_battleborn(DiceRoller& dice, Unit& unit) {
    if (unit.is_shaken() && unit.has_rule(RuleId::Battleborn)) {
        if (dice.roll_d6() >= 4) {
            unit.rally();
        }
    }
}

// Calculate extra hits from Rupture (extra wound on unmodified 6 to hit)
inline u16 apply_rupture_extra_wounds(u8 sixes_rolled, const Unit& attacker) {
    if (attacker.has_rule(RuleId::Rupture)) {
        return sixes_rolled; // Each 6 to hit deals 1 extra wound
    }
    return 0;
}

// Calculate extra attacks from Predator Fighter (6s generate more attacks)
inline u8 apply_predator_fighter(
    DiceRoller& dice,
    u8 sixes_rolled,
    const Unit& attacker,
    u8 weapon_attacks,
    u8 quality
) {
    if (!attacker.has_rule(RuleId::PredatorFighter) || sixes_rolled == 0) {
        return 0;
    }

    // Each 6 generates a new attack that can also generate more
    u8 extra_hits = 0;
    u8 attacks_to_make = sixes_rolled;

    // Limit recursion to prevent infinite loops
    u8 max_iterations = 10;
    while (attacks_to_make > 0 && max_iterations > 0) {
        auto [hits, new_sixes] = dice.roll_quality_test(attacks_to_make, quality);
        extra_hits += static_cast<u8>(hits);
        attacks_to_make = static_cast<u8>(new_sixes);
        --max_iterations;
    }

    return extra_hits;
}

// Apply Shred (extra wound on unmodified 1 to block)
inline u16 apply_shred_extra_wounds(DiceRoller& dice, u16 original_wounds, u8 ones_rolled) {
    // Each 1 to defend deals an extra wound
    return original_wounds + ones_rolled;
}

// Get faction rule applicator singleton
inline FactionRuleApplicator& get_faction_applicator() {
    static FactionRuleApplicator applicator;
    return applicator;
}

} // namespace battle
