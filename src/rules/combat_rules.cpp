#include "rules/combat_rules.hpp"
#include "core/rule_registry.hpp"
#include "core/unit.hpp"
#include "core/weapon.hpp"

namespace battle {

// Using declaration to bring RuleRegistry into scope for this file
using ::battle::RuleRegistry;

namespace rules {

// ==============================================================================
// Effect Function Implementations
// ==============================================================================

// === HIT_MODIFIERS Phase Effects ===

void precise_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    ctx.hit_modifier += 1;
}

void stealth_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    ctx.hit_modifier -= 1;
}

bool stealth_condition(const CombatContextCore& ctx) {
    // Stealth only applies when shooting from >9"
    return ctx.combat_type == CombatType::SHOOTING && ctx.distance > 9;
}

void ranged_shrouding_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    ctx.hit_modifier -= 1;
}

void melee_evasion_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    ctx.hit_modifier -= 1;
}

void melee_shrouding_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    ctx.hit_modifier -= 1;
}

void good_shot_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    ctx.hit_modifier += 1;
}

void bad_shot_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    ctx.hit_modifier -= 1;
}

void purge_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    ctx.hit_modifier += 1;
}

bool purge_condition(const CombatContextCore& ctx) {
    // Purge applies vs Tough(3+) targets
    if (!ctx.defender) return false;
    return ctx.defender->get_rule_value(RuleId::Tough) >= 3;
}

void thrust_hit_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    ctx.hit_modifier += 1;
}

bool thrust_condition(const CombatContextCore& ctx) {
    return ctx.is_charge;
}

// === ROLL_HITS Phase Effects ===

void reliable_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Quality becomes 2+
    ctx.quality_used = 2;
    if (ext) {
        ext->quality_override = 2;
    }
}

// === HIT_SEPARATION Phase Effects ===

void rending_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Natural 6s to hit get AP+4 - they are tracked separately
    if (ext) {
        ext->rending_hits = ctx.natural_sixes;
    }
    // Note: The actual AP+4 is applied during defense resolution
    // This function just marks which hits are "rending"
}

void rupture_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Natural 6s bypass regen and deal +1 wound per wound
    if (ext) {
        ext->rupture_hits = ctx.natural_sixes;
    }
    // Set bypass regen flag
    ctx.set_bypass_regen(true);
}

// === HIT_BONUSES Phase Effects ===

void relentless_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Extra hits on natural 6s
    u32 bonus = ctx.natural_sixes;
    ctx.hits += bonus;
    if (ext) {
        ext->bonus_hits += bonus;
    }
}

bool relentless_condition(const CombatContextCore& ctx) {
    // Only when shooting from >9"
    return ctx.combat_type == CombatType::SHOOTING && ctx.distance > 9;
}

void surge_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Extra hits on natural 6s
    u32 bonus = ctx.natural_sixes;
    ctx.hits += bonus;
    if (ext) {
        ext->bonus_hits += bonus;
    }
}

void point_blank_surge_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Extra hits on natural 6s at short range
    u32 bonus = ctx.natural_sixes;
    ctx.hits += bonus;
    if (ext) {
        ext->bonus_hits += bonus;
    }
}

bool point_blank_surge_condition(const CombatContextCore& ctx) {
    // Only when shooting at <=9"
    return ctx.combat_type == CombatType::SHOOTING && ctx.distance <= 9;
}

void furious_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Extra hits on natural 6s when charging
    u32 bonus = ctx.natural_sixes;
    ctx.hits += bonus;
    if (ext) {
        ext->bonus_hits += bonus;
    }
}

bool furious_condition(const CombatContextCore& ctx) {
    return ctx.is_charge;
}

// === HIT_MULTIPLICATION Phase Effects ===

void blast_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 value) {
    // Multiply hits by value, capped at defender model count
    if (!ctx.defender) return;

    u8 max_multiplier = ctx.defender->alive_count;
    u8 multiplier = std::min(value, max_multiplier);

    // If Takedown is active, cap at 1
    if (ctx.has_takedown()) {
        multiplier = 1;
    }

    ctx.hits *= multiplier;

    // Also multiply rending/rupture hits if they exist
    if (ext && ext->rending_hits > 0) {
        ext->rending_hits *= multiplier;
    }
    if (ext && ext->rupture_hits > 0) {
        ext->rupture_hits *= multiplier;
    }

    if (ext) {
        ext->hit_multiplier = multiplier;
    }
}

// ==============================================================================
// Hot Data Definitions
// ==============================================================================

const RuleHotData Precise_HotData{
    RuleId::Precise,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_MODIFIERS),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL)
};

const RuleHotData Reliable_HotData{
    RuleId::Reliable,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::ROLL_HITS),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::FIRST,
    static_cast<TraitMask>(RuleTrait::OVERRIDES_QUALITY)
};

const RuleHotData Rending_HotData{
    RuleId::Rending,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_SEPARATION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ON_HIT_ROLL_6,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::BYPASSES_REGENERATION) |
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP)
};

