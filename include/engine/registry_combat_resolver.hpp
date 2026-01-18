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
#include "engine/game_state.hpp"      // For CombatResult, ActionType
#include "engine/match_logger.hpp"    // Requires ActionType from game_state.hpp
#include "simulation/sim_state.hpp"   // For UnitView

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
// Detailed Combat Result - Internal result of unified combat resolution (Phase 5)
// ==============================================================================
// This is a detailed result used internally by the registry-based combat resolver.
// For the game interface, see CombatResult in game_state.hpp.

struct DetailedCombatResult {
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
    u32 self_destruct_hits = 0;  // SelfDestruct hits queued for attacker

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
    i8 defense_modifier = 0;  // From Shielded, etc.
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

    // VersatileAttack state
    bool versatile_ap_chosen = false;  // True = +1 AP, False = +1 hit (already applied)

    // Movement state (for Indirect penalty)
    bool moved_this_activation = false;  // Did unit move before shooting

    // Unstoppable state
    bool ignores_negative_hit_mods = false;  // Ignore negative hit modifiers
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
        // Suppress unused parameter warnings - these may be used for future rules
        (void)attacker;
        (void)defender;
        (void)combat_type;

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
    // Returns DetailedCombatResult with full tracking information.
    // For the simpler game interface, use resolve_shooting() or resolve_melee().
    DetailedCombatResult resolve_combat(
        Unit& attacker,
        Unit& defender,
        const Weapon& weapon,
        CombatType combat_type,
        u8 distance,
        bool is_charge
    ) {
        return resolve_combat_internal(attacker, defender, weapon, combat_type, distance, is_charge);
    }

    // ==============================================================================
    // PHASE-BASED COMBAT RESOLUTION (New Architecture)
    // ==============================================================================
    // This method uses the registry's effect system for ALL rule application.
    // Rules are defined once and apply automatically to both shooting and melee.

