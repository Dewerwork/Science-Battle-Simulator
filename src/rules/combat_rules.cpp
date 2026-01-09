#include "rules/combat_rules.hpp"
#include "core/rule_registry.hpp"
#include "core/contexts.hpp"
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

// === Additional Hit Modifier Effects ===

const RuleEffectEntry GoodShot_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_MODIFIERS, good_shot_effect)
    .build();

const RuleEffectEntry BadShot_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_MODIFIERS, bad_shot_effect)
    .build();

const RuleEffectEntry RangedShrouding_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_MODIFIERS, ranged_shrouding_effect)
    .build();

const RuleEffectEntry MeleeEvasion_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_MODIFIERS, melee_evasion_effect)
    .build();

const RuleEffectEntry MeleeShrouding_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_MODIFIERS, melee_shrouding_effect)
    .build();

const RuleEffectEntry Purge_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_MODIFIERS, purge_effect)
    .condition(CombatSubPhase::HIT_MODIFIERS, purge_condition)
    .build();

const RuleEffectEntry Thrust_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_MODIFIERS, thrust_hit_effect)
    .condition(CombatSubPhase::HIT_MODIFIERS, thrust_condition)
    .build();

const RuleEffectEntry PointBlankSurge_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_BONUSES, point_blank_surge_effect)
    .condition(CombatSubPhase::HIT_BONUSES, point_blank_surge_condition)
    .build();

const RuleEffectEntry Furious_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_BONUSES, furious_effect)
    .condition(CombatSubPhase::HIT_BONUSES, furious_condition)
    .build();

const RuleEffectEntry Rupture_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_SEPARATION, rupture_effect)
    .build();

// ==============================================================================
// Movement Phase Effects
// ==============================================================================

void fast_effect(MovementContext& ctx, u8 /*value*/) {
    // Fast units move 9" instead of 6"
    ctx.distance_modifier += 3;
}

void slow_effect(MovementContext& ctx, u8 /*value*/) {
    // Slow units move 4" instead of 6"
    ctx.distance_modifier -= 2;
}

void flying_effect(MovementContext& ctx, u8 /*value*/) {
    // Flying units ignore all terrain and can leave engagement
    ctx.terrain_flags |= MovementContext::IGNORE_DIFFICULT;
    ctx.terrain_flags |= MovementContext::IGNORE_DANGEROUS;
    ctx.terrain_flags |= MovementContext::FLY_OVER;
    ctx.ignores_engagement = true;
}

void strider_effect(MovementContext& ctx, u8 /*value*/) {
    // Strider ignores difficult terrain
    ctx.terrain_flags |= MovementContext::IGNORE_DIFFICULT;
}

void agile_effect(MovementContext& ctx, u8 /*value*/) {
    // Agile gives +1" advance, +2" rush/charge
    if (ctx.move_type == MovementContext::MoveType::ADVANCE) {
        ctx.distance_modifier += 1;
    } else if (ctx.move_type == MovementContext::MoveType::RUSH ||
               ctx.move_type == MovementContext::MoveType::CHARGE) {
        ctx.distance_modifier += 2;
    }
}

void rapid_charge_effect(MovementContext& ctx, u8 /*value*/) {
    // Rapid Charge gives +4" to charge moves
    ctx.charge_bonus += 4;
}

void hit_and_run_effect(MovementContext& ctx, u8 /*value*/) {
    // Hit and Run allows retreat after combat
    ctx.hit_and_run_pending = true;
}

// ==============================================================================
// Deployment Phase Effects
// ==============================================================================

void scout_effect(DeploymentContext& ctx, u8 /*value*/) {
    // Scout allows 12" forward deployment
    ctx.zone = DeploymentContext::DeployZone::FORWARD;
    ctx.timing = DeploymentContext::DeployTiming::SCOUT_MOVE;
    ctx.forward_distance = 12;
}