const RuleHotData Blast_HotData{
    RuleId::Blast,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_MULTIPLICATION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MULTIPLIES_HITS) |
    static_cast<TraitMask>(RuleTrait::HAS_VALUE)
};

const RuleHotData Deadly_HotData{
    RuleId::Deadly,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::WOUND_ALLOCATION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MULTIPLIES_WOUNDS) |
    static_cast<TraitMask>(RuleTrait::HAS_VALUE)
};

const RuleHotData Poison_HotData{
    RuleId::Poison,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::FORCES_DEFENSE_REROLL)
};

const RuleHotData Bane_HotData{
    RuleId::Bane,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::BYPASSES_REGENERATION) |
    static_cast<TraitMask>(RuleTrait::FORCES_DEFENSE_REROLL)
};

const RuleHotData Stealth_HotData{
    RuleId::Stealth,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_MODIFIERS),
    CombatType::SHOOTING,
    Target::DEFENDER,
    Trigger::ALWAYS,
    RulePriority::EARLY,
    static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL) |
    static_cast<TraitMask>(RuleTrait::RANGED_ONLY)
};

const RuleHotData Relentless_HotData{
    RuleId::Relentless,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_BONUSES),
    CombatType::SHOOTING,
    Target::ATTACKER,
    Trigger::ON_HIT_ROLL_6,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS) |
    static_cast<TraitMask>(RuleTrait::RANGED_ONLY)
};

const RuleHotData Furious_HotData{
    RuleId::Furious,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_BONUSES),
    CombatType::MELEE,
    Target::ATTACKER,
    Trigger::ON_HIT_ROLL_6,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS) |
    static_cast<TraitMask>(RuleTrait::CHARGE_ONLY) |
    static_cast<TraitMask>(RuleTrait::MELEE_ONLY)
};

const RuleHotData Surge_HotData{
    RuleId::Surge,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_BONUSES),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ON_HIT_ROLL_6,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS)
};

const RuleHotData PointBlankSurge_HotData{
    RuleId::PointBlankSurge,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_BONUSES),
    CombatType::SHOOTING,
    Target::ATTACKER,
    Trigger::ON_HIT_ROLL_6,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS) |
    static_cast<TraitMask>(RuleTrait::RANGED_ONLY)
};

const RuleHotData Rupture_HotData{
    RuleId::Rupture,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_SEPARATION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ON_HIT_ROLL_6,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::BYPASSES_REGENERATION) |
    static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_WOUNDS)
};

const RuleHotData GoodShot_HotData{
    RuleId::GoodShot,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_MODIFIERS),
    CombatType::SHOOTING,
    Target::ATTACKER,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL) |
    static_cast<TraitMask>(RuleTrait::RANGED_ONLY)
};

const RuleHotData BadShot_HotData{
    RuleId::BadShot,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_MODIFIERS),
    CombatType::SHOOTING,
    Target::ATTACKER,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL) |
    static_cast<TraitMask>(RuleTrait::RANGED_ONLY)
};

const RuleHotData RangedShrouding_HotData{
    RuleId::RangedShrouding,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_MODIFIERS),
    CombatType::SHOOTING,
    Target::DEFENDER,
    Trigger::ALWAYS,
    RulePriority::EARLY,
    static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL) |
    static_cast<TraitMask>(RuleTrait::RANGED_ONLY)
};

const RuleHotData MeleeEvasion_HotData{
    RuleId::MeleeEvasion,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_MODIFIERS),
    CombatType::MELEE,
    Target::DEFENDER,
    Trigger::ALWAYS,
    RulePriority::EARLY,
    static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL) |
    static_cast<TraitMask>(RuleTrait::MELEE_ONLY)
};

const RuleHotData MeleeShrouding_HotData{
    RuleId::MeleeShrouding,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_MODIFIERS),
    CombatType::MELEE,
    Target::DEFENDER,
    Trigger::ALWAYS,
    RulePriority::EARLY,
    static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL) |
    static_cast<TraitMask>(RuleTrait::MELEE_ONLY)
};

const RuleHotData Purge_HotData{
    RuleId::Purge,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_MODIFIERS),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::LATE,
    static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL)
};

const RuleHotData Thrust_HotData{
    RuleId::Thrust,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_MODIFIERS),
    CombatType::MELEE,
    Target::WEAPON,
    Trigger::ON_CHARGE,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL) |
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP) |
    static_cast<TraitMask>(RuleTrait::CHARGE_ONLY) |
    static_cast<TraitMask>(RuleTrait::MELEE_ONLY)
};

// ==============================================================================
// Cold Data Definitions
// ==============================================================================

const RuleColdData Precise_ColdData{
    "Precise",
    nullptr,
    0,
    "Precise: +1 to hit",
    "Attacks with this weapon get +1 to hit."
};

const RuleColdData Reliable_ColdData{
    "Reliable",
    nullptr,
    0,
    "Reliable: Quality becomes 2+",
    "Attacks with this weapon hit on 2+."
};