    DetailedCombatResult resolve_attack_phased(
        Unit& attacker,
        Unit& defender,
        const Weapon& weapon,
        CombatType combat_type,
        u8 distance,
        bool is_charge,
        u8 models_attacking
    ) {
        DetailedCombatResult result;

        // Initialize unified context
        UnifiedCombatContext ctx;
        ctx.attacker = &attacker;
        ctx.defender = &defender;
        ctx.weapon = const_cast<Weapon*>(&weapon);
        ctx.combat_type = combat_type;
        ctx.distance = distance;
        ctx.is_charge = is_charge;
        ctx.quality_used = attacker.quality;
        ctx.base_ap = weapon.ap;
        ctx.effective_ap = weapon.ap;
        ctx.attacks = static_cast<u32>(models_attacking) * weapon.attacks;

        // ============================================
        // Phase 1: PRE_ATTACK
        // ============================================
        // VersatileAttack choice, Limited check, Impact hits, Takedown targeting
        apply_all_phase_effects(CombatSubPhase::PRE_ATTACK, ctx);

        // ============================================
        // Phase 2: HIT_MODIFIERS
        // ============================================
        // Precise, Stealth, Shrouding, GoodShot, BadShot, etc.
        apply_all_phase_effects(CombatSubPhase::HIT_MODIFIERS, ctx);

        // ============================================
        // Phase 3: ROLL_HITS
        // ============================================
        // Quality overrides (Reliable) then actual dice roll
        apply_all_phase_effects(CombatSubPhase::ROLL_HITS, ctx);

        // Actually roll the dice
        auto hit_result = dice_.roll_quality_test(ctx.attacks, ctx.quality_used, ctx.hit_modifier);
        ctx.hits = hit_result.hits;
        ctx.natural_sixes = hit_result.sixes;

        // ============================================
        // Phase 4: HIT_SEPARATION
        // ============================================
        // Rending, Rupture - separate natural 6s
        apply_all_phase_effects(CombatSubPhase::HIT_SEPARATION, ctx);

        // If no explicit separation, normal hits = total hits
        if (ctx.rending_hits == 0 && ctx.rupture_hits == 0) {
            ctx.normal_hits = ctx.hits;
        } else {
            ctx.normal_hits = ctx.hits - ctx.rending_hits;
        }

        // ============================================
        // Phase 5: HIT_BONUSES
        // ============================================
        // Surge, Relentless, Furious, PredatorFighter - extra hits on 6s
        apply_all_phase_effects(CombatSubPhase::HIT_BONUSES, ctx);
        ctx.hits += ctx.bonus_hits;

        // ============================================
        // Phase 6: HIT_MULTIPLICATION
        // ============================================
        // Blast - multiply hits
        apply_all_phase_effects(CombatSubPhase::HIT_MULTIPLICATION, ctx);

        // ============================================
        // Phase 7: DEFENSE_RESOLUTION
        // ============================================
        // AP modifiers (Lance, Thrust), Shielded, Poison reroll, etc.
        apply_all_phase_effects(CombatSubPhase::DEFENSE_RESOLUTION, ctx);

        // Calculate effective defense target
        u8 defense_target = defender.defense;

        // Apply AP to defense target
        i8 effective_target = static_cast<i8>(defense_target) + static_cast<i8>(ctx.effective_ap);
        effective_target = std::max(i8(2), std::min(i8(6), effective_target));

        // Roll defense for normal hits
        u32 wounds_from_normal = dice_.roll_defense_test(
            ctx.normal_hits, defense_target, ctx.effective_ap, 0, ctx.forces_defense_reroll);

        // Roll defense for rending hits (AP+4)
        u32 wounds_from_rending = 0;
        if (ctx.rending_hits > 0) {
            u8 rending_ap = ctx.effective_ap + 4;
            wounds_from_rending = dice_.roll_defense_test(
                ctx.rending_hits, defense_target, rending_ap, 0, ctx.forces_defense_reroll);
        }

        ctx.wounds = wounds_from_normal + wounds_from_rending;

        // ============================================
        // Phase 8: WOUND_ALLOCATION
        // ============================================
        // Regeneration, Resistance, Tough, Deadly, Hero targeting
        apply_all_phase_effects(CombatSubPhase::WOUND_ALLOCATION, ctx);

        // Build result
        result.attacks = ctx.attacks;
        result.hits = ctx.hits;
        result.natural_sixes = ctx.natural_sixes;
        result.rending_hits = ctx.rending_hits;
        result.rupture_hits = ctx.rupture_hits;
        result.wounds_dealt = ctx.wounds;
        result.models_killed = ctx.models_killed;
        result.hit_modifier = ctx.hit_modifier;
        result.quality_used = ctx.quality_used;
        result.ap_used = ctx.effective_ap;
        result.bypassed_regeneration = ctx.bypasses_regeneration;
        result.bypassed_resistance = ctx.bypasses_resistance;
        result.forced_defense_reroll = ctx.forces_defense_reroll;

        return result;
    }

private:
    // Internal implementation of resolve_combat
    DetailedCombatResult resolve_combat_internal(
        Unit& attacker,
        Unit& defender,
        const Weapon& weapon,
        CombatType combat_type,
        u8 distance,
        bool is_charge
    ) {
        DetailedCombatResult result;

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
    // Generic Phase Effect Application
    // ==============================================================================
    // This method applies all registered effects for a given combat sub-phase.
    // It bridges UnifiedCombatContext with the CombatContextCore used by effects.

    void apply_all_phase_effects(CombatSubPhase phase, UnifiedCombatContext& uctx) {
        // Build CombatContextCore from UnifiedCombatContext
        CombatContextCore ctx;
        ctx.attacker = uctx.attacker;
        ctx.defender = uctx.defender;
        ctx.weapon = uctx.weapon;
        ctx.combat_type = uctx.combat_type;
        ctx.distance = uctx.distance;
        ctx.is_charge = uctx.is_charge;
        ctx.hit_modifier = uctx.hit_modifier;
        ctx.quality_used = uctx.quality_used;
        ctx.defense_modifier = 0;  // Will be set by effects
        ctx.ap_modifier = 0;       // Will be set by effects
        ctx.attacks = uctx.attacks;
        ctx.hits = uctx.hits;
        ctx.natural_sixes = uctx.natural_sixes;

        // Set trait flags
        if (uctx.bypasses_regeneration) ctx.set_bypass_regen(true);
        if (uctx.bypasses_resistance) ctx.set_bypass_resist(true);
        if (uctx.forces_defense_reroll) ctx.set_force_reroll(true);
        if (uctx.has_takedown) ctx.set_has_takedown(true);

        // Extended context for complex rules
        CombatContextExtended ext;
        ext.rending_hits = uctx.rending_hits;
        ext.rupture_hits = uctx.rupture_hits;
        ext.bonus_hits = uctx.bonus_hits;
        ext.logger = logger_;
        ext.dice = &dice_;

        // Collect all rules that apply in this phase
        auto rules = registry_.collect_combat_rules(
            phase,
            uctx.combat_type,
            *uctx.attacker,
            *uctx.defender,
            *uctx.weapon
        );

        // Apply each rule's effect
        for (const auto& rule : rules) {
            const auto& effects = registry_.get_effects(rule.id);

            // Check condition if one exists
            if (!check_combat_condition(effects, phase, ctx)) {
                continue;  // Condition not met, skip this rule
            }

            // Apply effect
            apply_combat_effect(effects, phase, ctx, &ext, rule.value);

            // Log the rule application
            if (logger_) {
                const auto& cold = registry_.get_cold_data(rule.id);
                CombatContextExtended::RuleApplication app;
                app.rule = rule.id;
                app.value = rule.value;
                app.effect_description = cold.description;
                ext.applied_rules.push_back(app);

                // Emit on_rule_triggered for trait-based effects
                const auto& hot = registry_.get_hot_data(rule.id);
                if (hot.has_trait(RuleTrait::FORCES_DEFENSE_REROLL)) {
                    logger_->on_rule_triggered(cold.name, "defense_reroll_6s", 1);
                }
                if (hot.has_trait(RuleTrait::BYPASSES_REGENERATION)) {
                    logger_->on_rule_triggered(cold.name, "bypasses_regeneration", 1);
                }
                if (hot.has_trait(RuleTrait::GENERATES_EXTRA_HITS) && ext.bonus_hits > 0) {
                    logger_->on_rule_triggered(cold.name, "extra_hits", ext.bonus_hits);
                }
            }
        }

        // Copy results back to UnifiedCombatContext
        uctx.hit_modifier = ctx.hit_modifier;
        uctx.quality_used = ctx.quality_used;
        uctx.defense_modifier = ctx.defense_modifier;
        uctx.hits = ctx.hits;
        uctx.natural_sixes = ctx.natural_sixes;

        // Copy trait flags back
        uctx.bypasses_regeneration = ctx.bypasses_regeneration();
        uctx.bypasses_resistance = ctx.bypasses_resistance();
        uctx.forces_defense_reroll = ctx.forces_defense_reroll();
        uctx.has_takedown = ctx.has_takedown();

        // Copy extended context results
        uctx.rending_hits = ext.rending_hits;
        uctx.rupture_hits = ext.rupture_hits;
        uctx.bonus_hits = ext.bonus_hits;
        uctx.versatile_ap_chosen = ext.versatile_ap_chosen;

        // Handle quality override from extended context
        if (ext.quality_override.has_value()) {
            uctx.quality_used = ext.quality_override.value();
        }
    }

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

        // Fragment: +1 AP against Defense 2+ to 4+ (defense values 2, 3, or 4)
        if (ctx.weapon->has_rule(RuleId::Fragment)) {
            u8 def = ctx.defender->defense;
            if (def >= 2 && def <= 4) {
                ctx.effective_ap += 1;
                if (logger_) logger_->on_rule_triggered("Fragment", "ap_bonus_vs_defense", def);
            }
        }

        // Decimate: +2 AP against Defense 2+ to 3+ (defense values 2 or 3)
        if (ctx.weapon->has_rule(RuleId::Decimate)) {
            u8 def = ctx.defender->defense;
            if (def >= 2 && def <= 3) {
                ctx.effective_ap += 2;
                if (logger_) logger_->on_rule_triggered("Decimate", "ap_bonus_vs_defense", def);
            }
        }

        // Disintegrate: +2 AP against Defense 2+ to 3+ (defense values 2 or 3)
        if (ctx.weapon->has_rule(RuleId::Disintegrate)) {
            u8 def = ctx.defender->defense;
            if (def >= 2 && def <= 3) {
                ctx.effective_ap += 2;
                if (logger_) logger_->on_rule_triggered("Disintegrate", "ap_bonus_vs_defense", def);
            }
            // Note: Disintegrate also bypasses regeneration (handled in trait aggregation)
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

    // Helper to get weapon rules as a string for logging
    std::string get_weapon_rules_str(const Weapon& w) const {
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
        if (w.has_rule(RuleId::Rupture)) rules += "Rupture,";
        if (w.has_rule(RuleId::Precise)) rules += "Precise,";
        if (w.has_rule(RuleId::Purge)) rules += "Purge,";
        if (w.has_rule(RuleId::Shred)) rules += "Shred,";
        if (w.has_rule(RuleId::Takedown)) rules += "Takedown,";
        if (w.has_rule(RuleId::Limited)) rules += "Limited,";
        if (!rules.empty()) rules.pop_back();  // Remove trailing comma
        return rules;
    }

    // ==============================================================================
    // Unified Per-Weapon Attack - Uses phase-based effect dispatch
    // ==============================================================================
    // This method resolves a single weapon's attack using the phase system.
    // Rules are applied via registered effects, not manual if-checks.

    struct WeaponAttackResult {
        u32 wounds_dealt = 0;
        u8 models_killed = 0;
        u32 self_destruct_hits = 0;
    };

    WeaponAttackResult resolve_weapon_attack_phased(
        UnitView attacker,
        UnitView defender,
        const Weapon& weapon,
        u8 weapon_index,
        CombatType combat_type,
        u8 distance,
        bool is_charge,
        u8 models_attacking,
        bool moved = false  // Did unit move before attacking (for Indirect penalty)
    ) {
        (void)weapon_index;  // Currently unused, may be used for logging
        WeaponAttackResult result;

        // Build unified context - use const_cast because effects need non-const
        // but we won't actually modify the original units through these pointers
        UnifiedCombatContext ctx;
        ctx.attacker = const_cast<Unit*>(attacker.unit);
        ctx.defender = const_cast<Unit*>(defender.unit);
        ctx.weapon = const_cast<Weapon*>(&weapon);
        ctx.combat_type = combat_type;
        ctx.distance = distance;
        ctx.is_charge = is_charge;
        ctx.moved_this_activation = moved;  // Track movement for Indirect penalty
        ctx.quality_used = attacker.quality();
        ctx.base_ap = weapon.ap;
        ctx.effective_ap = weapon.ap;
        ctx.attacks = static_cast<u32>(models_attacking) * weapon.attacks;

        // Log weapon attack start
        if (logger_) {
            std::string rules_str = get_weapon_rules_str(weapon);
            bool is_melee = (combat_type == CombatType::MELEE);
            logger_->on_weapon_attack_start(weapon.name.c_str(), is_melee,
                is_melee ? 0 : weapon.range, distance, weapon.attacks, weapon.ap, rules_str.c_str());
            logger_->on_attack_count(models_attacking, weapon.attacks, ctx.attacks);
        }

        // ============================================
        // Phase 1: PRE_ATTACK
        // ============================================
        apply_all_phase_effects(CombatSubPhase::PRE_ATTACK, ctx);

        // ============================================
        // Phase 2: HIT_MODIFIERS
        // ============================================
        apply_all_phase_effects(CombatSubPhase::HIT_MODIFIERS, ctx);

        // Indirect: -1 to hit when shooting after moving
        if (combat_type == CombatType::SHOOTING && ctx.moved_this_activation &&
            weapon.has_rule(RuleId::Indirect)) {
            ctx.hit_modifier -= 1;
            if (logger_) logger_->on_hit_modifier("Indirect", -1, "moved_before_shooting");
        }

        // Unstoppable: Ignore all negative modifiers to this weapon
        if (weapon.has_rule(RuleId::Unstoppable)) {
            ctx.ignores_negative_hit_mods = true;
            if (ctx.hit_modifier < 0) {
                if (logger_) logger_->on_rule_triggered("Unstoppable", "ignoring_negative_hit_modifiers", static_cast<u32>(-ctx.hit_modifier));
                ctx.hit_modifier = 0;
            }
        }

        // ============================================
        // Phase 3: ROLL_HITS
        // ============================================
        apply_all_phase_effects(CombatSubPhase::ROLL_HITS, ctx);

        // Roll dice
        if (logger_) dice_.enable_roll_recording(true);
        auto hit_result = dice_.roll_quality_test(ctx.attacks, ctx.quality_used, ctx.hit_modifier);
        ctx.hits = hit_result.hits;
        ctx.natural_sixes = hit_result.sixes;

        // Log hit rolls
        if (logger_) {
            std::vector<u8> hit_rolls = dice_.take_recorded_rolls();
            i8 effective = static_cast<i8>(ctx.quality_used) - ctx.hit_modifier;
            effective = std::max(i8(2), std::min(i8(6), effective));
            logger_->on_hit_rolls(attacker.quality(), ctx.hit_modifier,
                static_cast<u8>(effective), hit_rolls, ctx.hits, ctx.natural_sixes);
        }

        // ============================================
        // Phase 4: HIT_SEPARATION
        // ============================================
        apply_all_phase_effects(CombatSubPhase::HIT_SEPARATION, ctx);
        if (ctx.rending_hits == 0) {
            ctx.normal_hits = ctx.hits;
        } else {
            ctx.normal_hits = ctx.hits - ctx.rending_hits;
        }

        // ============================================
        // Phase 5: HIT_BONUSES
        // ============================================
        apply_all_phase_effects(CombatSubPhase::HIT_BONUSES, ctx);
        ctx.hits += ctx.bonus_hits;

        // ============================================
        // Phase 6: HIT_MULTIPLICATION
        // ============================================
        apply_all_phase_effects(CombatSubPhase::HIT_MULTIPLICATION, ctx);

        // Log hits after modifiers
        if (logger_) {
            logger_->on_hits_after_modifiers(ctx.normal_hits, ctx.rending_hits, ctx.hits);
        }

        // ============================================
        // Phase 7: DEFENSE_RESOLUTION
        // ============================================
        apply_all_phase_effects(CombatSubPhase::DEFENSE_RESOLUTION, ctx);

        // Apply VersatileAttack AP bonus if chosen
        if (ctx.versatile_ap_chosen) {
            ctx.effective_ap += 1;
            if (logger_) logger_->on_rule_triggered("VersatileAttack", "ap+1_bonus_applied", 1);
        }

        // Lance: +2 AP when charging
        if (ctx.is_charge && ctx.weapon->has_rule(RuleId::Lance)) {
            ctx.effective_ap += 2;
            if (logger_) logger_->on_rule_triggered("Lance", "ap_bonus_on_charge", 2);
        }

        // Thrust: +1 AP when charging
        if (ctx.is_charge && ctx.weapon->has_rule(RuleId::Thrust)) {
            ctx.effective_ap += 1;
            if (logger_) logger_->on_rule_triggered("Thrust", "ap_bonus_on_charge", 1);
        }

        // Fragment: +1 AP against Defense 2+ to 4+
        if (ctx.weapon->has_rule(RuleId::Fragment)) {
            u8 def = defender.defense();
            if (def >= 2 && def <= 4) {
                ctx.effective_ap += 1;
                if (logger_) logger_->on_rule_triggered("Fragment", "ap_bonus_vs_defense", def);
            }
        }

        // Decimate: +2 AP against Defense 2+ to 3+
        if (ctx.weapon->has_rule(RuleId::Decimate)) {
            u8 def = defender.defense();
            if (def >= 2 && def <= 3) {
                ctx.effective_ap += 2;
                if (logger_) logger_->on_rule_triggered("Decimate", "ap_bonus_vs_defense", def);
            }
        }

        // Disintegrate: +2 AP against Defense 2+ to 3+
        if (ctx.weapon->has_rule(RuleId::Disintegrate)) {
            u8 def = defender.defense();
            if (def >= 2 && def <= 3) {
                ctx.effective_ap += 2;
                if (logger_) logger_->on_rule_triggered("Disintegrate", "ap_bonus_vs_defense", def);
            }
        }

        // Roll defense for normal hits
        u8 defense_target = defender.defense();
        if (logger_) dice_.enable_roll_recording(true);
        u32 wounds_from_normal = dice_.roll_defense_test(
            ctx.normal_hits, defense_target, ctx.effective_ap, ctx.defense_modifier, ctx.forces_defense_reroll);

        // Log defense rolls
        if (logger_) {
            std::vector<u8> all_rolls = dice_.take_recorded_rolls();
            i8 effective_target = static_cast<i8>(defense_target) + static_cast<i8>(ctx.effective_ap) - ctx.defense_modifier;
            effective_target = std::max(i8(2), std::min(i8(6), effective_target));
            u32 saves = ctx.normal_hits - wounds_from_normal;
            if (ctx.defense_modifier != 0 && logger_) {
                logger_->on_defense_modifier("Shielded", ctx.defense_modifier, "easier_saves");
            }

            // Split recorded rolls into initial rolls and rerolls
            // First normal_hits rolls are initial, rest are rerolls of 6s
            std::vector<u8> def_rolls;
            std::vector<u8> rerolls;
            u32 sixes_count = 0;

            if (ctx.forces_defense_reroll && all_rolls.size() > ctx.normal_hits) {
                def_rolls.assign(all_rolls.begin(), all_rolls.begin() + ctx.normal_hits);
                rerolls.assign(all_rolls.begin() + ctx.normal_hits, all_rolls.end());
                sixes_count = static_cast<u32>(rerolls.size());
            } else {
                def_rolls = std::move(all_rolls);
            }

            u32 reroll_saves = 0;
            for (u8 r : rerolls) {
                reroll_saves += (r >= static_cast<u8>(effective_target));
            }

            logger_->on_defense_rolls(defense_target, ctx.effective_ap,
                static_cast<u8>(effective_target), ctx.forces_defense_reroll,
                def_rolls, saves, wounds_from_normal, sixes_count, rerolls, reroll_saves);
        }

        // Roll defense for rending hits (AP+4)
        u32 wounds_from_rending = 0;
        if (ctx.rending_hits > 0) {
            u8 rending_ap = ctx.effective_ap + 4;
            if (logger_) dice_.enable_roll_recording(true);
            wounds_from_rending = dice_.roll_defense_test(
                ctx.rending_hits, defense_target, rending_ap, ctx.defense_modifier, ctx.forces_defense_reroll);

            if (logger_) {
                std::vector<u8> rend_rolls = dice_.take_recorded_rolls();
                i8 effective_target = static_cast<i8>(defense_target) + static_cast<i8>(rending_ap) - ctx.defense_modifier;
                effective_target = std::max(i8(2), std::min(i8(6), effective_target));
                u32 saves = ctx.rending_hits - wounds_from_rending;
                logger_->on_defense_rolls_rending(defense_target, rending_ap,
                    static_cast<u8>(effective_target), rend_rolls, saves, wounds_from_rending);
            }
        }

        if (logger_) dice_.enable_roll_recording(false);

        u32 total_wounds = wounds_from_normal + wounds_from_rending;
        ctx.wounds = total_wounds;

        // ============================================
        // Phase 8: WOUND_ALLOCATION
        // ============================================
        apply_all_phase_effects(CombatSubPhase::WOUND_ALLOCATION, ctx);

        // Apply wounds to defender
        if (total_wounds > 0) {
            u8 deadly_value = weapon.get_rule_value(RuleId::Deadly);
            if (deadly_value == 0) deadly_value = 1;  // No Deadly = 1 damage per wound
            auto wound_result = apply_wounds(defender, total_wounds, ctx.bypasses_regeneration, -1, deadly_value);
            result.wounds_dealt = wound_result.wounds_dealt;
            result.models_killed = wound_result.models_killed;
            result.self_destruct_hits = wound_result.self_destruct_hits;
        }

        if (logger_) {
            logger_->on_weapon_attack_end(weapon.name.c_str(), result.wounds_dealt, result.models_killed,
                                          attacker.total_wounds_remaining(), defender.total_wounds_remaining());
        }

        return result;
    }

public:
    // ==============================================================================
    // Game Interface Methods - Drop-in replacement for CombatEngine
    // ==============================================================================
    // These methods provide the same interface as the deprecated CombatEngine,
    // allowing GameRunner to use RegistryCombatResolver as a drop-in replacement.

    // Wound result structure (same as CombatEngine::WoundResult)
    struct WoundResult {
        u16 wounds_dealt = 0;
        u8 models_killed = 0;
        u32 self_destruct_hits = 0;  // Hits to return from SelfDestruct models
    };

    // Resolve shooting attack - iterates over all ranged weapons
    CombatResult resolve_shooting(UnitView attacker, UnitView defender, i8 distance, bool /*moved*/) {
        CombatResult result;

        u8 models_shooting = attacker.alive_count();
        if (models_shooting == 0) return result;

        // Check if any ranged weapons are in range
        bool has_valid_weapon = false;
        for (u8 i = 0; i < attacker.weapon_count(); ++i) {
            const Weapon& w = attacker.get_weapon(i);
            if (w.is_ranged() && w.range >= static_cast<u8>(distance)) {
                has_valid_weapon = true;
                break;
            }
        }
        if (!has_valid_weapon) return result;

        if (logger_) {
            logger_->on_shooting_start(true, attacker.unit->name.c_str(),
                                       defender.unit->name.c_str(), distance, models_shooting);
        }

        // Process each weapon
        for (u8 i = 0; i < attacker.weapon_count(); ++i) {
            const Weapon& w = attacker.get_weapon(i);
            if (!w.is_ranged() || w.range < static_cast<u8>(distance)) continue;

            // Limited: skip if already used this game
            if (w.has_rule(RuleId::Limited)) {
                if (attacker.is_limited_weapon_used(i)) {
                    if (logger_) logger_->on_rule_triggered("Limited", "weapon_already_used", i);
                    continue;
                }
                attacker.mark_limited_weapon_used(i);
                if (logger_) logger_->on_rule_triggered("Limited", "using_one_time_weapon", i);
            }

            // Calculate models with this weapon
            u8 models_with_weapon = std::min(w.count, models_shooting);
            if (models_with_weapon == 0) continue;

            // Calculate attacks
            u32 attacks = static_cast<u32>(models_with_weapon) * w.attacks;

            // Log weapon attack start
            if (logger_) {
                std::string rules_str = get_weapon_rules_str(w);
                logger_->on_weapon_attack_start(w.name.c_str(), false, w.range, static_cast<u8>(distance), w.attacks, w.ap, rules_str.c_str());
                logger_->on_attack_count(models_with_weapon, w.attacks, attacks);
            }

            // Roll to hit with modifiers
            u8 base_quality = attacker.quality();
            u8 quality = base_quality;
            i8 hit_modifier = 0;
            bool versatile_ap_bonus = false;

            // VersatileAttack: roll d6 to choose AP+1 (1-3) or +1 hit (4-6)
            if (attacker.has_rule(RuleId::VersatileAttack)) {
                u8 versatile_roll = dice_.roll_d6();
                if (versatile_roll <= 3) {
                    versatile_ap_bonus = true;
                    if (logger_) logger_->on_rule_triggered("VersatileAttack", "rolled_ap+1", versatile_roll);
                } else {
                    hit_modifier += 1;
                    if (logger_) logger_->on_hit_modifier("VersatileAttack", +1, "rolled_+1_hit");
                }
            }

            // Apply hit modifiers and log each one
            if (w.has_rule(RuleId::Reliable)) {
                quality = 2;
                if (logger_) logger_->on_hit_modifier("Reliable", 0, "quality_becomes_2+");
            }
            if (defender.has_rule(RuleId::Stealth) && distance > 9) {
                hit_modifier -= 1;
                if (logger_) logger_->on_hit_modifier("Stealth", -1, "target_beyond_9\"");
            }
            if (defender.has_rule(RuleId::RangedShrouding)) {
                hit_modifier -= 1;
                if (logger_) logger_->on_hit_modifier("RangedShrouding", -1, "harder_to_hit_at_range");
            }
            if (w.has_rule(RuleId::Precise)) {
                hit_modifier += 1;
                if (logger_) logger_->on_hit_modifier("Precise", +1, "weapon_accuracy");
            }
            if (attacker.has_rule(RuleId::GoodShot)) {
                hit_modifier += 1;
                if (logger_) logger_->on_hit_modifier("GoodShot", +1, "skilled_shooter");
            }
            if (attacker.has_rule(RuleId::BadShot)) {
                hit_modifier -= 1;
                if (logger_) logger_->on_hit_modifier("BadShot", -1, "poor_shooter");
            }
            u8 defender_tough = defender.get_rule_value(RuleId::Tough);
            if (w.has_rule(RuleId::Purge) && defender_tough >= 3) {
                hit_modifier += 1;
                if (logger_) logger_->on_hit_modifier("Purge", +1, "targeting_tough_3+");
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

            // Apply hit separation (Rending/Rupture)
            bool has_rending = w.has_rule(RuleId::Rending);
            bool has_rupture = w.has_rule(RuleId::Rupture);
            u32 rending_hits = has_rending ? sixes : 0;
            u32 normal_hits = hits - rending_hits;

            // Log Rending/Rupture triggers
            if (logger_ && has_rending && sixes > 0) logger_->on_rule_triggered("Rending", "ap+4_on_6s", sixes);
            if (logger_ && has_rupture && sixes > 0) logger_->on_rule_triggered("Rupture", "bonus_wounds_on_6s", sixes);

            // Apply hit bonuses (Relentless, Surge, etc.)
            u32 bonus_hits = 0;
            if (attacker.has_rule(RuleId::Relentless) && distance > 9) {
                bonus_hits += sixes;
                hits += sixes;
                if (logger_) logger_->on_rule_triggered("Relentless", "extra_hits_on_6s", sixes);
            }
            if (w.has_rule(RuleId::Surge)) {
                bonus_hits += sixes;
                hits += sixes;
                if (logger_) logger_->on_rule_triggered("Surge", "extra_hits_on_6s", sixes);
            }
            if (attacker.has_rule(RuleId::PointBlankSurge) && distance <= 9) {
                bonus_hits += sixes;
                hits += sixes;
                if (logger_) logger_->on_rule_triggered("PointBlankSurge", "extra_hits_on_6s_at_close_range", sixes);
            }

            // Apply hit multiplication (Blast)
            bool has_takedown = w.has_rule(RuleId::Takedown);
            u8 blast_value = w.get_rule_value(RuleId::Blast);
            if (blast_value > 0) {
                u8 max_multiplier = has_takedown ? u8(1) : static_cast<u8>(defender.alive_count());
                u8 multiplier = std::min(blast_value, max_multiplier);
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

            // Calculate effective AP with VersatileAttack bonus
            u8 effective_ap = w.ap;
            if (versatile_ap_bonus) {
                effective_ap += 1;
                if (logger_) logger_->on_rule_triggered("VersatileAttack", "ap+1_bonus_applied", 1);
            }

            // Determine bypass and reroll flags
            bool bypass_regen = has_rending || has_rupture ||
                                w.has_rule(RuleId::Bane) || w.has_rule(RuleId::Shred) ||
                                w.has_rule(RuleId::Unstoppable);
            bool force_reroll = w.has_rule(RuleId::Poison) || w.has_rule(RuleId::Bane);

            // Log reroll rule triggers
            if (force_reroll && logger_) {
                if (w.has_rule(RuleId::Poison)) logger_->on_rule_triggered("Poison", "defense_reroll_6s", 1);
                if (w.has_rule(RuleId::Bane)) logger_->on_rule_triggered("Bane", "defense_reroll_6s", 1);
            }

            // Calculate defense modifiers
            u8 effective_defense = defender.defense();
            if (defender.has_rule(RuleId::Shielded)) {
                effective_defense = std::max(u8(2), static_cast<u8>(effective_defense - 1));
                if (logger_) logger_->on_defense_modifier("Shielded", -1, "easier_saves_vs_shooting");
            }

            // Roll defense for normal hits
            if (logger_) dice_.enable_roll_recording(true);
            u32 wounds_from_normal = dice_.roll_defense_test(normal_hits, effective_defense, effective_ap, 0, force_reroll);

            // Log defense rolls
            if (logger_) {
                std::vector<u8> all_rolls = dice_.take_recorded_rolls();
                i8 effective_target = static_cast<i8>(effective_defense) + static_cast<i8>(effective_ap);
                effective_target = std::max(i8(2), std::min(i8(6), effective_target));
                u32 saves = normal_hits - wounds_from_normal;

                // Split recorded rolls into initial rolls and rerolls
                std::vector<u8> def_rolls;
                std::vector<u8> rerolls;
                u32 sixes_count = 0;

                if (force_reroll && all_rolls.size() > normal_hits) {
                    def_rolls.assign(all_rolls.begin(), all_rolls.begin() + normal_hits);
                    rerolls.assign(all_rolls.begin() + normal_hits, all_rolls.end());
                    sixes_count = static_cast<u32>(rerolls.size());
                } else {
                    def_rolls = std::move(all_rolls);
                }

                u32 reroll_saves = 0;
                for (u8 r : rerolls) {
                    reroll_saves += (r >= static_cast<u8>(effective_target));
                }

                logger_->on_defense_rolls(effective_defense, effective_ap, static_cast<u8>(effective_target), force_reroll,
                                          def_rolls, saves, wounds_from_normal, sixes_count, rerolls, reroll_saves);
            }

            // Roll defense for rending hits (AP+4)
            u32 wounds_from_rending = 0;
            if (rending_hits > 0) {
                u8 rending_ap = effective_ap + 4;
                if (logger_) dice_.enable_roll_recording(true);
                wounds_from_rending = dice_.roll_defense_test(rending_hits, effective_defense, rending_ap, 0, force_reroll);

                if (logger_) {
                    std::vector<u8> rend_rolls = dice_.take_recorded_rolls();
                    i8 effective_target = static_cast<i8>(effective_defense) + static_cast<i8>(rending_ap);
                    effective_target = std::max(i8(2), std::min(i8(6), effective_target));
                    u32 saves = rending_hits - wounds_from_rending;
                    logger_->on_defense_rolls_rending(defender.defense(), rending_ap, static_cast<u8>(effective_target),
                                                      rend_rolls, saves, wounds_from_rending);
                }
            }

            if (logger_) dice_.enable_roll_recording(false);

            u32 total_wounds = wounds_from_normal + wounds_from_rending;
            u8 weapon_models_killed = 0;
            u16 actual_wounds_dealt = 0;

            // Apply wounds
            if (total_wounds > 0) {
                u8 deadly_value = w.get_rule_value(RuleId::Deadly);
                if (deadly_value == 0) deadly_value = 1;
                auto wound_result = apply_wounds(defender, total_wounds, bypass_regen, -1, deadly_value);
                actual_wounds_dealt = wound_result.wounds_dealt;
                result.wounds_dealt += wound_result.wounds_dealt;
                result.models_killed += wound_result.models_killed;
                weapon_models_killed = wound_result.models_killed;
                result.self_destruct_hits += wound_result.self_destruct_hits;
            }

            if (logger_) {
                logger_->on_weapon_attack_end(w.name.c_str(), actual_wounds_dealt, weapon_models_killed,
                                              attacker.total_wounds_remaining(), defender.total_wounds_remaining());
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

    // Resolve melee attack - handles Impact, Counter, etc.
    CombatResult resolve_melee(UnitView attacker, UnitView defender, bool is_charging, u8 counter_models = 0) {
        CombatResult result;

        u8 models_fighting = attacker.alive_count();
        if (models_fighting == 0) return result;

        // Impact attacks (only when charging)
        if (is_charging && attacker.has_rule(RuleId::Impact)) {
            u8 impact_value = attacker.get_rule_value(RuleId::Impact);
            if (impact_value > 0) {
                // Counter: reduce Impact attacks by models with Counter
                u8 effective_impact = (impact_value > counter_models) ? (impact_value - counter_models) : 0;
                if (effective_impact > 0) {
                    u32 impact_attacks = static_cast<u32>(effective_impact) * models_fighting;
                    if (logger_) {
                        logger_->on_rule_triggered("Impact", "bonus_attacks_on_charge", impact_attacks);
                        if (counter_models > 0) {
                            logger_->on_rule_triggered("Counter", "reduced_impact_attacks", counter_models);
                        }
                    }

                    // Impact attacks hit on 2+
                    if (logger_) dice_.enable_roll_recording(true);
                    auto impact_hits = dice_.roll_d6_target(impact_attacks, 2);

                    if (logger_) {
                        std::vector<u8> rolls = dice_.take_recorded_rolls();
                        logger_->on_hit_rolls(2, 0, 2, rolls, impact_hits, 0);
                    }

                    // Roll defense
                    if (logger_) dice_.enable_roll_recording(true);
                    u32 impact_wounds = dice_.roll_defense_test(
                        impact_hits, defender.defense(), 0, 0, false);

                    if (logger_) {
                        std::vector<u8> def_rolls = dice_.take_recorded_rolls();
                        u32 saves = impact_hits - impact_wounds;
                        logger_->on_defense_rolls(defender.defense(), 0, defender.defense(), false,
                                                  def_rolls, saves, impact_wounds, 0, {}, 0);
                        dice_.enable_roll_recording(false);
                    }

                    if (impact_wounds > 0) {
                        auto wound_result = apply_wounds(defender, impact_wounds, false);
                        result.wounds_dealt += wound_result.wounds_dealt;
                        result.models_killed += wound_result.models_killed;
                        result.self_destruct_hits += wound_result.self_destruct_hits;
                    }
                }
            }
        }

        // Process each melee weapon
        for (u8 i = 0; i < attacker.weapon_count(); ++i) {
            const Weapon& w = attacker.get_weapon(i);
            if (!w.is_melee()) continue;

            // Limited: skip if already used
            if (w.has_rule(RuleId::Limited)) {
                if (attacker.is_limited_weapon_used(i)) {
                    if (logger_) logger_->on_rule_triggered("Limited", "weapon_already_used", i);
                    continue;
                }
                attacker.mark_limited_weapon_used(i);
                if (logger_) logger_->on_rule_triggered("Limited", "using_one_time_weapon", i);
            }

            u8 models_with_weapon = std::min(w.count, models_fighting);
            if (models_with_weapon == 0) continue;

            // Calculate attacks
            u32 attacks = static_cast<u32>(models_with_weapon) * w.attacks;

            // Log weapon attack start
            if (logger_) {
                std::string rules_str = get_weapon_rules_str(w);
                logger_->on_weapon_attack_start(w.name.c_str(), true, 0, 0, w.attacks, w.ap, rules_str.c_str());
                logger_->on_attack_count(models_with_weapon, w.attacks, attacks);
            }

            // Roll to hit with modifiers
            u8 base_quality = attacker.quality();
            u8 quality = base_quality;
            i8 hit_modifier = 0;
            bool versatile_ap_bonus = false;

            // VersatileAttack: roll d6 to choose AP+1 (1-3) or +1 hit (4-6)
            if (attacker.has_rule(RuleId::VersatileAttack)) {
                u8 versatile_roll = dice_.roll_d6();
                if (versatile_roll <= 3) {
                    versatile_ap_bonus = true;
                    if (logger_) logger_->on_rule_triggered("VersatileAttack", "rolled_ap+1", versatile_roll);
                } else {
                    hit_modifier += 1;
                    if (logger_) logger_->on_hit_modifier("VersatileAttack", +1, "rolled_+1_hit");
                }
            }

            // Apply hit modifiers and log each one
            if (w.has_rule(RuleId::Reliable)) {
                quality = 2;
                if (logger_) logger_->on_hit_modifier("Reliable", 0, "quality_becomes_2+");
            }
            if (w.has_rule(RuleId::Precise)) {
                hit_modifier += 1;
                if (logger_) logger_->on_hit_modifier("Precise", +1, "weapon_accuracy");
            }
            if (is_charging && w.has_rule(RuleId::Thrust)) {
                hit_modifier += 1;
                if (logger_) logger_->on_hit_modifier("Thrust", +1, "charging_accuracy");
            }
            if (defender.has_rule(RuleId::MeleeEvasion)) {
                hit_modifier -= 1;
                if (logger_) logger_->on_hit_modifier("Melee Evasion", -1, "target_evasive");
            }
            if (defender.has_rule(RuleId::MeleeShrouding)) {
                hit_modifier -= 1;
                if (logger_) logger_->on_hit_modifier("Melee Shrouding", -1, "target_shrouded");
            }
            u8 defender_tough = defender.get_rule_value(RuleId::Tough);
            if (w.has_rule(RuleId::Purge) && defender_tough >= 3) {
                hit_modifier += 1;
                if (logger_) logger_->on_hit_modifier("Purge", +1, "targeting_tough_3+");
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

            // Apply hit separation (Rending/Rupture)
            bool has_rending = w.has_rule(RuleId::Rending);
            bool has_rupture = w.has_rule(RuleId::Rupture);
            u32 rending_hits = has_rending ? sixes : 0;
            u32 normal_hits = hits - rending_hits;

            // Log Rending/Rupture triggers
            if (logger_ && has_rending && sixes > 0) logger_->on_rule_triggered("Rending", "ap+4_on_6s", sixes);
            if (logger_ && has_rupture && sixes > 0) logger_->on_rule_triggered("Rupture", "bonus_wounds_on_6s", sixes);

            // Apply hit bonuses (Furious)
            u32 bonus_hits = 0;
            if (is_charging && attacker.has_rule(RuleId::Furious)) {
                bonus_hits += sixes;
                hits += sixes;
                if (logger_) logger_->on_rule_triggered("Furious", "extra_hits_on_6s_when_charging", sixes);
            }
            if (w.has_rule(RuleId::Surge)) {
                bonus_hits += sixes;
                hits += sixes;
                if (logger_) logger_->on_rule_triggered("Surge", "extra_hits_on_6s", sixes);
            }

            // Apply hit multiplication (Blast)
            bool has_takedown = w.has_rule(RuleId::Takedown);
            u8 blast_value = w.get_rule_value(RuleId::Blast);
            if (blast_value > 0) {
                u8 max_multiplier = has_takedown ? u8(1) : static_cast<u8>(defender.alive_count());
                u8 multiplier = std::min(blast_value, max_multiplier);
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

            // Calculate AP with charge bonuses
            u8 effective_ap = w.ap;
            if (is_charging && w.has_rule(RuleId::Lance)) {
                effective_ap += 2;
                if (logger_) logger_->on_rule_triggered("Lance", "ap+2_on_charge", 2);
            }
            if (is_charging && w.has_rule(RuleId::Thrust)) {
                effective_ap += 1;
                if (logger_) logger_->on_rule_triggered("Thrust", "ap+1_on_charge", 1);
            }
            // VersatileAttack AP bonus (if rolled earlier)
            if (versatile_ap_bonus) {
                effective_ap += 1;
                if (logger_) logger_->on_rule_triggered("VersatileAttack", "ap+1_bonus_applied", 1);
            }

            // Determine bypass and reroll flags
            bool bypass_regen = has_rending || has_rupture ||
                                w.has_rule(RuleId::Bane) || w.has_rule(RuleId::Shred) ||
                                w.has_rule(RuleId::Unstoppable) ||
                                attacker.has_rule(RuleId::BaneInMelee);
            bool force_reroll = w.has_rule(RuleId::Poison) || w.has_rule(RuleId::Bane) ||
                                attacker.has_rule(RuleId::BaneInMelee);

            // Log reroll rule triggers
            if (force_reroll && logger_) {
                if (w.has_rule(RuleId::Poison)) logger_->on_rule_triggered("Poison", "defense_reroll_6s", 1);
                if (w.has_rule(RuleId::Bane)) logger_->on_rule_triggered("Bane", "defense_reroll_6s", 1);
                if (attacker.has_rule(RuleId::BaneInMelee)) logger_->on_rule_triggered("BaneInMelee", "defense_reroll_6s", 1);
            }

            // Calculate defense modifiers
            i8 defense_mod = 0;
            // Shielded: +1 to Defense vs non-spell attacks (easier saves)
            if (defender.has_rule(RuleId::Shielded)) {
                defense_mod -= 1;  // easier save (lower defense roll needed)
                if (logger_) logger_->on_defense_modifier("Shielded", -1, "easier_saves_vs_melee");
            }

            // Roll defense for normal hits
            if (logger_) dice_.enable_roll_recording(true);
            u32 wounds_from_normal = dice_.roll_defense_test(normal_hits, defender.defense(), effective_ap, defense_mod, force_reroll);

            // Log defense rolls
            if (logger_) {
                std::vector<u8> all_rolls = dice_.take_recorded_rolls();
                i8 effective_target = static_cast<i8>(defender.defense()) + static_cast<i8>(effective_ap);
                effective_target = std::max(i8(2), std::min(i8(6), effective_target));
                u32 saves = normal_hits - wounds_from_normal;

                // Split recorded rolls into initial rolls and rerolls
                std::vector<u8> def_rolls;
                std::vector<u8> rerolls;
                u32 sixes_count = 0;

                if (force_reroll && all_rolls.size() > normal_hits) {
                    def_rolls.assign(all_rolls.begin(), all_rolls.begin() + normal_hits);
                    rerolls.assign(all_rolls.begin() + normal_hits, all_rolls.end());
                    sixes_count = static_cast<u32>(rerolls.size());
                } else {
                    def_rolls = std::move(all_rolls);
                }

                u32 reroll_saves = 0;
                for (u8 r : rerolls) {
                    reroll_saves += (r >= static_cast<u8>(effective_target));
                }

                logger_->on_defense_rolls(defender.defense(), effective_ap, static_cast<u8>(effective_target), force_reroll,
                                          def_rolls, saves, wounds_from_normal, sixes_count, rerolls, reroll_saves);
            }

            // Roll defense for rending hits (AP+4)
            u32 wounds_from_rending = 0;
            if (rending_hits > 0) {
                u8 rending_ap = effective_ap + 4;
                if (logger_) dice_.enable_roll_recording(true);
                wounds_from_rending = dice_.roll_defense_test(rending_hits, defender.defense(), rending_ap, defense_mod, force_reroll);

                if (logger_) {
                    std::vector<u8> rend_rolls = dice_.take_recorded_rolls();
                    i8 effective_target = static_cast<i8>(defender.defense()) + static_cast<i8>(rending_ap);
                    effective_target = std::max(i8(2), std::min(i8(6), effective_target));
                    u32 saves = rending_hits - wounds_from_rending;
                    logger_->on_defense_rolls_rending(defender.defense(), rending_ap, static_cast<u8>(effective_target),
                                                      rend_rolls, saves, wounds_from_rending);
                }
            }

            if (logger_) dice_.enable_roll_recording(false);

            u32 total_wounds = wounds_from_normal + wounds_from_rending;
            u8 weapon_models_killed = 0;
            u16 actual_wounds_dealt = 0;

            // Apply wounds
            if (total_wounds > 0) {
                u8 deadly_value = w.get_rule_value(RuleId::Deadly);
                if (deadly_value == 0) deadly_value = 1;
                auto wound_result = apply_wounds(defender, total_wounds, bypass_regen, -1, deadly_value);
                actual_wounds_dealt = wound_result.wounds_dealt;
                result.wounds_dealt += wound_result.wounds_dealt;
                result.models_killed += wound_result.models_killed;
                weapon_models_killed = wound_result.models_killed;
                result.self_destruct_hits += wound_result.self_destruct_hits;
            }

            if (logger_) {
                logger_->on_weapon_attack_end(w.name.c_str(), actual_wounds_dealt, weapon_models_killed,
                                              attacker.total_wounds_remaining(), defender.total_wounds_remaining());
            }
        }

        result.target_destroyed = defender.is_destroyed();
        result.target_shaken = defender.is_shaken();
        result.target_routed = defender.is_routed();

        return result;
    }

    // ==============================================================================
    // PHASED COMBAT METHODS - Use registry effect dispatch
    // ==============================================================================
    // These methods use the new phase-based effect system where rules are defined
    // once and apply automatically. They can replace the manual methods above.

    // Resolve shooting using phase-based effect dispatch
    CombatResult resolve_shooting_phased(UnitView attacker, UnitView defender, i8 distance, bool moved) {
        CombatResult result;

        u8 models_shooting = attacker.alive_count();
        if (models_shooting == 0) return result;

        // Check if any ranged weapons are in range
        bool has_valid_weapon = false;
        for (u8 i = 0; i < attacker.weapon_count(); ++i) {
            const Weapon& w = attacker.get_weapon(i);
            if (w.is_ranged() && w.range >= static_cast<u8>(distance)) {
                has_valid_weapon = true;
                break;
            }
        }
        if (!has_valid_weapon) return result;

        if (logger_) {
            logger_->on_shooting_start(true, attacker.unit->name.c_str(),
                                       defender.unit->name.c_str(), distance, models_shooting);
        }

        // Process each weapon using phased resolution
        for (u8 i = 0; i < attacker.weapon_count(); ++i) {
            const Weapon& w = attacker.get_weapon(i);
            if (!w.is_ranged() || w.range < static_cast<u8>(distance)) continue;

            // Limited: skip if already used this game
            if (w.has_rule(RuleId::Limited)) {
                if (attacker.is_limited_weapon_used(i)) {
                    if (logger_) logger_->on_rule_triggered("Limited", "weapon_already_used", i);
                    continue;
                }
                attacker.mark_limited_weapon_used(i);
                if (logger_) logger_->on_rule_triggered("Limited", "using_one_time_weapon", i);
            }

            u8 models_with_weapon = std::min(w.count, models_shooting);
            if (models_with_weapon == 0) continue;

            // Use phased weapon attack resolution
            auto weapon_result = resolve_weapon_attack_phased(
                attacker, defender, w, i,
                CombatType::SHOOTING, static_cast<u8>(distance),
                false,  // not charging
                models_with_weapon,
                moved   // did unit move before shooting
            );

            result.wounds_dealt = static_cast<u16>(result.wounds_dealt + weapon_result.wounds_dealt);
            result.models_killed = static_cast<u8>(result.models_killed + weapon_result.models_killed);
            result.self_destruct_hits += weapon_result.self_destruct_hits;
        }

        result.target_destroyed = defender.is_destroyed();
        result.target_shaken = defender.is_shaken();
        result.target_routed = defender.is_routed();

        if (logger_) {
            logger_->on_shooting_end(true, result.wounds_dealt, result.models_killed, result.target_destroyed);
        }

        return result;
    }

    // Resolve melee using phase-based effect dispatch
    CombatResult resolve_melee_phased(UnitView attacker, UnitView defender, bool is_charging, u8 counter_models = 0) {
        CombatResult result;

        u8 models_fighting = attacker.alive_count();
        if (models_fighting == 0) return result;

        // Impact attacks (only when charging) - handle separately as it's a unit-level effect
        if (is_charging && attacker.has_rule(RuleId::Impact)) {
            u8 impact_value = attacker.get_rule_value(RuleId::Impact);
            if (impact_value > 0) {
                u8 effective_impact = (impact_value > counter_models) ? (impact_value - counter_models) : 0;
                if (effective_impact > 0) {
                    u32 impact_attacks = static_cast<u32>(effective_impact) * models_fighting;
                    if (logger_) {
                        logger_->on_rule_triggered("Impact", "bonus_attacks_on_charge", impact_attacks);
                        if (counter_models > 0) {
                            logger_->on_rule_triggered("Counter", "reduced_impact_attacks", counter_models);
                        }
                    }

                    // Impact attacks hit on 2+ (automatic hit on 2+, not quality-based)
                    if (logger_) dice_.enable_roll_recording(true);
                    u32 impact_hits = dice_.roll_d6_target(impact_attacks, 2);

                    if (logger_) {
                        std::vector<u8> rolls = dice_.take_recorded_rolls();
                        logger_->on_hit_rolls(2, 0, 2, rolls, impact_hits, 0);
                    }

                    if (impact_hits > 0) {
                        // Impact has AP(1) by default
                        u8 impact_ap = 1;

                        // Roll defense for Impact hits
                        if (logger_) dice_.enable_roll_recording(true);
                        u32 impact_wounds = dice_.roll_defense_test(
                            impact_hits, defender.defense(), impact_ap, 0, false);

                        if (logger_) {
                            std::vector<u8> def_rolls = dice_.take_recorded_rolls();
                            u32 saves = impact_hits - impact_wounds;
                            u8 defense_target = std::min(static_cast<u8>(6),
                                static_cast<u8>(defender.defense() + impact_ap));
                            logger_->on_defense_rolls(defender.defense(), impact_ap, defense_target, false,
                                                      def_rolls, saves, impact_wounds, 0, {}, 0);
                            dice_.enable_roll_recording(false);
                        }

                        if (impact_wounds > 0) {
                            auto wound_result = apply_wounds(defender, impact_wounds, false);
                            result.wounds_dealt += wound_result.wounds_dealt;
                            result.models_killed += wound_result.models_killed;
                            result.self_destruct_hits += wound_result.self_destruct_hits;

                            if (logger_) {
                                logger_->on_weapon_attack_end("Impact", impact_wounds, wound_result.models_killed,
                                                              attacker.total_wounds_remaining(), defender.total_wounds_remaining());
                            }
                        }
                    }
                }
            }
        }

        if (logger_) {
            const char* charge_str = is_charging ? " (CHARGING)" : "";
            std::string msg = std::string("MELEE: ") + attacker.unit->name.c_str() +
                              " vs " + defender.unit->name.c_str() + charge_str;
        }

        // Process each melee weapon using phased resolution
        for (u8 i = 0; i < attacker.weapon_count(); ++i) {
            const Weapon& w = attacker.get_weapon(i);
            if (!w.is_melee()) continue;

            // Limited: skip if already used
            if (w.has_rule(RuleId::Limited)) {
                if (attacker.is_limited_weapon_used(i)) {
                    if (logger_) logger_->on_rule_triggered("Limited", "weapon_already_used", i);
                    continue;
                }
                attacker.mark_limited_weapon_used(i);
                if (logger_) logger_->on_rule_triggered("Limited", "using_one_time_weapon", i);
            }

            u8 models_with_weapon = std::min(w.count, models_fighting);
            if (models_with_weapon == 0) continue;

            // Use phased weapon attack resolution
            auto weapon_result = resolve_weapon_attack_phased(
                attacker, defender, w, i,
                CombatType::MELEE, 0,  // distance 0 for melee
                is_charging,
                models_with_weapon
            );

            result.wounds_dealt = static_cast<u16>(result.wounds_dealt + weapon_result.wounds_dealt);
            result.models_killed = static_cast<u8>(result.models_killed + weapon_result.models_killed);
            result.self_destruct_hits += weapon_result.self_destruct_hits;
        }

        result.target_destroyed = defender.is_destroyed();
        result.target_shaken = defender.is_shaken();
        result.target_routed = defender.is_routed();

        return result;
    }

    // Apply wounds to a unit with proper wound allocation
    // deadly_value: each unsaved wound deals this much damage (no carry-over between models)
    WoundResult apply_wounds(UnitView unit, u32 wounds, bool bypass_regeneration = false, i8 takedown_target = -1, u8 deadly_value = 1) {
        WoundResult result;

        // Regeneration check (before Deadly multiplication)
        if (!bypass_regeneration && unit.has_rule(RuleId::Regeneration)) {
            u32 original_wounds = wounds;
            wounds = dice_.roll_regeneration(wounds, 5);
            if (logger_) {
                u32 blocked = original_wounds - wounds;
                logger_->on_rule_triggered("Regeneration", "blocked_wounds", blocked);
            }
        }

        // Resistance: 6+ to ignore each wound (after regeneration)
        if (unit.has_rule(RuleId::Resistance) && wounds > 0) {
            u32 original_wounds = wounds;
            wounds = dice_.roll_regeneration(wounds, 6);
            if (logger_) {
                u32 blocked = original_wounds - wounds;
                logger_->on_rule_triggered("Resistance", "resisted_wounds", blocked);
            }
        }

        // Takedown: apply wounds to specific target first (Deadly not compatible with Takedown)
        if (takedown_target >= 0 && unit.model_is_alive(static_cast<u8>(takedown_target))) {
            u8 model_idx = static_cast<u8>(takedown_target);
            u8 wounds_to_kill = unit.model_remaining_wounds(model_idx);
            u8 wounds_applied = static_cast<u8>(std::min(wounds, static_cast<u32>(wounds_to_kill)));

            result.wounds_dealt = wounds_applied;
            for (u8 w = 0; w < wounds_applied; ++w) {
                if (unit.apply_wound_to_model(model_idx)) {
                    result.models_killed++;
                    // SelfDestruct: when model dies, queue hits for attacker
                    if (unit.has_rule(RuleId::SelfDestruct)) {
                        u8 destruct_value = unit.get_rule_value(RuleId::SelfDestruct);
                        result.self_destruct_hits += destruct_value;
                        if (logger_) logger_->on_rule_triggered("SelfDestruct", "queued_hits_for_attacker", destruct_value);
                    }
                    break;
                }
            }
            return result;
        }

        // Get wound allocation order (normal allocation)
        std::array<u8, MAX_MODELS_PER_UNIT> order;
        u8 order_count = 0;
        unit.get_wound_allocation_order(order, order_count);

        // Deadly(X): each wound deals X damage, but doesn't carry over between models
        if (deadly_value > 1) {
            if (logger_) logger_->on_rule_triggered("Deadly", "damage_per_wound", deadly_value);

            u8 order_idx = 0;
            for (u32 w = 0; w < wounds && order_idx < order_count; ++w) {
                // Find next alive model
                while (order_idx < order_count && !unit.model_is_alive(order[order_idx])) {
                    order_idx++;
                }
                if (order_idx >= order_count) break;

                u8 model_idx = order[order_idx];
                u8 model_wounds_remaining = unit.model_remaining_wounds(model_idx);

                // Apply up to deadly_value wounds to this model (capped at what kills it)
                u8 damage_to_apply = std::min(deadly_value, model_wounds_remaining);
                result.wounds_dealt += damage_to_apply;

                for (u8 d = 0; d < damage_to_apply; ++d) {
                    if (unit.apply_wound_to_model(model_idx)) {
                        result.models_killed++;
                        if (unit.has_rule(RuleId::SelfDestruct)) {
                            u8 destruct_value = unit.get_rule_value(RuleId::SelfDestruct);
                            result.self_destruct_hits += destruct_value;
                            if (logger_) logger_->on_rule_triggered("SelfDestruct", "queued_hits_for_attacker", destruct_value);
                        }
                        order_idx++;  // Move to next model for next wound
                        break;
                    }
                }
                // Note: excess damage from Deadly is lost (doesn't carry over)
            }
            return result;
        }

        // Normal wound allocation (no Deadly)
        result.wounds_dealt = static_cast<u16>(wounds);
        u32 remaining_wounds = wounds;

        for (u8 i = 0; i < order_count && remaining_wounds > 0; ++i) {
            u8 model_idx = order[i];
            if (!unit.model_is_alive(model_idx)) continue;

            u8 wounds_to_kill = unit.model_remaining_wounds(model_idx);
            u8 wounds_applied = static_cast<u8>(std::min(remaining_wounds, static_cast<u32>(wounds_to_kill)));

            for (u8 w = 0; w < wounds_applied; ++w) {
                if (unit.apply_wound_to_model(model_idx)) {
                    result.models_killed++;
                    if (unit.has_rule(RuleId::SelfDestruct)) {
                        u8 destruct_value = unit.get_rule_value(RuleId::SelfDestruct);
                        result.self_destruct_hits += destruct_value;
                        if (logger_) logger_->on_rule_triggered("SelfDestruct", "queued_hits_for_attacker", destruct_value);
                    }
                    break;
                }
            }
            remaining_wounds -= wounds_applied;
        }

        return result;
    }

    // Morale check
    bool check_morale(UnitView unit, bool is_from_melee = false, u32 melee_wounds_taken = 0, u32 melee_wounds_dealt = 0, bool is_unit_a = true) {
        // Check if morale test is needed
        bool needs_test = false;
        const char* trigger_reason = "";

        // At half strength
        if (unit.is_at_half_strength() && !unit.is_shaken() && !unit.is_routed()) {
            needs_test = true;
            trigger_reason = "half_strength";
        }

        // Lost melee - caller already determined this unit lost (including Fear adjustment)
        if (is_from_melee) {
            needs_test = true;
            trigger_reason = "lost_melee";
        }

        if (!needs_test) return true;

        UnitStatus old_status = unit.state->status;

        if (logger_) {
            logger_->on_morale_check_start(is_unit_a, unit.unit->name.c_str(), trigger_reason,
                                           unit.alive_count(), unit.unit->model_count,
                                           static_cast<u16>(melee_wounds_taken), static_cast<u16>(melee_wounds_dealt));
        }

        // Roll morale test
        u8 roll = dice_.roll_d6();
        u8 target = unit.quality();

        // MoraleBoost: +1 to morale tests
        if (unit.has_rule(RuleId::MoraleBoost)) {
            roll += 1;
            if (logger_) logger_->on_rule_triggered("MoraleBoost", "morale_bonus", 1);
        }

        bool passed = roll >= target;

        if (logger_) {
            logger_->on_morale_roll(roll, target, passed);
        }

        // Fearless: reroll failed test, pass on 4+
        if (!passed && unit.has_rule(RuleId::Fearless)) {
            u8 fearless_roll = dice_.roll_d6();
            bool fearless_passed = fearless_roll >= 4;
            passed = fearless_passed;
            if (logger_) logger_->on_fearless_roll(fearless_roll, 4, fearless_passed);
        }

        // Hold the Line: reroll failed morale test using quality
        if (!passed && unit.has_rule(RuleId::HoldTheLine)) {
            u8 hold_line_roll = dice_.roll_d6();
            bool hold_line_passed = hold_line_roll >= unit.quality();
            passed = hold_line_passed;
            if (logger_) logger_->on_fearless_roll(hold_line_roll, unit.quality(), hold_line_passed);
        }

        if (passed) {
            if (logger_) logger_->on_morale_check_end(true, old_status, old_status, "passed");
            return true;
        }

        // Failed morale
        UnitStatus new_status = old_status;
        const char* result_desc = "";

        // NoRetreat: Take wounds instead of becoming Shaken
        if (unit.has_rule(RuleId::NoRetreat)) {
            u8 wounds_taken = (dice_.roll_d6() + 1) / 2;  // d3
            if (logger_) logger_->on_rule_triggered("NoRetreat", "wounds_instead_of_shaken", wounds_taken);

            for (u8 w = 0; w < wounds_taken && !unit.is_destroyed(); ++w) {
                for (u8 m = 0; m < unit.unit->model_count; ++m) {
                    if (unit.model_is_alive(m)) {
                        unit.apply_wound_to_model(m);
                        break;
                    }
                }
            }
            result_desc = "no_retreat_wounds_taken";
        } else if (is_from_melee) {
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
    (void)attacker;
    (void)defender;

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
    (void)defender;

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