void ambush_effect(DeploymentContext& ctx, u8 /*value*/) {
    // Ambush allows deploying anywhere >9" from enemy
    ctx.zone = DeploymentContext::DeployZone::ANYWHERE;
    ctx.timing = DeploymentContext::DeployTiming::NORMAL;
    ctx.min_enemy_distance = 9;
}

// ==============================================================================
// End Round Phase Effects
// ==============================================================================

void fearless_morale_effect(EndRoundContext& /*ctx*/, Unit& unit, u8 /*value*/) {
    // Fearless allows reroll of failed morale test
    // Find or create morale state for this unit
    // Note: In a real implementation, this would modify morale_states
    (void)unit;  // Placeholder - actual implementation depends on morale system
}

void no_retreat_effect(EndRoundContext& /*ctx*/, Unit& unit, u8 /*value*/) {
    // No Retreat - can't be shaken/routed, take wounds instead
    // This is typically handled during morale resolution
    (void)unit;  // Placeholder
}

void morale_boost_effect(EndRoundContext& /*ctx*/, Unit& unit, u8 /*value*/) {
    // Morale Boost - +1 to morale test rolls
    (void)unit;  // Placeholder
}

void hold_the_line_effect(EndRoundContext& /*ctx*/, Unit& unit, u8 /*value*/) {
    // Hold the Line - reroll failed morale tests
    (void)unit;  // Placeholder
}

void regeneration_heal_effect(EndRoundContext& /*ctx*/, Unit& unit, u8 /*value*/) {
    // Regeneration - heal wounded models at end of round (5+ per model)
    (void)unit;  // Placeholder - actual implementation needs dice roller
}

void battleborn_effect(EndRoundContext& /*ctx*/, Unit& unit, u8 /*value*/) {
    // Battleborn - 4+ to stop being Shaken at round start
    (void)unit;  // Placeholder
}

// ==============================================================================
// Movement Hot Data Definitions
// ==============================================================================

const RuleHotData Fast_HotData{
    RuleId::Fast,
    GamePhase::MOVEMENT,
    static_cast<u8>(MoveSubPhase::CALCULATE_DISTANCE),
    CombatType::BOTH,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    0
};

const RuleHotData Slow_HotData{
    RuleId::Slow,
    GamePhase::MOVEMENT,
    static_cast<u8>(MoveSubPhase::CALCULATE_DISTANCE),
    CombatType::BOTH,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    0
};

const RuleHotData Flying_HotData{
    RuleId::Flying,
    GamePhase::MOVEMENT,
    static_cast<u8>(MoveSubPhase::EXECUTE_MOVE),
    CombatType::BOTH,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    0
};

const RuleHotData Strider_HotData{
    RuleId::Strider,
    GamePhase::MOVEMENT,
    static_cast<u8>(MoveSubPhase::EXECUTE_MOVE),
    CombatType::BOTH,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    0
};

const RuleHotData Agile_HotData{
    RuleId::Agile,
    GamePhase::MOVEMENT,
    static_cast<u8>(MoveSubPhase::CALCULATE_DISTANCE),
    CombatType::BOTH,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    0
};

const RuleHotData RapidCharge_HotData{
    RuleId::RapidCharge,
    GamePhase::MOVEMENT,
    static_cast<u8>(MoveSubPhase::CHARGE_RESOLVE),
    CombatType::BOTH,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    0
};

const RuleHotData HitAndRun_HotData{
    RuleId::HitAndRun,
    GamePhase::MOVEMENT,
    static_cast<u8>(MoveSubPhase::POST_MOVE),
    CombatType::MELEE,
    Target::SELF,
    Trigger::AFTER_COMBAT,
    RulePriority::NORMAL,
    0
};

// ==============================================================================
// Deployment Hot Data Definitions
// ==============================================================================

const RuleHotData Scout_HotData{
    RuleId::Scout,
    GamePhase::DEPLOYMENT,
    static_cast<u8>(DeploySubPhase::SCOUT_MOVE),
    CombatType::BOTH,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    0
};

