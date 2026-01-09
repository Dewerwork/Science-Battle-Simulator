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
// Hit Separation Result - Result of Rending/Rupture processing
// ==============================================================================

struct HitSeparationResult {
    u32 normal_hits = 0;         // Hits without special effects
    u32 rending_hits = 0;        // Hits with AP+4 (from natural 6s)
    u32 rupture_hits = 0;        // Hits that bypass regen and deal +1 wound
    bool has_rending = false;    // Whether Rending was applied
    bool has_rupture = false;    // Whether Rupture was applied
};

// ==============================================================================
// Hit Bonuses Result - Result of extra hit generation
// ==============================================================================

struct HitBonusesResult {
    u32 bonus_hits = 0;          // Extra hits generated (Relentless, Surge, etc.)
    u32 total_hits = 0;          // Total hits after bonuses
};

// ==============================================================================
// Hit Multiplication Result - Result of Blast processing
// ==============================================================================

struct HitMultiplicationResult {
    u32 total_hits = 0;          // Hits after multiplication
    u32 rending_hits = 0;        // Rending hits after multiplication
    u32 rupture_hits = 0;        // Rupture hits after multiplication
    u8 multiplier_used = 1;      // The multiplier that was applied
};

// ==============================================================================
// Combat Result - Final result of unified combat resolution (Phase 5)
// ==============================================================================

struct CombatResult {
    // Hits
    u32 attacks = 0;             // Total attacks made
    u32 hits = 0;                // Total hits scored
    u32 natural_sixes = 0;       // Natural 6s rolled (for bonus effects)
    u32 rending_hits = 0;        // Hits with AP+4
    u32 rupture_hits = 0;        // Hits that bypass regen

    // Wounds
    u32 wounds_dealt = 0;        // Wounds inflicted on defender
    u32 models_killed = 0;       // Models removed from defender

    // Counter-damage (from SelfDestruct, etc.)
    u32 attacker_wounds = 0;     // Wounds taken by attacker
    u32 attacker_models_killed = 0;

    // Modifiers used
    i8 hit_modifier = 0;         // Hit modifier applied
    u8 quality_used = 4;         // Quality value used
    u8 ap_used = 0;              // AP value used
    u8 blast_multiplier = 1;     // Blast multiplier used

    // Trait flags
    bool bypassed_regeneration = false;
    bool bypassed_resistance = false;
    bool forced_defense_reroll = false;
};

// ==============================================================================
// Unified Combat Context - Full context for combat resolution (Phase 5)
// ==============================================================================

struct UnifiedCombatContext {
    // Participants
    Unit* attacker = nullptr;
    Unit* defender = nullptr;
    Weapon* weapon = nullptr;

    // Combat parameters
    CombatType combat_type = CombatType::SHOOTING;
    u8 distance = 0;
    bool is_charge = false;

    // Hit phase state
    i8 hit_modifier = 0;
    u8 quality_used = 4;
    u32 attacks = 0;
    u32 hits = 0;
    u32 natural_sixes = 0;

    // Hit separation state
    u32 normal_hits = 0;
    u32 rending_hits = 0;
    u32 rupture_hits = 0;

    // Bonus hits state
    u32 bonus_hits = 0;

    // Defense phase state
    u8 base_ap = 0;
    u8 effective_ap = 0;
    u32 wounds = 0;
    u32 models_killed = 0;

    // Trait flags (aggregated from all applicable rules)
    bool bypasses_regeneration = false;
    bool bypasses_resistance = false;
    bool forces_defense_reroll = false;  // Poison

    // Counter-damage tracking
    u32 attacker_wounds = 0;
    u32 attacker_models_killed = 0;

    // Takedown targeting
    bool has_takedown = false;
    i8 takedown_target_idx = -1;
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
    // HIT_SEPARATION Phase (Phase 4)
    // ==============================================================================

    // Apply hit separation rules (Rending, Rupture)
    // Separates natural 6s for special treatment
    HitSeparationResult apply_hit_separation(
        const Unit& attacker,
        const Unit& defender,
        const Weapon& weapon,
        CombatType combat_type,
        u32 total_hits,
        u32 natural_sixes
    ) {
        HitSeparationResult result;
        result.normal_hits = total_hits;

        // Check for Rending
        if (weapon.has_rule(RuleId::Rending)) {
            result.has_rending = true;
            result.rending_hits = natural_sixes;
            result.normal_hits = total_hits - natural_sixes;
        }

        // Check for Rupture (can stack with Rending)
        if (weapon.has_rule(RuleId::Rupture)) {
            result.has_rupture = true;
            result.rupture_hits = natural_sixes;
        }

        return result;
    }

