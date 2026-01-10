#include "rules/combat_rules.hpp"
#include "core/rule_registry.hpp"
#include "core/contexts.hpp"
#include "core/unit.hpp"
#include "core/weapon.hpp"
#include "engine/dice.hpp"
#include "engine/game_state.hpp"
#include "engine/match_logger.hpp"

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

// === DEFENSE_RESOLUTION Phase Effects ===

void ap_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 value) {
    // AP(X) adds X to armor piercing
    ctx.ap_modifier += value;
}

void poison_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Poison forces defender to reroll successful defense rolls of 6
    ctx.set_force_reroll(true);
}

void bane_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Bane bypasses regeneration and forces defense reroll on 6s
    ctx.set_bypass_regen(true);
    ctx.set_force_reroll(true);
}

void shielded_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Shielded gives +1 defense vs non-spell hits
    ctx.defense_modifier += 1;
}

void shield_wall_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // ShieldWall gives +1 Defense in melee
    ctx.defense_modifier += 1;
}

bool shield_wall_condition(const CombatContextCore& ctx) {
    return ctx.combat_type == CombatType::MELEE;
}

void protected_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 /*value*/) {
    // Protected - roll 6+ to reduce AP by 1
    // This is handled during defense resolution - flag for later
    if (ext) {
        ext->protected_active = true;
    }
}

void resistance_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 /*value*/) {
    // Resistance - 6+ to ignore each wound (after regeneration)
    if (ext) {
        ext->resistance_active = true;
    }
}

void lance_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Lance gives +2 AP when charging
    ctx.ap_modifier += 2;
}

bool lance_condition(const CombatContextCore& ctx) {
    return ctx.is_charge;
}

void thrust_ap_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Thrust gives +1 AP when charging (in addition to +1 hit from HIT_MODIFIERS)
    ctx.ap_modifier += 1;
}

void piercing_assault_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Piercing Assault gives minimum AP(1) when charging in melee
    if (ctx.ap_modifier < 1) {
        ctx.ap_modifier = 1;
    }
}

bool piercing_assault_condition(const CombatContextCore& ctx) {
    return ctx.combat_type == CombatType::MELEE && ctx.is_charge;
}

void bane_in_melee_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // BaneInMelee - all melee attacks bypass regeneration and force reroll
    ctx.set_bypass_regen(true);
    ctx.set_force_reroll(true);
}

bool bane_in_melee_condition(const CombatContextCore& ctx) {
    return ctx.combat_type == CombatType::MELEE;
}

// === WOUND_ALLOCATION Phase Effects ===

void deadly_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 value) {
    // Deadly(X) multiplies each wound dealt by X
    if (ext) {
        ext->wound_multiplier = value;
    }
}

void tough_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 value) {
    // Tough(X) - model requires X wounds to kill instead of 1
    if (ext) {
        ext->tough_value = value;
    }
}

void hero_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 /*value*/) {
    // Hero - takes wounds last in the unit
    if (ext) {
        ext->hero_present = true;
    }
}

void self_destruct_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 value) {
    // SelfDestruct(X) - when killed in melee, deal X hits back to attacker
    if (ext && ctx.combat_type == CombatType::MELEE) {
        ext->self_destruct_value = value;
    }
}

void shred_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 /*value*/) {
    // Shred - extra wound on unmodified 1 to block
    // Tracked during defense resolution
    if (ext) {
        ext->shred_active = true;
    }
}

void smash_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Smash - ignore regen, +Blast(3) vs Defense 5+/6+
    ctx.set_bypass_regen(true);
    if (ext && ctx.defender && ctx.defender->defense >= 5) {
        ext->smash_bonus_blast = 3;
    }
}

void takedown_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 /*value*/) {
    // Takedown - pick target model, resolve as unit of 1
    if (ext) {
        ext->takedown_active = true;
    }
}

void unstoppable_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Unstoppable - ignore regen and negative modifiers to this weapon
    ctx.set_bypass_regen(true);
    if (ext) {
        ext->ignores_negative_hit_mods = true;
    }
}

// === PRE_ATTACK Phase Effects ===

void limited_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 /*value*/) {
    // Limited - weapon can only be used once per game
    // Check is handled externally; this just marks it as limited type
    if (ext) {
        ext->limited_weapon = true;
    }
}