const RuleHotData Ambush_HotData{
    RuleId::Ambush,
    GamePhase::DEPLOYMENT,
    static_cast<u8>(DeploySubPhase::INFILTRATE),
    CombatType::BOTH,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    0
};

// ==============================================================================
// End Round Hot Data Definitions
// ==============================================================================

const RuleHotData Fearless_HotData{
    RuleId::Fearless,
    GamePhase::END_ROUND,
    static_cast<u8>(EndRoundSubPhase::MORALE),
    CombatType::BOTH,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::AFFECTS_MORALE)
};

const RuleHotData NoRetreat_HotData{
    RuleId::NoRetreat,
    GamePhase::END_ROUND,
    static_cast<u8>(EndRoundSubPhase::MORALE),
    CombatType::BOTH,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::EARLY,  // Process before other morale effects
    static_cast<TraitMask>(RuleTrait::AFFECTS_MORALE)
};

const RuleHotData MoraleBoost_HotData{
    RuleId::MoraleBoost,
    GamePhase::END_ROUND,
    static_cast<u8>(EndRoundSubPhase::MORALE),
    CombatType::BOTH,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::AFFECTS_MORALE)
};

const RuleHotData HoldTheLine_HotData{
    RuleId::HoldTheLine,
    GamePhase::END_ROUND,
    static_cast<u8>(EndRoundSubPhase::MORALE),
    CombatType::BOTH,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::AFFECTS_MORALE)
};

const RuleHotData Regeneration_HotData{
    RuleId::Regeneration,
    GamePhase::END_ROUND,
    static_cast<u8>(EndRoundSubPhase::REGENERATION),
    CombatType::BOTH,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::HEALS_WOUNDS)
};

const RuleHotData Battleborn_HotData{
    RuleId::Battleborn,
    GamePhase::END_ROUND,
    static_cast<u8>(EndRoundSubPhase::MORALE),
    CombatType::BOTH,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::FIRST,  // Process very early
    static_cast<TraitMask>(RuleTrait::AFFECTS_MORALE)
};

// ==============================================================================
// Movement Effect Entries
// ==============================================================================

const RuleEffectEntry Fast_Effects = EffectBuilder()
    .movement(MoveSubPhase::CALCULATE_DISTANCE, fast_effect)
    .build();

const RuleEffectEntry Slow_Effects = EffectBuilder()
    .movement(MoveSubPhase::CALCULATE_DISTANCE, slow_effect)
    .build();

const RuleEffectEntry Flying_Effects = EffectBuilder()
    .movement(MoveSubPhase::EXECUTE_MOVE, flying_effect)
    .build();

const RuleEffectEntry Strider_Effects = EffectBuilder()
    .movement(MoveSubPhase::EXECUTE_MOVE, strider_effect)
    .build();

const RuleEffectEntry Agile_Effects = EffectBuilder()
    .movement(MoveSubPhase::CALCULATE_DISTANCE, agile_effect)
    .build();

const RuleEffectEntry RapidCharge_Effects = EffectBuilder()
    .movement(MoveSubPhase::CHARGE_RESOLVE, rapid_charge_effect)
    .build();

const RuleEffectEntry HitAndRun_Effects = EffectBuilder()
    .movement(MoveSubPhase::POST_MOVE, hit_and_run_effect)
    .build();

// ==============================================================================
// Deployment Effect Entries
// ==============================================================================

const RuleEffectEntry Scout_Effects = EffectBuilder()
    .deploy(DeploySubPhase::SCOUT_MOVE, scout_effect)
    .build();

const RuleEffectEntry Ambush_Effects = EffectBuilder()
    .deploy(DeploySubPhase::INFILTRATE, ambush_effect)
    .build();

// ==============================================================================
// End Round Effect Entries
// ==============================================================================

const RuleEffectEntry Fearless_Effects = EffectBuilder()
    .endround(EndRoundSubPhase::MORALE, fearless_morale_effect)
    .build();