    // ==============================================================================
    // HIT_BONUSES Phase (Phase 4)
    // ==============================================================================

    // Apply hit bonus rules (Relentless, Surge, PointBlankSurge, Furious)
    // Generates extra hits from natural 6s
    HitBonusesResult apply_hit_bonuses(
        const Unit& attacker,
        const Unit& defender,
        const Weapon& weapon,
        CombatType combat_type,
        u8 distance,
        bool is_charge,
        u32 current_hits,
        u32 natural_sixes,
        u8 quality,
        i8 hit_modifier
    ) {
        HitBonusesResult result;
        result.total_hits = current_hits;

        // Build context for condition checks
        CombatContextCore ctx;
        ctx.attacker = const_cast<Unit*>(&attacker);
        ctx.defender = const_cast<Unit*>(&defender);
        ctx.weapon = const_cast<Weapon*>(&weapon);
        ctx.combat_type = combat_type;
        ctx.distance = distance;
        ctx.is_charge = is_charge;
        ctx.natural_sixes = natural_sixes;

        // Relentless: extra hits on 6s when shooting >9"
        if (attacker.has_rule(RuleId::Relentless) &&
            combat_type == CombatType::SHOOTING && distance > 9) {
            result.bonus_hits += natural_sixes;
        }

        // Surge: extra hits on 6s (weapon rule)
        if (weapon.has_rule(RuleId::Surge)) {
            result.bonus_hits += natural_sixes;
        }

        // PointBlankSurge: extra hits on 6s at close range
        if (attacker.has_rule(RuleId::PointBlankSurge) &&
            combat_type == CombatType::SHOOTING && distance <= 9) {
            result.bonus_hits += natural_sixes;
        }

        // Furious: extra hits on 6s when charging (melee only)
        if (attacker.has_rule(RuleId::Furious) &&
            combat_type == CombatType::MELEE && is_charge) {
            result.bonus_hits += natural_sixes;
        }

        // PredatorFighter: recursive extra attacks on 6s (melee only)
        if (attacker.has_rule(RuleId::PredatorFighter) &&
            combat_type == CombatType::MELEE && natural_sixes > 0) {
            u32 total_bonus = 0;
            u32 current_sixes = natural_sixes;

            while (current_sixes > 0) {
                auto bonus_result = dice_.roll_quality_test(current_sixes, quality, hit_modifier);
                total_bonus += bonus_result.hits;
                current_sixes = bonus_result.sixes;
            }
            result.bonus_hits += total_bonus;
        }

        result.total_hits = current_hits + result.bonus_hits;
        return result;
    }

    // ==============================================================================
    // HIT_MULTIPLICATION Phase (Phase 4)
    // ==============================================================================

    // Apply hit multiplication rules (Blast)
    HitMultiplicationResult apply_hit_multiplication(
        const Unit& defender,
        const Weapon& weapon,
        u32 total_hits,
        u32 rending_hits,
        u32 rupture_hits,
        bool has_takedown
    ) {
        HitMultiplicationResult result;
        result.total_hits = total_hits;
        result.rending_hits = rending_hits;
        result.rupture_hits = rupture_hits;
        result.multiplier_used = 1;

        // Blast: multiply hits by value (capped at defender model count)
        u8 blast_value = weapon.get_rule_value(RuleId::Blast);
        if (blast_value > 0) {
            u8 max_multiplier = has_takedown ? u8(1) : defender.alive_count;
            u8 multiplier = std::min(blast_value, max_multiplier);

            result.total_hits *= multiplier;
            result.rending_hits *= multiplier;
            result.rupture_hits *= multiplier;
            result.multiplier_used = multiplier;
        }

        return result;
    }

    // ==============================================================================
    // UNIFIED COMBAT RESOLUTION (Phase 5)
    // ==============================================================================