void versatile_attack_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // VersatileAttack - roll d6: 1-3 = AP+1, 4-6 = +1 to hit
    if (ext && ext->dice) {
        u8 roll = ext->dice->roll_d6();
        if (roll <= 3) {
            // AP+1 bonus - tracked for later application
            ext->versatile_ap_chosen = true;
            if (ext->logger) {
                ext->logger->on_rule_triggered("VersatileAttack", "rolled_ap+1", roll);
            }
        } else {
            // +1 hit bonus - applied immediately
            ctx.hit_modifier += 1;
            ext->versatile_ap_chosen = false;
            if (ext->logger) {
                ext->logger->on_hit_modifier("VersatileAttack", +1, "rolled_+1_hit");
            }
        }
    }
}

void impact_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 value) {
    // Impact(X) - generate X extra attacks when charging
    if (ext) {
        ext->impact_attacks = value;
    }
}

bool impact_condition(const CombatContextCore& ctx) {
    return ctx.is_charge;
}

void counter_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 /*value*/) {
    // Counter - strikes first when charged
    if (ext) {
        ext->counter_active = true;
    }
}

// === Targeting Effects (PRE_ATTACK phase) ===

void sniper_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 /*value*/) {
    // Sniper - can pick target model
    if (ext) {
        ext->sniper_active = true;
    }
}

void indirect_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Indirect - ignore cover, but -1 to hit if moved
    if (ext) {
        ext->ignores_cover = true;
        // Apply -1 to hit penalty when shooting after moving
        if (ext->moved_this_activation) {
            ctx.hit_modifier -= 1;
        }
    }
}

void lock_on_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Lock-On - +1 to hit vs vehicles
    ctx.hit_modifier += 1;
}

bool lock_on_condition(const CombatContextCore& ctx) {
    // Lock-On applies vs units with the Vehicle rule
    // For now, always return false (effect applies when manually enabled)
    // TODO: Add vehicle detection when Vehicle rule/tag is implemented
    (void)ctx;
    return false;  // Disabled until vehicle tag support is added
}

// === HIT_BONUSES Additional Effects ===

void predator_fighter_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // PredatorFighter - recursive extra attacks on 6s in melee
    u32 bonus = ctx.natural_sixes;
    ctx.hits += bonus;
    if (ext) {
        ext->bonus_hits += bonus;
        ext->predator_fighter_active = true;
    }
}

bool predator_fighter_condition(const CombatContextCore& ctx) {
    return ctx.combat_type == CombatType::MELEE;
}

// === END_ROUND Phase Effects ===

void fear_effect(EndRoundContext& /*ctx*/, Unit& /*unit*/, u8 /*value*/) {
    // Fear(X) - counts as X extra wounds for morale purposes
    // Actual implementation in morale system
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

// === Additional Combat Hot Data ===

const RuleHotData AP_HotData{
    RuleId::AP,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP) |
    static_cast<TraitMask>(RuleTrait::HAS_VALUE)
};

const RuleHotData Lance_HotData{
    RuleId::Lance,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::MELEE,
    Target::WEAPON,
    Trigger::ON_CHARGE,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP) |
    static_cast<TraitMask>(RuleTrait::CHARGE_ONLY) |
    static_cast<TraitMask>(RuleTrait::MELEE_ONLY)
};

const RuleHotData Impact_HotData{
    RuleId::Impact,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::PRE_ATTACK),
    CombatType::MELEE,
    Target::ATTACKER,
    Trigger::ON_CHARGE,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS) |
    static_cast<TraitMask>(RuleTrait::CHARGE_ONLY) |
    static_cast<TraitMask>(RuleTrait::HAS_VALUE)
};

const RuleHotData Shielded_HotData{
    RuleId::Shielded,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::DEFENDER,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MODIFIES_DEFENSE)
};

const RuleHotData ShieldWall_HotData{
    RuleId::ShieldWall,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::MELEE,
    Target::DEFENDER,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MODIFIES_DEFENSE) |
    static_cast<TraitMask>(RuleTrait::MELEE_ONLY)
};

const RuleHotData Protected_HotData{
    RuleId::Protected,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::DEFENDER,
    Trigger::ALWAYS,
    RulePriority::EARLY,
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP)
};