const RuleEffectEntry NoRetreat_Effects = EffectBuilder()
    .endround(EndRoundSubPhase::MORALE, no_retreat_effect)
    .build();

const RuleEffectEntry MoraleBoost_Effects = EffectBuilder()
    .endround(EndRoundSubPhase::MORALE, morale_boost_effect)
    .build();

const RuleEffectEntry HoldTheLine_Effects = EffectBuilder()
    .endround(EndRoundSubPhase::MORALE, hold_the_line_effect)
    .build();

const RuleEffectEntry Regeneration_EndRound_Effects = EffectBuilder()
    .endround(EndRoundSubPhase::REGENERATION, regeneration_heal_effect)
    .build();

const RuleEffectEntry Battleborn_Effects = EffectBuilder()
    .endround(EndRoundSubPhase::MORALE, battleborn_effect)
    .build();

// ==============================================================================
// Cold Data for Movement Rules
// ==============================================================================

const RuleColdData Fast_ColdData{
    "Fast",
    nullptr,
    0,
    "Fast: +3\" movement",
    "This unit moves 9\" instead of 6\"."
};

const RuleColdData Slow_ColdData{
    "Slow",
    nullptr,
    0,
    "Slow: -2\" movement",
    "This unit moves 4\" instead of 6\"."
};

const RuleColdData Flying_ColdData{
    "Flying",
    nullptr,
    0,
    "Flying: ignores terrain",
    "This unit can fly over terrain and other units."
};

const RuleColdData Strider_ColdData{
    "Strider",
    nullptr,
    0,
    "Strider: ignores difficult terrain",
    "This unit ignores difficult terrain penalties."
};

const RuleColdData Agile_ColdData{
    "Agile",
    nullptr,
    0,
    "Agile: +1\" advance, +2\" rush/charge",
    "This unit gets bonus movement when advancing, rushing, or charging."
};

const RuleColdData RapidCharge_ColdData{
    "RapidCharge",
    nullptr,
    0,
    "Rapid Charge: +4\" charge",
    "This unit adds 4\" to its charge range."
};

const RuleColdData HitAndRun_ColdData{
    "HitAndRun",
    nullptr,
    0,
    "Hit and Run: retreat after combat",
    "This unit can retreat after fighting in melee."
};

// ==============================================================================
// Cold Data for Deployment Rules
// ==============================================================================

const RuleColdData Scout_ColdData{
    "Scout",
    nullptr,
    0,
    "Scout: 12\" forward deploy",
    "This unit can deploy up to 12\" forward of the deployment zone."
};

const RuleColdData Ambush_ColdData{
    "Ambush",
    nullptr,
    0,
    "Ambush: deploy anywhere",
    "This unit can deploy anywhere more than 9\" from enemy units."
};

// ==============================================================================
// Cold Data for End Round Rules
// ==============================================================================

const RuleColdData Fearless_ColdData{
    "Fearless",
    nullptr,
    0,
    "Fearless: reroll morale",
    "This unit can reroll failed morale tests."
};

const RuleColdData NoRetreat_ColdData{
    "NoRetreat",
    nullptr,
    0,
    "No Retreat: never flee",
    "This unit can't be shaken or routed, but takes extra wounds instead."
};

const RuleColdData MoraleBoost_ColdData{
    "MoraleBoost",
    nullptr,
    0,
    "Morale Boost: +1 morale",
    "This unit gets +1 to morale test rolls."
};

const RuleColdData HoldTheLine_ColdData{
    "HoldTheLine",
    nullptr,
    0,
    "Hold the Line: reroll morale",
    "This unit can reroll failed morale tests."
};

const RuleColdData Regeneration_ColdData{
    "Regeneration",
    nullptr,
    0,
    "Regeneration: heal wounds",
    "At the end of each round, roll 5+ for each wounded model to heal a wound."
};

const RuleColdData Battleborn_ColdData{
    "Battleborn",
    nullptr,
    0,
    "Battleborn: rally on 4+",
    "At the start of the round, roll 4+ to remove Shaken status."
};