    // Resolve combat using unified path - works for both shooting and melee
    CombatResult resolve_combat(
        Unit& attacker,
        Unit& defender,
        const Weapon& weapon,
        CombatType combat_type,
        u8 distance,
        bool is_charge
    ) {
        CombatResult result;

        // Initialize context
        UnifiedCombatContext ctx;
        ctx.attacker = &attacker;
        ctx.defender = &defender;
        ctx.weapon = const_cast<Weapon*>(&weapon);
        ctx.combat_type = combat_type;
        ctx.distance = distance;
        ctx.is_charge = is_charge;
        ctx.quality_used = attacker.quality;
        ctx.base_ap = weapon.ap;

        // Check for Takedown
        ctx.has_takedown = weapon.has_rule(RuleId::Takedown);

        // Phase 1: HIT_MODIFIERS
        execute_hit_modifiers(ctx);

        // Phase 2: ROLL_HITS
        execute_roll_hits(ctx);

        // Phase 3: HIT_SEPARATION
        execute_hit_separation(ctx);

        // Phase 4: HIT_BONUSES
        execute_hit_bonuses(ctx);

        // Phase 5: HIT_MULTIPLICATION
        execute_hit_multiplication(ctx);

        // Phase 6: DEFENSE_RESOLUTION
        execute_defense_resolution(ctx);

        // Phase 7: WOUND_ALLOCATION
        execute_wound_allocation(ctx);

        // Build result
        result.attacks = ctx.attacks;
        result.hits = ctx.hits + ctx.bonus_hits;
        result.natural_sixes = ctx.natural_sixes;
        result.rending_hits = ctx.rending_hits;
        result.rupture_hits = ctx.rupture_hits;
        result.wounds_dealt = ctx.wounds;
        result.models_killed = ctx.models_killed;
        result.attacker_wounds = ctx.attacker_wounds;
        result.attacker_models_killed = ctx.attacker_models_killed;
        result.hit_modifier = ctx.hit_modifier;
        result.quality_used = ctx.quality_used;
        result.ap_used = ctx.effective_ap;
        result.bypassed_regeneration = ctx.bypasses_regeneration;
        result.bypassed_resistance = ctx.bypasses_resistance;
        result.forced_defense_reroll = ctx.forces_defense_reroll;

        return result;
    }

private:
    // ==============================================================================
    // Phase Execution Methods
    // ==============================================================================

    void execute_hit_modifiers(UnifiedCombatContext& ctx) {
        auto mod_result = apply_hit_modifiers(
            *ctx.attacker, *ctx.defender, *ctx.weapon,
            ctx.combat_type, ctx.distance, ctx.is_charge);

        ctx.hit_modifier = mod_result.hit_modifier;
        if (mod_result.quality_override > 0) {
            ctx.quality_used = mod_result.quality_override;
        }
    }

    void execute_roll_hits(UnifiedCombatContext& ctx) {
        // Calculate total attacks
        ctx.attacks = ctx.weapon->attacks * ctx.attacker->alive_count;

        // Roll to hit
        auto roll_result = dice_.roll_quality_test(
            ctx.attacks, ctx.quality_used, ctx.hit_modifier);

        ctx.hits = roll_result.hits;
        ctx.natural_sixes = roll_result.sixes;
        ctx.normal_hits = ctx.hits;
    }

    void execute_hit_separation(UnifiedCombatContext& ctx) {
        auto sep_result = apply_hit_separation(
            *ctx.attacker, *ctx.defender, *ctx.weapon,
            ctx.combat_type, ctx.hits, ctx.natural_sixes);

        ctx.normal_hits = sep_result.normal_hits;
        ctx.rending_hits = sep_result.rending_hits;
        ctx.rupture_hits = sep_result.rupture_hits;

        // Aggregate traits for later phases
        if (sep_result.has_rending || sep_result.has_rupture) {
            ctx.bypasses_regeneration = true;
        }
    }

    void execute_hit_bonuses(UnifiedCombatContext& ctx) {
        auto bonus_result = apply_hit_bonuses(
            *ctx.attacker, *ctx.defender, *ctx.weapon,
            ctx.combat_type, ctx.distance, ctx.is_charge,
            ctx.hits, ctx.natural_sixes, ctx.quality_used, ctx.hit_modifier);

        ctx.bonus_hits = bonus_result.bonus_hits;
        ctx.hits = bonus_result.total_hits;
    }

    void execute_hit_multiplication(UnifiedCombatContext& ctx) {
        auto mult_result = apply_hit_multiplication(
            *ctx.defender, *ctx.weapon,
            ctx.hits, ctx.rending_hits, ctx.rupture_hits, ctx.has_takedown);

        ctx.hits = mult_result.total_hits;
        ctx.rending_hits = mult_result.rending_hits;
        ctx.rupture_hits = mult_result.rupture_hits;
    }