const RuleHotData Resistance_HotData{
    RuleId::Resistance,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::WOUND_ALLOCATION),
    CombatType::BOTH,
    Target::DEFENDER,
    Trigger::ALWAYS,
    RulePriority::LATE,
    0
};

const RuleHotData Tough_HotData{
    RuleId::Tough,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::WOUND_ALLOCATION),
    CombatType::BOTH,
    Target::DEFENDER,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::HAS_VALUE)
};

const RuleHotData Hero_HotData{
    RuleId::Hero,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::WOUND_ALLOCATION),
    CombatType::BOTH,
    Target::DEFENDER,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    0
};

const RuleHotData Takedown_HotData{
    RuleId::Takedown,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::PRE_ATTACK),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::PICK_TARGET_MODEL)
};

const RuleHotData SelfDestruct_HotData{
    RuleId::SelfDestruct,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::WOUND_ALLOCATION),
    CombatType::MELEE,
    Target::DEFENDER,
    Trigger::ON_MODEL_DEATH,
    RulePriority::LAST,
    static_cast<TraitMask>(RuleTrait::ON_DEATH_EFFECT) |
    static_cast<TraitMask>(RuleTrait::MELEE_ONLY) |
    static_cast<TraitMask>(RuleTrait::HAS_VALUE)
};

const RuleHotData PredatorFighter_HotData{
    RuleId::PredatorFighter,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_BONUSES),
    CombatType::MELEE,
    Target::ATTACKER,
    Trigger::ON_HIT_ROLL_6,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS) |
    static_cast<TraitMask>(RuleTrait::MELEE_ONLY)
};

const RuleHotData Counter_HotData{
    RuleId::Counter,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::PRE_ATTACK),
    CombatType::MELEE,
    Target::DEFENDER,
    Trigger::ALWAYS,
    RulePriority::FIRST,
    static_cast<TraitMask>(RuleTrait::MELEE_ONLY)
};

const RuleHotData Sniper_HotData{
    RuleId::Sniper,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::PRE_ATTACK),
    CombatType::SHOOTING,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::PICK_TARGET_MODEL) |
    static_cast<TraitMask>(RuleTrait::RANGED_ONLY)
};

const RuleHotData Indirect_HotData{
    RuleId::Indirect,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::PRE_ATTACK),
    CombatType::SHOOTING,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::IGNORES_COVER) |
    static_cast<TraitMask>(RuleTrait::RANGED_ONLY)
};

const RuleHotData Lock_On_HotData{
    RuleId::Lock_On,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_MODIFIERS),
    CombatType::SHOOTING,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL) |
    static_cast<TraitMask>(RuleTrait::RANGED_ONLY)
};

const RuleHotData Shred_HotData{
    RuleId::Shred,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_WOUNDS) |
    static_cast<TraitMask>(RuleTrait::BYPASSES_REGENERATION)
};

const RuleHotData Smash_HotData{
    RuleId::Smash,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::BYPASSES_REGENERATION) |
    static_cast<TraitMask>(RuleTrait::MULTIPLIES_HITS)
};

const RuleHotData VersatileAttack_HotData{
    RuleId::VersatileAttack,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::PRE_ATTACK),
    CombatType::BOTH,
    Target::ATTACKER,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL) |
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP)
};

const RuleHotData Limited_HotData{
    RuleId::Limited,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::PRE_ATTACK),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::FIRST,
    0
};

const RuleHotData BaneInMelee_HotData{
    RuleId::BaneInMelee,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::MELEE,
    Target::ATTACKER,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::BYPASSES_REGENERATION) |
    static_cast<TraitMask>(RuleTrait::FORCES_DEFENSE_REROLL) |
    static_cast<TraitMask>(RuleTrait::MELEE_ONLY)
};

const RuleHotData Fear_HotData{
    RuleId::Fear,
    GamePhase::END_ROUND,
    static_cast<u8>(EndRoundSubPhase::MORALE),
    CombatType::BOTH,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::AFFECTS_MORALE) |
    static_cast<TraitMask>(RuleTrait::HAS_VALUE)
};