// ==============================================================================
// Cold Data for Hit Modifier Rules
// ==============================================================================

const RuleColdData GoodShot_ColdData{
    "GoodShot",
    nullptr,
    0,
    "GoodShot: +1 to hit",
    "This unit gets +1 to hit when shooting."
};

const RuleColdData BadShot_ColdData{
    "BadShot",
    nullptr,
    0,
    "BadShot: -1 to hit",
    "This unit gets -1 to hit when shooting."
};

const RuleColdData RangedShrouding_ColdData{
    "RangedShrouding",
    nullptr,
    0,
    "RangedShrouding: -1 to be hit",
    "Enemies shooting this unit get -1 to hit."
};

const RuleColdData MeleeEvasion_ColdData{
    "MeleeEvasion",
    nullptr,
    0,
    "MeleeEvasion: -1 to be hit",
    "Enemies attacking this unit in melee get -1 to hit."
};

const RuleColdData MeleeShrouding_ColdData{
    "MeleeShrouding",
    nullptr,
    0,
    "MeleeShrouding: -1 to be hit",
    "Enemies attacking this unit in melee get -1 to hit."
};

const RuleColdData Purge_ColdData{
    "Purge",
    nullptr,
    0,
    "Purge: +1 to hit vs Tough(3+)",
    "Weapons with this rule get +1 to hit against targets with Tough(3+)."
};

const RuleColdData Thrust_ColdData{
    "Thrust",
    nullptr,
    0,
    "Thrust: +1 to hit when charging",
    "Weapons with this rule get +1 to hit and AP+1 when charging."
};

// ==============================================================================
// Registration Function
// ==============================================================================