    void execute_defense_resolution(UnifiedCombatContext& ctx) {
        // Aggregate traits from all applicable rules
        aggregate_combat_traits(ctx);

        // Calculate effective AP
        ctx.effective_ap = ctx.base_ap;

        // Lance: +2 AP when charging
        if (ctx.is_charge && ctx.weapon->has_rule(RuleId::Lance)) {
            ctx.effective_ap += 2;
        }

        // Thrust: +1 AP when charging (in addition to hit bonus)
        if (ctx.is_charge && ctx.weapon->has_rule(RuleId::Thrust)) {
            ctx.effective_ap += 1;
        }

        // Process normal hits
        u32 normal_wounds = 0;
        if (ctx.normal_hits > 0) {
            normal_wounds = dice_.roll_defense_test(
                ctx.normal_hits, ctx.defender->defense, ctx.effective_ap,
                0, ctx.forces_defense_reroll);
        }

        // Process rending hits (AP+4)
        u32 rending_wounds = 0;
        if (ctx.rending_hits > 0) {
            u8 rending_ap = ctx.effective_ap + 4;
            rending_wounds = dice_.roll_defense_test(
                ctx.rending_hits, ctx.defender->defense, rending_ap,
                0, ctx.forces_defense_reroll);
        }

        ctx.wounds = normal_wounds + rending_wounds;

        // Rupture adds +1 wound per wound from rupture hits
        // (simplified - in full implementation would track which wounds came from rupture)
    }

    void execute_wound_allocation(UnifiedCombatContext& ctx) {
        if (ctx.wounds == 0) return;

        u32 wounds_remaining = ctx.wounds;
        u32 models_killed = 0;

        // Get wound allocation order
        std::array<u8, MAX_MODELS_PER_UNIT> order;
        u8 order_count;
        ctx.defender->get_wound_allocation_order(order, order_count);

        // Apply Deadly multiplier
        u8 deadly_value = ctx.weapon->get_rule_value(RuleId::Deadly);
        if (deadly_value > 1) {
            wounds_remaining *= deadly_value;
        }

        // Allocate wounds to models
        for (u8 i = 0; i < order_count && wounds_remaining > 0; ++i) {
            Model& model = ctx.defender->models[order[i]];
            if (!model.is_alive()) continue;

            u32 wounds_to_apply = wounds_remaining;

            // Check regeneration (if not bypassed)
            if (!ctx.bypasses_regeneration && ctx.defender->has_rule(RuleId::Regeneration)) {
                u8 regen_target = ctx.defender->get_rule_value(RuleId::Regeneration);
                if (regen_target == 0) regen_target = 5;  // Default 5+
                wounds_to_apply = dice_.roll_regeneration(wounds_to_apply, regen_target);
            }

            // Apply wounds to model
            u32 model_wounds = std::min(wounds_to_apply,
                static_cast<u32>(model.remaining_wounds()));

            model.wounds_taken += static_cast<u8>(model_wounds);
            wounds_remaining -= model_wounds;

            if (!model.is_alive()) {
                models_killed++;
            }
        }

        ctx.models_killed = models_killed;
        ctx.defender->update_alive_count();
    }