const RuleHotData Unstoppable_HotData{
    RuleId::Unstoppable,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::BYPASSES_REGENERATION)
};

const RuleHotData PiercingAssault_HotData{
    RuleId::PiercingAssault,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::MELEE,
    Target::WEAPON,
    Trigger::ON_CHARGE,
    RulePriority::NORMAL,
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

// === Additional Combat Cold Data ===

const RuleColdData Deadly_ColdData{
    "Deadly",
    nullptr,
    0,
    "Deadly({}): multiply wounds",
    "Each wound dealt becomes X wounds (no carry-over between models)."
};

const RuleColdData Poison_ColdData{
    "Poison",
    nullptr,
    0,
    "Poison: reroll defense 6s",
    "Defender must reroll successful defense rolls of 6."
};

const RuleColdData Bane_ColdData{
    "Bane",
    nullptr,
    0,
    "Bane: bypass regen, reroll 6s",
    "Bypasses regeneration and forces defender to reroll defense 6s."
};

const RuleColdData AP_ColdData{
    "AP",
    nullptr,
    0,
    "AP({}): armor piercing",
    "Attacks with this weapon have Armor Piercing X."
};

const RuleColdData Lance_ColdData{
    "Lance",
    nullptr,
    0,
    "Lance: +2 AP on charge",
    "When charging, attacks gain +2 AP."
};

const RuleColdData Impact_ColdData{
    "Impact",
    nullptr,
    0,
    "Impact({}): extra charge attacks",
    "Generate X extra attacks when charging."
};

const RuleColdData Shielded_ColdData{
    "Shielded",
    nullptr,
    0,
    "Shielded: +1 Defense",
    "This unit gets +1 Defense against non-spell attacks."
};

const RuleColdData ShieldWall_ColdData{
    "ShieldWall",
    nullptr,
    0,
    "Shield Wall: +1 Defense in melee",
    "This unit gets +1 Defense in melee combat."
};

const RuleColdData Protected_ColdData{
    "Protected",
    nullptr,
    0,
    "Protected: 6+ to reduce AP",
    "Roll 6+ to reduce incoming AP by 1."
};

const RuleColdData Resistance_ColdData{
    "Resistance",
    nullptr,
    0,
    "Resistance: 6+ to ignore wound",
    "Roll 6+ to ignore each wound after other saves."
};

const RuleColdData Tough_ColdData{
    "Tough",
    nullptr,
    0,
    "Tough({}): multi-wound",
    "Each model requires X wounds to kill instead of 1."
};

const RuleColdData Hero_ColdData{
    "Hero",
    nullptr,
    0,
    "Hero: takes wounds last",
    "This model takes wounds after other models in the unit."
};

const RuleColdData Takedown_ColdData{
    "Takedown",
    nullptr,
    0,
    "Takedown: pick target model",
    "Pick a specific model to target, resolve as if unit of 1."
};

const RuleColdData SelfDestruct_ColdData{
    "SelfDestruct",
    nullptr,
    0,
    "Self-Destruct({}): death strike",
    "When killed in melee, deal X hits back to the attacker."
};

const RuleColdData PredatorFighter_ColdData{
    "PredatorFighter",
    nullptr,
    0,
    "Predator Fighter: recursive 6s",
    "Unmodified 6s to hit in melee generate extra attacks that can also trigger."
};

const RuleColdData Counter_ColdData{
    "Counter",
    nullptr,
    0,
    "Counter: strikes first",
    "This unit strikes first when charged."
};

const RuleColdData Sniper_ColdData{
    "Sniper",
    nullptr,
    0,
    "Sniper: pick target model",
    "Can pick which model to target when shooting."
};

const RuleColdData Indirect_ColdData{
    "Indirect",
    nullptr,
    0,
    "Indirect: ignores cover",
    "Attacks with this weapon ignore cover."
};

const RuleColdData Lock_On_ColdData{
    "Lock_On",
    nullptr,
    0,
    "Lock-On: +1 to hit vehicles",
    "Get +1 to hit when targeting vehicles."
};

const RuleColdData Shred_ColdData{
    "Shred",
    nullptr,
    0,
    "Shred: bonus wound on 1s",
    "Unmodified defense roll of 1 causes an extra wound."
};

const RuleColdData Smash_ColdData{
    "Smash",
    nullptr,
    0,
    "Smash: ignore regen, +Blast vs heavy armor",
    "Bypass regeneration. Against Defense 5+/6+, gain Blast(3)."
};

const RuleColdData VersatileAttack_ColdData{
    "VersatileAttack",
    nullptr,
    0,
    "Versatile: choose hit or AP",
    "Roll d6: 1-3 get +1 to hit, 4-6 get AP+1."
};

const RuleColdData Limited_ColdData{
    "Limited",
    nullptr,
    0,
    "Limited: one use only",
    "This weapon can only be used once per game."
};

const RuleColdData BaneInMelee_ColdData{
    "BaneInMelee",
    nullptr,
    0,
    "Bane in Melee: all melee has Bane",
    "All melee attacks from this unit have Bane."
};

const RuleColdData Fear_ColdData{
    "Fear",
    nullptr,
    0,
    "Fear({}): morale penalty",
    "Counts as X extra wounds for enemy morale tests."
};

const RuleColdData Unstoppable_ColdData{
    "Unstoppable",
    nullptr,
    0,
    "Unstoppable: bypass saves",
    "Attacks bypass regeneration and ignore negative hit modifiers."
};

const RuleColdData PiercingAssault_ColdData{
    "PiercingAssault",
    nullptr,
    0,
    "Piercing Assault: minimum AP(1)",
    "When charging in melee, attacks have at least AP(1)."
};

const RuleColdData Surge_ColdData{
    "Surge",
    nullptr,
    0,
    "Surge: extra hit on 6s",
    "Unmodified 6s to hit generate an extra hit."
};

const RuleColdData PointBlankSurge_ColdData{
    "PointBlankSurge",
    nullptr,
    0,
    "Point-Blank Surge: 6s at short range",
    "Extra hit on 6s when shooting at 9\" or less."
};

const RuleColdData Rupture_ColdData{
    "Rupture",
    nullptr,
    0,
    "Rupture: bypass regen, +1 wound on 6s",
    "Unmodified 6s to hit bypass regeneration and deal +1 wound per wound."
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

// === Additional Combat Effect Entries ===

const RuleEffectEntry Deadly_Effects = EffectBuilder()
    .combat(CombatSubPhase::WOUND_ALLOCATION, deadly_effect)
    .build();

const RuleEffectEntry Poison_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, poison_effect)
    .build();

const RuleEffectEntry Bane_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, bane_effect)
    .build();