const RuleColdData Rending_ColdData{
    "Rending",
    nullptr,
    0,
    "Rending: {} hits at AP+4",
    "Unmodified 6s to hit gain AP(4) and bypass regeneration."
};

const RuleColdData Blast_ColdData{
    "Blast",
    nullptr,
    0,
    "Blast({}): multiplied hits",
    "Multiply hits by value (max = target model count)."
};

const RuleColdData Stealth_ColdData{
    "Stealth",
    nullptr,
    0,
    "Stealth: -1 to hit from >9\"",
    "Enemies shooting this unit from more than 9\" away get -1 to hit."
};

// ==============================================================================
// Effect Entry Definitions
// ==============================================================================

const RuleEffectEntry Precise_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_MODIFIERS, precise_effect)
    .build();

const RuleEffectEntry Reliable_Effects = EffectBuilder()
    .combat(CombatSubPhase::ROLL_HITS, reliable_effect)
    .build();

const RuleEffectEntry Rending_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_SEPARATION, rending_effect)
    .build();

const RuleEffectEntry Stealth_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_MODIFIERS, stealth_effect)
    .condition(CombatSubPhase::HIT_MODIFIERS, stealth_condition)
    .build();

const RuleEffectEntry Relentless_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_BONUSES, relentless_effect)
    .condition(CombatSubPhase::HIT_BONUSES, relentless_condition)
    .build();

const RuleEffectEntry Surge_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_BONUSES, surge_effect)
    .build();

const RuleEffectEntry Blast_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_MULTIPLICATION, blast_effect)
    .build();

// ==============================================================================
// Registration Function
// ==============================================================================

void register_combat_rules(RuleRegistry& registry) {
    // Register hot data
    registry.register_hot_data(RuleId::Precise, Precise_HotData);
    registry.register_hot_data(RuleId::Reliable, Reliable_HotData);
    registry.register_hot_data(RuleId::Rending, Rending_HotData);
    registry.register_hot_data(RuleId::Blast, Blast_HotData);
    registry.register_hot_data(RuleId::Deadly, Deadly_HotData);
    registry.register_hot_data(RuleId::Poison, Poison_HotData);
    registry.register_hot_data(RuleId::Bane, Bane_HotData);
    registry.register_hot_data(RuleId::Stealth, Stealth_HotData);
    registry.register_hot_data(RuleId::Relentless, Relentless_HotData);
    registry.register_hot_data(RuleId::Furious, Furious_HotData);
    registry.register_hot_data(RuleId::Surge, Surge_HotData);
    registry.register_hot_data(RuleId::PointBlankSurge, PointBlankSurge_HotData);
    registry.register_hot_data(RuleId::Rupture, Rupture_HotData);
    registry.register_hot_data(RuleId::GoodShot, GoodShot_HotData);
    registry.register_hot_data(RuleId::BadShot, BadShot_HotData);
    registry.register_hot_data(RuleId::RangedShrouding, RangedShrouding_HotData);
    registry.register_hot_data(RuleId::MeleeEvasion, MeleeEvasion_HotData);
    registry.register_hot_data(RuleId::MeleeShrouding, MeleeShrouding_HotData);
    registry.register_hot_data(RuleId::Purge, Purge_HotData);
    registry.register_hot_data(RuleId::Thrust, Thrust_HotData);

    // Register cold data
    registry.register_cold_data(RuleId::Precise, Precise_ColdData);
    registry.register_cold_data(RuleId::Reliable, Reliable_ColdData);
    registry.register_cold_data(RuleId::Rending, Rending_ColdData);
    registry.register_cold_data(RuleId::Blast, Blast_ColdData);
    registry.register_cold_data(RuleId::Stealth, Stealth_ColdData);

    // Register effects
    registry.register_effects(RuleId::Precise, Precise_Effects);
    registry.register_effects(RuleId::Reliable, Reliable_Effects);
    registry.register_effects(RuleId::Rending, Rending_Effects);
    registry.register_effects(RuleId::Stealth, Stealth_Effects);
    registry.register_effects(RuleId::Relentless, Relentless_Effects);
    registry.register_effects(RuleId::Surge, Surge_Effects);
    registry.register_effects(RuleId::Blast, Blast_Effects);

    // Register aliases for parsing
    registry.register_alias("precise", RuleId::Precise);
    registry.register_alias("Precise", RuleId::Precise);
    registry.register_alias("reliable", RuleId::Reliable);
    registry.register_alias("Reliable", RuleId::Reliable);
    registry.register_alias("rending", RuleId::Rending);
    registry.register_alias("Rending", RuleId::Rending);
    registry.register_alias("blast", RuleId::Blast);
    registry.register_alias("Blast", RuleId::Blast);
    registry.register_alias("stealth", RuleId::Stealth);
    registry.register_alias("Stealth", RuleId::Stealth);
}

} // namespace rules

// ==============================================================================
// Factory Function Implementation
// ==============================================================================

RuleRegistry create_default_registry() {
    RuleRegistry registry;
    rules::register_combat_rules(registry);
    return registry;
}

} // namespace battle