    void aggregate_combat_traits(UnifiedCombatContext& ctx) {
        // Check weapon traits
        if (ctx.weapon->has_rule(RuleId::Poison)) {
            ctx.forces_defense_reroll = true;
        }
        if (ctx.weapon->has_rule(RuleId::Bane)) {
            ctx.bypasses_regeneration = true;
        }
        if (ctx.weapon->has_rule(RuleId::Rending) ||
            ctx.weapon->has_rule(RuleId::Rupture)) {
            ctx.bypasses_regeneration = true;
        }

        // Check attacker traits
        // (none currently affect defense resolution)
    }

public:
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
// A/B Test Helper Functions - DEPRECATED
// ==============================================================================
// These functions were used during migration (Phases 3-5) to validate that
// the registry-based implementation produces identical results to the old
// hardcoded implementation. Migration is now complete (Phase 8).
//
// These functions are kept for reference and potential debugging but should
// not be used in production code.
// ==============================================================================

// Compute old-style hit modifier (for comparison)
// DEPRECATED: Use apply_hit_modifiers() instead
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
// A/B Test Helper Functions for Phase 4 Sub-Phases
// ==============================================================================

// Compute old-style hit separation (for comparison)
inline HitSeparationResult compute_hit_separation_old_style(
    const Unit& attacker,
    const Unit& defender,
    const Weapon& weapon,
    u32 total_hits,
    u32 natural_sixes
) {
    HitSeparationResult result;
    result.normal_hits = total_hits;

    if (weapon.has_rule(RuleId::Rending)) {
        result.has_rending = true;
        result.rending_hits = natural_sixes;
        result.normal_hits = total_hits - natural_sixes;
    }

    if (weapon.has_rule(RuleId::Rupture)) {
        result.has_rupture = true;
        result.rupture_hits = natural_sixes;
    }

    return result;
}

// Compute old-style hit bonuses (for comparison)
inline HitBonusesResult compute_hit_bonuses_old_style(
    const Unit& attacker,
    const Unit& defender,
    const Weapon& weapon,
    CombatType combat_type,
    u8 distance,
    bool is_charge,
    u32 current_hits,
    u32 natural_sixes,
    u8 quality,
    i8 hit_modifier,
    DiceRoller& dice
) {
    HitBonusesResult result;
    result.total_hits = current_hits;

    // Relentless: extra hits on 6s when shooting >9"
    if (attacker.has_rule(RuleId::Relentless) &&
        combat_type == CombatType::SHOOTING && distance > 9) {
        result.bonus_hits += natural_sixes;
    }

    // Surge: extra hits on 6s (weapon rule)
    if (weapon.has_rule(RuleId::Surge)) {
        result.bonus_hits += natural_sixes;
    }

    // PointBlankSurge: extra hits on 6s at close range
    if (attacker.has_rule(RuleId::PointBlankSurge) &&
        combat_type == CombatType::SHOOTING && distance <= 9) {
        result.bonus_hits += natural_sixes;
    }

    // Furious: extra hits on 6s when charging (melee only)
    if (attacker.has_rule(RuleId::Furious) &&
        combat_type == CombatType::MELEE && is_charge) {
        result.bonus_hits += natural_sixes;
    }

    // PredatorFighter: recursive extra attacks on 6s (melee only)
    if (attacker.has_rule(RuleId::PredatorFighter) &&
        combat_type == CombatType::MELEE && natural_sixes > 0) {
        u32 total_bonus = 0;
        u32 current_sixes = natural_sixes;

        while (current_sixes > 0) {
            auto bonus_result = dice.roll_quality_test(current_sixes, quality, hit_modifier);
            total_bonus += bonus_result.hits;
            current_sixes = bonus_result.sixes;
        }
        result.bonus_hits += total_bonus;
    }

    result.total_hits = current_hits + result.bonus_hits;
    return result;
}

// Compute old-style hit multiplication (for comparison)
inline HitMultiplicationResult compute_hit_multiplication_old_style(
    const Unit& defender,
    const Weapon& weapon,
    u32 total_hits,
    u32 rending_hits,
    u32 rupture_hits,
    bool has_takedown
) {
    HitMultiplicationResult result;
    result.total_hits = total_hits;
    result.rending_hits = rending_hits;
    result.rupture_hits = rupture_hits;
    result.multiplier_used = 1;

    u8 blast_value = weapon.get_rule_value(RuleId::Blast);
    if (blast_value > 0) {
        u8 max_multiplier = has_takedown ? u8(1) : defender.alive_count;
        u8 multiplier = std::min(blast_value, max_multiplier);

        result.total_hits *= multiplier;
        result.rending_hits *= multiplier;
        result.rupture_hits *= multiplier;
        result.multiplier_used = multiplier;
    }

    return result;
}

// ==============================================================================
// A/B Test Mode Flag - DEPRECATED
// ==============================================================================
// A/B testing was used during migration (Phases 3-5) to validate correctness.
// Migration is now complete (Phase 8). These flags are disabled by default.
// Enable only if debugging migration-related issues.

inline constexpr bool AB_TEST_HIT_MODIFIERS = false;
inline constexpr bool AB_TEST_HIT_SEPARATION = false;
inline constexpr bool AB_TEST_HIT_BONUSES = false;
inline constexpr bool AB_TEST_HIT_MULTIPLICATION = false;

} // namespace battle
