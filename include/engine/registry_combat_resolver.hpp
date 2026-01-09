#pragma once

// ==============================================================================
// Registry-Based Combat Resolver (Phase 3)
// ==============================================================================
// This implements combat resolution using the RuleRegistry instead of hardcoded
// conditionals. It provides the same functionality as combat_engine.hpp but
// uses the new registry-based approach.
//
// A/B Testing: This resolver can be run in parallel with the old resolver
// to verify they produce identical results.

#include "core/types.hpp"
#include "core/unit.hpp"
#include "core/weapon.hpp"
#include "core/phases.hpp"
#include "core/contexts.hpp"
#include "core/rule_registry.hpp"
#include "engine/dice.hpp"

// Forward declare MatchLogger to avoid header dependency issues
namespace battle {
class MatchLogger;
}

namespace battle {

// ==============================================================================
// Hit Modifier Result - Result of applying all hit modifiers
// ==============================================================================

struct HitModifierResult {
    i8 hit_modifier = 0;         // Total hit modifier
    u8 quality_override = 0;     // Quality override (0 = use default)
    bool skip_hit_roll = false;  // Skip hit roll entirely (e.g., auto-hit)
};

// ==============================================================================
// Registry Combat Resolver
// ==============================================================================

class RegistryCombatResolver {
public:
    explicit RegistryCombatResolver(
        const RuleRegistry& registry,
        DiceRoller& dice,
        MatchLogger* logger = nullptr
    ) : registry_(registry), dice_(dice), logger_(logger) {}

    // ==============================================================================
    // Hit Modifier Phase (Phase 3 Focus)
    // ==============================================================================

    // Apply all hit modifiers for a weapon attack
    // This is the registry-based equivalent of the hardcoded hit modifier logic
    HitModifierResult apply_hit_modifiers(
        const Unit& attacker,
        const Unit& defender,
        const Weapon& weapon,
        CombatType combat_type,
        u8 distance,
        bool is_charge
    ) {
        HitModifierResult result;

        // Build combat context for condition checking
        CombatContextCore ctx;
        ctx.attacker = const_cast<Unit*>(&attacker);
        ctx.defender = const_cast<Unit*>(&defender);
        ctx.weapon = const_cast<Weapon*>(&weapon);
        ctx.combat_type = combat_type;
        ctx.distance = distance;
        ctx.is_charge = is_charge;
        ctx.hit_modifier = 0;
        ctx.quality_used = attacker.quality;

        // Collect applicable rules for HIT_MODIFIERS phase
        auto rules = registry_.collect_combat_rules(
            CombatSubPhase::HIT_MODIFIERS,
            combat_type,
            attacker,
            defender,
            weapon
        );

        // Apply each rule's effect
        for (const auto& rule : rules) {
            const auto& effects = registry_.get_effects(rule.id);
            auto effect_fn = effects.combat_effects[static_cast<size_t>(CombatSubPhase::HIT_MODIFIERS)];
            auto condition_fn = effects.conditions[static_cast<size_t>(CombatSubPhase::HIT_MODIFIERS)];

            // Check condition if one exists
            if (condition_fn && !condition_fn(ctx)) {
                continue;  // Condition not met, skip this rule
            }

            // Apply effect
            if (effect_fn) {
                effect_fn(ctx, nullptr, rule.value);

                // Update result
                result.hit_modifier = ctx.hit_modifier;
            }
        }

        // Also check for ROLL_HITS phase quality overrides (like Reliable)
        auto roll_rules = registry_.collect_combat_rules(
            CombatSubPhase::ROLL_HITS,
            combat_type,
            attacker,
            defender,
            weapon
        );

        CombatContextExtended ext;
        for (const auto& rule : roll_rules) {
            const auto& effects = registry_.get_effects(rule.id);
            auto effect_fn = effects.combat_effects[static_cast<size_t>(CombatSubPhase::ROLL_HITS)];

            if (effect_fn) {
                effect_fn(ctx, &ext, rule.value);

                if (ext.quality_override.has_value()) {
                    result.quality_override = ext.quality_override.value();
                }
            }
        }

        return result;
    }

    // ==============================================================================
    // Comparison Helper - For A/B Testing
    // ==============================================================================