void register_combat_rules(RuleRegistry& registry) {
    // =========================================================================
    // Register Combat hot data
    // =========================================================================
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

    // =========================================================================
    // Register Movement hot data
    // =========================================================================
    registry.register_hot_data(RuleId::Fast, Fast_HotData);
    registry.register_hot_data(RuleId::Slow, Slow_HotData);
    registry.register_hot_data(RuleId::Flying, Flying_HotData);
    registry.register_hot_data(RuleId::Strider, Strider_HotData);
    registry.register_hot_data(RuleId::Agile, Agile_HotData);
    registry.register_hot_data(RuleId::RapidCharge, RapidCharge_HotData);
    registry.register_hot_data(RuleId::HitAndRun, HitAndRun_HotData);

    // =========================================================================
    // Register Deployment hot data
    // =========================================================================
    registry.register_hot_data(RuleId::Scout, Scout_HotData);
    registry.register_hot_data(RuleId::Ambush, Ambush_HotData);

    // =========================================================================
    // Register End Round hot data
    // =========================================================================
    registry.register_hot_data(RuleId::Fearless, Fearless_HotData);
    registry.register_hot_data(RuleId::NoRetreat, NoRetreat_HotData);
    registry.register_hot_data(RuleId::MoraleBoost, MoraleBoost_HotData);
    registry.register_hot_data(RuleId::HoldTheLine, HoldTheLine_HotData);
    registry.register_hot_data(RuleId::Regeneration, Regeneration_HotData);
    registry.register_hot_data(RuleId::Battleborn, Battleborn_HotData);

    // =========================================================================
    // Register Combat cold data
    // =========================================================================
    registry.register_cold_data(RuleId::Precise, Precise_ColdData);
    registry.register_cold_data(RuleId::Reliable, Reliable_ColdData);
    registry.register_cold_data(RuleId::Rending, Rending_ColdData);
    registry.register_cold_data(RuleId::Blast, Blast_ColdData);
    registry.register_cold_data(RuleId::Stealth, Stealth_ColdData);
    registry.register_cold_data(RuleId::GoodShot, GoodShot_ColdData);
    registry.register_cold_data(RuleId::BadShot, BadShot_ColdData);
    registry.register_cold_data(RuleId::RangedShrouding, RangedShrouding_ColdData);
    registry.register_cold_data(RuleId::MeleeEvasion, MeleeEvasion_ColdData);
    registry.register_cold_data(RuleId::MeleeShrouding, MeleeShrouding_ColdData);
    registry.register_cold_data(RuleId::Purge, Purge_ColdData);
    registry.register_cold_data(RuleId::Thrust, Thrust_ColdData);

    // =========================================================================
    // Register Movement cold data
    // =========================================================================
    registry.register_cold_data(RuleId::Fast, Fast_ColdData);
    registry.register_cold_data(RuleId::Slow, Slow_ColdData);
    registry.register_cold_data(RuleId::Flying, Flying_ColdData);
    registry.register_cold_data(RuleId::Strider, Strider_ColdData);
    registry.register_cold_data(RuleId::Agile, Agile_ColdData);
    registry.register_cold_data(RuleId::RapidCharge, RapidCharge_ColdData);
    registry.register_cold_data(RuleId::HitAndRun, HitAndRun_ColdData);

    // =========================================================================
    // Register Deployment cold data
    // =========================================================================
    registry.register_cold_data(RuleId::Scout, Scout_ColdData);
    registry.register_cold_data(RuleId::Ambush, Ambush_ColdData);

    // =========================================================================
    // Register End Round cold data
    // =========================================================================
    registry.register_cold_data(RuleId::Fearless, Fearless_ColdData);
    registry.register_cold_data(RuleId::NoRetreat, NoRetreat_ColdData);
    registry.register_cold_data(RuleId::MoraleBoost, MoraleBoost_ColdData);
    registry.register_cold_data(RuleId::HoldTheLine, HoldTheLine_ColdData);
    registry.register_cold_data(RuleId::Regeneration, Regeneration_ColdData);
    registry.register_cold_data(RuleId::Battleborn, Battleborn_ColdData);

    // =========================================================================
    // Register Combat effects
    // =========================================================================
    registry.register_effects(RuleId::Precise, Precise_Effects);
    registry.register_effects(RuleId::Reliable, Reliable_Effects);
    registry.register_effects(RuleId::Rending, Rending_Effects);
    registry.register_effects(RuleId::Stealth, Stealth_Effects);
    registry.register_effects(RuleId::Relentless, Relentless_Effects);
    registry.register_effects(RuleId::Surge, Surge_Effects);
    registry.register_effects(RuleId::Blast, Blast_Effects);
    registry.register_effects(RuleId::GoodShot, GoodShot_Effects);
    registry.register_effects(RuleId::BadShot, BadShot_Effects);
    registry.register_effects(RuleId::RangedShrouding, RangedShrouding_Effects);
    registry.register_effects(RuleId::MeleeEvasion, MeleeEvasion_Effects);
    registry.register_effects(RuleId::MeleeShrouding, MeleeShrouding_Effects);
    registry.register_effects(RuleId::Purge, Purge_Effects);
    registry.register_effects(RuleId::Thrust, Thrust_Effects);
    registry.register_effects(RuleId::PointBlankSurge, PointBlankSurge_Effects);
    registry.register_effects(RuleId::Furious, Furious_Effects);
    registry.register_effects(RuleId::Rupture, Rupture_Effects);

    // =========================================================================
    // Register Movement effects
    // =========================================================================
    registry.register_effects(RuleId::Fast, Fast_Effects);
    registry.register_effects(RuleId::Slow, Slow_Effects);
    registry.register_effects(RuleId::Flying, Flying_Effects);
    registry.register_effects(RuleId::Strider, Strider_Effects);
    registry.register_effects(RuleId::Agile, Agile_Effects);
    registry.register_effects(RuleId::RapidCharge, RapidCharge_Effects);
    registry.register_effects(RuleId::HitAndRun, HitAndRun_Effects);

    // =========================================================================
    // Register Deployment effects
    // =========================================================================
    registry.register_effects(RuleId::Scout, Scout_Effects);
    registry.register_effects(RuleId::Ambush, Ambush_Effects);

    // =========================================================================
    // Register End Round effects
    // =========================================================================
    registry.register_effects(RuleId::Fearless, Fearless_Effects);
    registry.register_effects(RuleId::NoRetreat, NoRetreat_Effects);
    registry.register_effects(RuleId::MoraleBoost, MoraleBoost_Effects);
    registry.register_effects(RuleId::HoldTheLine, HoldTheLine_Effects);
    registry.register_effects(RuleId::Regeneration, Regeneration_EndRound_Effects);
    registry.register_effects(RuleId::Battleborn, Battleborn_Effects);

    // =========================================================================
    // Register Combat aliases for parsing
    // =========================================================================
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
    registry.register_alias("goodshot", RuleId::GoodShot);
    registry.register_alias("GoodShot", RuleId::GoodShot);
    registry.register_alias("badshot", RuleId::BadShot);
    registry.register_alias("BadShot", RuleId::BadShot);
    registry.register_alias("rangedshrouding", RuleId::RangedShrouding);
    registry.register_alias("RangedShrouding", RuleId::RangedShrouding);
    registry.register_alias("meleeevasion", RuleId::MeleeEvasion);
    registry.register_alias("MeleeEvasion", RuleId::MeleeEvasion);
    registry.register_alias("meleeshrouding", RuleId::MeleeShrouding);
    registry.register_alias("MeleeShrouding", RuleId::MeleeShrouding);
    registry.register_alias("purge", RuleId::Purge);
    registry.register_alias("Purge", RuleId::Purge);
    registry.register_alias("thrust", RuleId::Thrust);
    registry.register_alias("Thrust", RuleId::Thrust);

    // =========================================================================
    // Register Movement aliases for parsing
    // =========================================================================
    registry.register_alias("fast", RuleId::Fast);
    registry.register_alias("Fast", RuleId::Fast);
    registry.register_alias("slow", RuleId::Slow);
    registry.register_alias("Slow", RuleId::Slow);
    registry.register_alias("flying", RuleId::Flying);
    registry.register_alias("Flying", RuleId::Flying);
    registry.register_alias("strider", RuleId::Strider);
    registry.register_alias("Strider", RuleId::Strider);
    registry.register_alias("agile", RuleId::Agile);
    registry.register_alias("Agile", RuleId::Agile);
    registry.register_alias("rapidcharge", RuleId::RapidCharge);
    registry.register_alias("RapidCharge", RuleId::RapidCharge);
    registry.register_alias("hitandrun", RuleId::HitAndRun);
    registry.register_alias("HitAndRun", RuleId::HitAndRun);

    // =========================================================================
    // Register Deployment aliases for parsing
    // =========================================================================
    registry.register_alias("scout", RuleId::Scout);
    registry.register_alias("Scout", RuleId::Scout);
    registry.register_alias("ambush", RuleId::Ambush);
    registry.register_alias("Ambush", RuleId::Ambush);

    // =========================================================================
    // Register End Round aliases for parsing
    // =========================================================================
    registry.register_alias("fearless", RuleId::Fearless);
    registry.register_alias("Fearless", RuleId::Fearless);
    registry.register_alias("noretreat", RuleId::NoRetreat);
    registry.register_alias("NoRetreat", RuleId::NoRetreat);
    registry.register_alias("moraleboost", RuleId::MoraleBoost);
    registry.register_alias("MoraleBoost", RuleId::MoraleBoost);
    registry.register_alias("holdtheline", RuleId::HoldTheLine);
    registry.register_alias("HoldTheLine", RuleId::HoldTheLine);
    registry.register_alias("regeneration", RuleId::Regeneration);
    registry.register_alias("Regeneration", RuleId::Regeneration);
    registry.register_alias("battleborn", RuleId::Battleborn);
    registry.register_alias("Battleborn", RuleId::Battleborn);
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