const RuleEffectEntry AP_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, ap_effect)
    .build();

const RuleEffectEntry Lance_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, lance_effect)
    .condition(CombatSubPhase::DEFENSE_RESOLUTION, lance_condition)
    .build();

const RuleEffectEntry Impact_Effects = EffectBuilder()
    .combat(CombatSubPhase::PRE_ATTACK, impact_effect)
    .condition(CombatSubPhase::PRE_ATTACK, impact_condition)
    .build();

const RuleEffectEntry Shielded_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, shielded_effect)
    .build();

const RuleEffectEntry ShieldWall_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, shield_wall_effect)
    .condition(CombatSubPhase::DEFENSE_RESOLUTION, shield_wall_condition)
    .build();

const RuleEffectEntry Protected_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, protected_effect)
    .build();

const RuleEffectEntry Resistance_Effects = EffectBuilder()
    .combat(CombatSubPhase::WOUND_ALLOCATION, resistance_effect)
    .build();

const RuleEffectEntry Tough_Effects = EffectBuilder()
    .combat(CombatSubPhase::WOUND_ALLOCATION, tough_effect)
    .build();

const RuleEffectEntry Hero_Effects = EffectBuilder()
    .combat(CombatSubPhase::WOUND_ALLOCATION, hero_effect)
    .build();

const RuleEffectEntry Takedown_Effects = EffectBuilder()
    .combat(CombatSubPhase::PRE_ATTACK, takedown_effect)
    .build();

const RuleEffectEntry SelfDestruct_Effects = EffectBuilder()
    .combat(CombatSubPhase::WOUND_ALLOCATION, self_destruct_effect)
    .build();

const RuleEffectEntry PredatorFighter_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_BONUSES, predator_fighter_effect)
    .condition(CombatSubPhase::HIT_BONUSES, predator_fighter_condition)
    .build();