    // Compare registry-based result with expected values from old implementation
    struct ComparisonResult {
        bool match = true;
        i8 expected_modifier = 0;
        i8 actual_modifier = 0;
        u8 expected_quality = 0;
        u8 actual_quality = 0;
        const char* mismatch_reason = nullptr;
    };

    ComparisonResult compare_hit_modifiers(
        const HitModifierResult& registry_result,
        i8 old_hit_modifier,
        u8 old_quality
    ) {
        ComparisonResult cmp;
        cmp.expected_modifier = old_hit_modifier;
        cmp.actual_modifier = registry_result.hit_modifier;
        cmp.expected_quality = old_quality;
        cmp.actual_quality = registry_result.quality_override > 0 ?
                             registry_result.quality_override : old_quality;

        if (cmp.expected_modifier != cmp.actual_modifier) {
            cmp.match = false;
            cmp.mismatch_reason = "hit_modifier_mismatch";
        } else if (cmp.expected_quality != cmp.actual_quality) {
            cmp.match = false;
            cmp.mismatch_reason = "quality_mismatch";
        }

        return cmp;
    }

private:
    const RuleRegistry& registry_;
    DiceRoller& dice_;
    MatchLogger* logger_;
};

// ==============================================================================
// A/B Test Helper Functions
// ==============================================================================

// Compute old-style hit modifier (for comparison)
// This replicates the logic from combat_engine.hpp for A/B testing
inline i8 compute_hit_modifier_old_style(
    const Unit& attacker,
    const Unit& defender,
    const Weapon& weapon,
    CombatType combat_type,
    u8 distance,
    bool is_charge
) {
    i8 hit_modifier = 0;

    if (combat_type == CombatType::SHOOTING) {
        // Stealth: -1 to hit from >9"
        if (defender.has_rule(RuleId::Stealth) && distance > 9) {
            hit_modifier -= 1;
        }

        // RangedShrouding: -1 to be hit when shot
        if (defender.has_rule(RuleId::RangedShrouding)) {
            hit_modifier -= 1;
        }

        // Precise: +1 to hit (weapon rule)
        if (weapon.has_rule(RuleId::Precise)) {
            hit_modifier += 1;
        }

        // GoodShot: +1 to hit when shooting (unit rule)
        if (attacker.has_rule(RuleId::GoodShot)) {
            hit_modifier += 1;
        }

        // BadShot: -1 to hit when shooting (unit rule)
        if (attacker.has_rule(RuleId::BadShot)) {
            hit_modifier -= 1;
        }

        // Purge: +1 to hit vs Tough(3+)
        u8 defender_tough = defender.get_rule_value(RuleId::Tough);
        if (weapon.has_rule(RuleId::Purge) && defender_tough >= 3) {
            hit_modifier += 1;
        }
    } else {
        // Melee
        // Thrust: +1 to hit when charging
        if (is_charge && weapon.has_rule(RuleId::Thrust)) {
            hit_modifier += 1;
        }

        // Precise: +1 to hit (weapon rule)
        if (weapon.has_rule(RuleId::Precise)) {
            hit_modifier += 1;
        }

        // MeleeEvasion: -1 to be hit in melee (defender rule)
        if (defender.has_rule(RuleId::MeleeEvasion)) {
            hit_modifier -= 1;
        }

        // MeleeShrouding: -1 to be hit in melee (defender rule)
        if (defender.has_rule(RuleId::MeleeShrouding)) {
            hit_modifier -= 1;
        }

        // Purge: +1 to hit vs Tough(3+)
        u8 defender_tough = defender.get_rule_value(RuleId::Tough);
        if (weapon.has_rule(RuleId::Purge) && defender_tough >= 3) {
            hit_modifier += 1;
        }
    }

    return hit_modifier;
}

// Compute old-style quality (for comparison)
inline u8 compute_quality_old_style(const Unit& attacker, const Weapon& weapon) {
    u8 quality = attacker.quality;

    // Reliable: Quality becomes 2+
    if (weapon.has_rule(RuleId::Reliable)) {
        quality = 2;
    }

    return quality;
}

// ==============================================================================
// A/B Test Mode Flag
// ==============================================================================

// Set to true to enable A/B testing (compares old and new implementations)
// Should be disabled in production for performance
#ifndef NDEBUG
inline constexpr bool AB_TEST_HIT_MODIFIERS = true;
#else
inline constexpr bool AB_TEST_HIT_MODIFIERS = false;
#endif

} // namespace battle