const RuleEffectEntry Counter_Effects = EffectBuilder()
    .combat(CombatSubPhase::PRE_ATTACK, counter_effect)
    .build();

const RuleEffectEntry Sniper_Effects = EffectBuilder()
    .combat(CombatSubPhase::PRE_ATTACK, sniper_effect)
    .build();

const RuleEffectEntry Indirect_Effects = EffectBuilder()
    .combat(CombatSubPhase::PRE_ATTACK, indirect_effect)
    .build();

const RuleEffectEntry Lock_On_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_MODIFIERS, lock_on_effect)
    .condition(CombatSubPhase::HIT_MODIFIERS, lock_on_condition)
    .build();

const RuleEffectEntry Shred_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, shred_effect)
    .build();

const RuleEffectEntry Smash_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, smash_effect)
    .build();

const RuleEffectEntry VersatileAttack_Effects = EffectBuilder()
    .combat(CombatSubPhase::PRE_ATTACK, versatile_attack_effect)
    .build();

const RuleEffectEntry Limited_Effects = EffectBuilder()
    .combat(CombatSubPhase::PRE_ATTACK, limited_effect)
    .build();

const RuleEffectEntry BaneInMelee_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, bane_in_melee_effect)
    .condition(CombatSubPhase::DEFENSE_RESOLUTION, bane_in_melee_condition)
    .build();

const RuleEffectEntry Fear_Effects = EffectBuilder()
    .endround(EndRoundSubPhase::MORALE, fear_effect)
    .build();

const RuleEffectEntry Unstoppable_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, unstoppable_effect)
    .build();

const RuleEffectEntry PiercingAssault_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, piercing_assault_effect)
    .condition(CombatSubPhase::DEFENSE_RESOLUTION, piercing_assault_condition)
    .build();

const RuleEffectEntry Thrust_AP_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, thrust_ap_effect)
    .condition(CombatSubPhase::DEFENSE_RESOLUTION, thrust_condition)
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

    // Additional combat hot data
    registry.register_hot_data(RuleId::AP, AP_HotData);
    registry.register_hot_data(RuleId::Lance, Lance_HotData);
    registry.register_hot_data(RuleId::Impact, Impact_HotData);
    registry.register_hot_data(RuleId::Shielded, Shielded_HotData);
    registry.register_hot_data(RuleId::ShieldWall, ShieldWall_HotData);
    registry.register_hot_data(RuleId::Protected, Protected_HotData);
    registry.register_hot_data(RuleId::Resistance, Resistance_HotData);
    registry.register_hot_data(RuleId::Tough, Tough_HotData);
    registry.register_hot_data(RuleId::Hero, Hero_HotData);
    registry.register_hot_data(RuleId::Takedown, Takedown_HotData);
    registry.register_hot_data(RuleId::SelfDestruct, SelfDestruct_HotData);
    registry.register_hot_data(RuleId::PredatorFighter, PredatorFighter_HotData);
    registry.register_hot_data(RuleId::Counter, Counter_HotData);
    registry.register_hot_data(RuleId::Sniper, Sniper_HotData);
    registry.register_hot_data(RuleId::Indirect, Indirect_HotData);
    registry.register_hot_data(RuleId::Lock_On, Lock_On_HotData);
    registry.register_hot_data(RuleId::Shred, Shred_HotData);
    registry.register_hot_data(RuleId::Smash, Smash_HotData);
    registry.register_hot_data(RuleId::VersatileAttack, VersatileAttack_HotData);
    registry.register_hot_data(RuleId::Limited, Limited_HotData);
    registry.register_hot_data(RuleId::BaneInMelee, BaneInMelee_HotData);
    registry.register_hot_data(RuleId::Fear, Fear_HotData);
    registry.register_hot_data(RuleId::Unstoppable, Unstoppable_HotData);
    registry.register_hot_data(RuleId::PiercingAssault, PiercingAssault_HotData);

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

    // Additional combat cold data
    registry.register_cold_data(RuleId::Deadly, Deadly_ColdData);
    registry.register_cold_data(RuleId::Poison, Poison_ColdData);
    registry.register_cold_data(RuleId::Bane, Bane_ColdData);
    registry.register_cold_data(RuleId::AP, AP_ColdData);
    registry.register_cold_data(RuleId::Lance, Lance_ColdData);
    registry.register_cold_data(RuleId::Impact, Impact_ColdData);
    registry.register_cold_data(RuleId::Shielded, Shielded_ColdData);
    registry.register_cold_data(RuleId::ShieldWall, ShieldWall_ColdData);
    registry.register_cold_data(RuleId::Protected, Protected_ColdData);
    registry.register_cold_data(RuleId::Resistance, Resistance_ColdData);
    registry.register_cold_data(RuleId::Tough, Tough_ColdData);
    registry.register_cold_data(RuleId::Hero, Hero_ColdData);
    registry.register_cold_data(RuleId::Takedown, Takedown_ColdData);
    registry.register_cold_data(RuleId::SelfDestruct, SelfDestruct_ColdData);
    registry.register_cold_data(RuleId::PredatorFighter, PredatorFighter_ColdData);
    registry.register_cold_data(RuleId::Counter, Counter_ColdData);
    registry.register_cold_data(RuleId::Sniper, Sniper_ColdData);
    registry.register_cold_data(RuleId::Indirect, Indirect_ColdData);
    registry.register_cold_data(RuleId::Lock_On, Lock_On_ColdData);
    registry.register_cold_data(RuleId::Shred, Shred_ColdData);
    registry.register_cold_data(RuleId::Smash, Smash_ColdData);
    registry.register_cold_data(RuleId::VersatileAttack, VersatileAttack_ColdData);
    registry.register_cold_data(RuleId::Limited, Limited_ColdData);
    registry.register_cold_data(RuleId::BaneInMelee, BaneInMelee_ColdData);
    registry.register_cold_data(RuleId::Fear, Fear_ColdData);
    registry.register_cold_data(RuleId::Unstoppable, Unstoppable_ColdData);
    registry.register_cold_data(RuleId::PiercingAssault, PiercingAssault_ColdData);
    registry.register_cold_data(RuleId::Surge, Surge_ColdData);
    registry.register_cold_data(RuleId::PointBlankSurge, PointBlankSurge_ColdData);
    registry.register_cold_data(RuleId::Rupture, Rupture_ColdData);

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

    // Additional combat effects
    registry.register_effects(RuleId::Deadly, Deadly_Effects);
    registry.register_effects(RuleId::Poison, Poison_Effects);
    registry.register_effects(RuleId::Bane, Bane_Effects);
    registry.register_effects(RuleId::AP, AP_Effects);
    registry.register_effects(RuleId::Lance, Lance_Effects);
    registry.register_effects(RuleId::Impact, Impact_Effects);
    registry.register_effects(RuleId::Shielded, Shielded_Effects);
    registry.register_effects(RuleId::ShieldWall, ShieldWall_Effects);
    registry.register_effects(RuleId::Protected, Protected_Effects);
    registry.register_effects(RuleId::Resistance, Resistance_Effects);
    registry.register_effects(RuleId::Tough, Tough_Effects);
    registry.register_effects(RuleId::Hero, Hero_Effects);
    registry.register_effects(RuleId::Takedown, Takedown_Effects);
    registry.register_effects(RuleId::SelfDestruct, SelfDestruct_Effects);
    registry.register_effects(RuleId::PredatorFighter, PredatorFighter_Effects);
    registry.register_effects(RuleId::Counter, Counter_Effects);
    registry.register_effects(RuleId::Sniper, Sniper_Effects);
    registry.register_effects(RuleId::Indirect, Indirect_Effects);
    registry.register_effects(RuleId::Lock_On, Lock_On_Effects);
    registry.register_effects(RuleId::Shred, Shred_Effects);
    registry.register_effects(RuleId::Smash, Smash_Effects);
    registry.register_effects(RuleId::VersatileAttack, VersatileAttack_Effects);
    registry.register_effects(RuleId::Limited, Limited_Effects);
    registry.register_effects(RuleId::BaneInMelee, BaneInMelee_Effects);
    registry.register_effects(RuleId::Fear, Fear_Effects);
    registry.register_effects(RuleId::Unstoppable, Unstoppable_Effects);
    registry.register_effects(RuleId::PiercingAssault, PiercingAssault_Effects);
    registry.register_effects(RuleId::Thrust, Thrust_AP_Effects);  // AP effect for thrust

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

    // Additional combat aliases
    registry.register_alias("deadly", RuleId::Deadly);
    registry.register_alias("Deadly", RuleId::Deadly);
    registry.register_alias("poison", RuleId::Poison);
    registry.register_alias("Poison", RuleId::Poison);
    registry.register_alias("bane", RuleId::Bane);
    registry.register_alias("Bane", RuleId::Bane);
    registry.register_alias("ap", RuleId::AP);
    registry.register_alias("AP", RuleId::AP);
    registry.register_alias("lance", RuleId::Lance);
    registry.register_alias("Lance", RuleId::Lance);
    registry.register_alias("impact", RuleId::Impact);
    registry.register_alias("Impact", RuleId::Impact);
    registry.register_alias("shielded", RuleId::Shielded);
    registry.register_alias("Shielded", RuleId::Shielded);
    registry.register_alias("shieldwall", RuleId::ShieldWall);
    registry.register_alias("ShieldWall", RuleId::ShieldWall);
    registry.register_alias("protected", RuleId::Protected);
    registry.register_alias("Protected", RuleId::Protected);
    registry.register_alias("resistance", RuleId::Resistance);
    registry.register_alias("Resistance", RuleId::Resistance);
    registry.register_alias("tough", RuleId::Tough);
    registry.register_alias("Tough", RuleId::Tough);
    registry.register_alias("hero", RuleId::Hero);
    registry.register_alias("Hero", RuleId::Hero);
    registry.register_alias("takedown", RuleId::Takedown);
    registry.register_alias("Takedown", RuleId::Takedown);
    registry.register_alias("selfdestruct", RuleId::SelfDestruct);
    registry.register_alias("SelfDestruct", RuleId::SelfDestruct);
    registry.register_alias("predatorfighter", RuleId::PredatorFighter);
    registry.register_alias("PredatorFighter", RuleId::PredatorFighter);
    registry.register_alias("counter", RuleId::Counter);
    registry.register_alias("Counter", RuleId::Counter);
    registry.register_alias("sniper", RuleId::Sniper);
    registry.register_alias("Sniper", RuleId::Sniper);
    registry.register_alias("indirect", RuleId::Indirect);
    registry.register_alias("Indirect", RuleId::Indirect);
    registry.register_alias("lockon", RuleId::Lock_On);
    registry.register_alias("Lock_On", RuleId::Lock_On);
    registry.register_alias("shred", RuleId::Shred);
    registry.register_alias("Shred", RuleId::Shred);
    registry.register_alias("smash", RuleId::Smash);
    registry.register_alias("Smash", RuleId::Smash);
    registry.register_alias("versatileattack", RuleId::VersatileAttack);
    registry.register_alias("VersatileAttack", RuleId::VersatileAttack);
    registry.register_alias("limited", RuleId::Limited);
    registry.register_alias("Limited", RuleId::Limited);
    registry.register_alias("baneinmelee", RuleId::BaneInMelee);
    registry.register_alias("BaneInMelee", RuleId::BaneInMelee);
    registry.register_alias("fear", RuleId::Fear);
    registry.register_alias("Fear", RuleId::Fear);
    registry.register_alias("unstoppable", RuleId::Unstoppable);
    registry.register_alias("Unstoppable", RuleId::Unstoppable);
    registry.register_alias("piercingassault", RuleId::PiercingAssault);
    registry.register_alias("PiercingAssault", RuleId::PiercingAssault);
    registry.register_alias("surge", RuleId::Surge);
    registry.register_alias("Surge", RuleId::Surge);
    registry.register_alias("pointblanksurge", RuleId::PointBlankSurge);
    registry.register_alias("PointBlankSurge", RuleId::PointBlankSurge);
    registry.register_alias("rupture", RuleId::Rupture);
    registry.register_alias("Rupture", RuleId::Rupture);
    registry.register_alias("relentless", RuleId::Relentless);
    registry.register_alias("Relentless", RuleId::Relentless);
    registry.register_alias("furious", RuleId::Furious);
    registry.register_alias("Furious", RuleId::Furious);

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
