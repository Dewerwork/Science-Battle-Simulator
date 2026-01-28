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

// ==============================================================================
// Category A: Simple Modifier Effect Implementations
// ==============================================================================

void evasive_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Evasive - enemies always get -1 to hit (unlike Stealth which is >9" only)
    ctx.hit_modifier -= 1;
}

void steadfast_morale_effect(EndRoundContext& /*ctx*/, Unit& unit, u8 /*value*/) {
    // Steadfast - 4+ to stop being Shaken at round start (same as Battleborn)
    (void)unit;  // Placeholder - implemented in morale system
}

void swift_effect(MovementContext& ctx, u8 /*value*/) {
    // Swift - ignore Slow rule (cancels the -2" penalty)
    // If Slow is also present, this nullifies its effect
    ctx.swift_active = true;
}

void ferocious_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Ferocious - extra hit on natural 6s (unit rule version of Surge)
    u32 bonus = ctx.natural_sixes;
    ctx.hits += bonus;
    if (ext) {
        ext->bonus_hits += bonus;
    }
}

void lacerate_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Lacerate - force defender to reroll defense 6s (like Poison)
    ctx.set_force_reroll(true);
}

void mischievous_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Mischievous - force defender to reroll defense 6s (like Poison)
    ctx.set_force_reroll(true);
}

void scrapper_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Scrapper - force defender to reroll defense 6s (like Poison)
    ctx.set_force_reroll(true);
}

// ==============================================================================
// Category B: Weapon Conditional Effect Implementations
// ==============================================================================

void bash_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Bash - +Blast(3) vs Defense 5+/6+
    if (ext && ctx.defender && ctx.defender->defense >= 5) {
        ext->smash_bonus_blast = 3;  // Reuse smash_bonus_blast for conditional blast
    }
}

void thrash_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Thrash - +Blast(3) vs Defense 5+/6+ (same as Bash)
    if (ext && ctx.defender && ctx.defender->defense >= 5) {
        ext->smash_bonus_blast = 3;
    }
}

void crack_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Crack - AP(+2) on natural 6s to hit
    if (ext && ctx.natural_sixes > 0) {
        ext->crack_ap_bonus = 2;
    }
}

void destructive_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Destructive - AP(+4) on natural 6s to hit
    if (ext && ctx.natural_sixes > 0) {
        ext->crack_ap_bonus = 4;
    }
}

void fracture_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Fracture - AP(+2) on natural 6s to hit + ignores cover
    if (ext) {
        ext->ignores_cover = true;
        if (ctx.natural_sixes > 0) {
            ext->crack_ap_bonus = 2;
        }
    }
}

void break_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Break - AP(+2) on natural 6s to hit + ignores regen
    ctx.set_bypass_regen(true);
    if (ext && ctx.natural_sixes > 0) {
        ext->crack_ap_bonus = 2;
    }
}

void slash_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Slash - Extra hit on 6s + ignores cover
    u32 bonus = ctx.natural_sixes;
    ctx.hits += bonus;
    if (ext) {
        ext->bonus_hits += bonus;
        ext->ignores_cover = true;
    }
}

void butcher_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Butcher - Extra hit on 6s + ignores regen
    u32 bonus = ctx.natural_sixes;
    ctx.hits += bonus;
    ctx.set_bypass_regen(true);
    if (ext) {
        ext->bonus_hits += bonus;
    }
}

void slam_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 /*value*/) {
    // Slam - Extra wound on defense 1s + ignores cover
    if (ext) {
        ext->shred_active = true;  // Reuse shred mechanism
        ext->ignores_cover = true;
    }
}

void quake_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Quake - Extra wound on defense 1s + ignores regen
    ctx.set_bypass_regen(true);
    if (ext) {
        ext->shred_active = true;  // Reuse shred mechanism
    }
}

bool tough_target_condition(const CombatContextCore& ctx) {
    // Condition for rules that affect Tough(3-9) targets
    if (!ctx.defender) return false;
    u8 tough_val = ctx.defender->get_rule_value(RuleId::Tough);
    return tough_val >= 3 && tough_val <= 9;
}

void tear_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Tear - AP(+4) vs Tough(3-9)
    ctx.ap_modifier += 4;
}

void scratch_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Scratch - AP(+2) vs Tough(3-9)
    ctx.ap_modifier += 2;
}

void puncture_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Puncture - AP(+4) vs Tough(3-9) + ignores regen
    ctx.ap_modifier += 4;
    ctx.set_bypass_regen(true);
}

void shatter_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Shatter - AP(+2) vs Tough(3-9) + ignores regen
    ctx.ap_modifier += 2;
    ctx.set_bypass_regen(true);
}

void demolish_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Demolish - AP(+2) vs Tough(3-9) + ignores cover
    ctx.ap_modifier += 2;
    if (ext) {
        ext->ignores_cover = true;
    }
}

void impale_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 /*value*/) {
    // Impale - Deadly(+3) vs Tough(3-9)
    if (ext) {
        ext->wound_multiplier += 3;
    }
}

void skewer_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 /*value*/) {
    // Skewer - Deadly(+3) vs Tough(3-9) + ignores cover
    if (ext) {
        ext->wound_multiplier += 3;
        ext->ignores_cover = true;
    }
}

bool light_armor_condition(const CombatContextCore& ctx) {
    // Condition for rules that affect Defense 2-3+ (light armor)
    if (!ctx.defender) return false;
    return ctx.defender->defense >= 2 && ctx.defender->defense <= 3;
}

bool light_medium_armor_condition(const CombatContextCore& ctx) {
    // Condition for rules that affect Defense 2-4+ (light-medium armor)
    if (!ctx.defender) return false;
    return ctx.defender->defense >= 2 && ctx.defender->defense <= 4;
}

void reap_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Reap - AP(+2) vs Defense 2-3+
    ctx.ap_modifier += 2;
}

void disintegrate_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Disintegrate - AP(+2) vs Defense 2-3+ + ignores regen
    ctx.ap_modifier += 2;
    ctx.set_bypass_regen(true);
}

void decimate_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Decimate - AP(+2) vs Defense 2-3+ + ignores cover
    ctx.ap_modifier += 2;
    if (ext) {
        ext->ignores_cover = true;
    }
}

void fragment_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Fragment - AP(+1) vs Defense 2-4+ + ignores cover
    ctx.ap_modifier += 1;
    if (ext) {
        ext->ignores_cover = true;
    }
}

void wreck_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Wreck - Re-roll defense 6s + ignores cover
    ctx.set_force_reroll(true);
    if (ext) {
        ext->ignores_cover = true;
    }
}

// ==============================================================================
// Category C: Movement Bonus Effect Implementations
// ==============================================================================

void lustbound_effect(MovementContext& ctx, u8 /*value*/) {
    // Lustbound - +1" Advance, +3" Rush/Charge
    if (ctx.move_type == MovementContext::MoveType::ADVANCE) {
        ctx.distance_modifier += 1;
    } else if (ctx.move_type == MovementContext::MoveType::RUSH ||
               ctx.move_type == MovementContext::MoveType::CHARGE) {
        ctx.distance_modifier += 3;
    }
}

void highborn_effect(MovementContext& ctx, u8 /*value*/) {
    // Highborn - +2" Advance, +2" Rush/Charge
    if (ctx.move_type == MovementContext::MoveType::ADVANCE) {
        ctx.distance_modifier += 2;
    } else if (ctx.move_type == MovementContext::MoveType::RUSH ||
               ctx.move_type == MovementContext::MoveType::CHARGE) {
        ctx.distance_modifier += 2;
    }
}

void scurry_effect(MovementContext& ctx, u8 /*value*/) {
    // Scurry - +2" Advance, +2" Rush/Charge (same as Highborn)
    if (ctx.move_type == MovementContext::MoveType::ADVANCE) {
        ctx.distance_modifier += 2;
    } else if (ctx.move_type == MovementContext::MoveType::RUSH ||
               ctx.move_type == MovementContext::MoveType::CHARGE) {
        ctx.distance_modifier += 2;
    }
}

void darkborn_effect(MovementContext& ctx, u8 /*value*/) {
    // Darkborn - +3" range, +3" Charge
    // Range bonus handled elsewhere, charge bonus here
    if (ctx.move_type == MovementContext::MoveType::CHARGE) {
        ctx.distance_modifier += 3;
    }
}

void hit_and_run_shooter_effect(MovementContext& ctx, u8 /*value*/) {
    // Hit & Run Shooter - Move 3" after shooting
    ctx.hit_and_run_pending = true;
    ctx.hit_and_run_distance = 3;
}

// ==============================================================================
// Category D: Defense Modifier Effect Implementations
// ==============================================================================

void fortified_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Fortified - All hits count as AP(-1), min AP(0)
    // Reduce attacker's AP by 1 (minimum 0)
    if (ctx.ap_modifier > 0) {
        ctx.ap_modifier -= 1;
    }
}

bool distance_over_9_condition(const CombatContextCore& ctx) {
    // Condition for rules that only apply from >9" away
    return ctx.distance > 9;
}

void guardian_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Guardian - AP(-1) when shot/charged from >9"
    // Condition checked separately, this just applies the effect
    if (ctx.ap_modifier > 0) {
        ctx.ap_modifier -= 1;
    }
}

void guardian_boost_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Guardian Boost - Guardian always applies (AP(-1))
    // No distance check needed
    if (ctx.ap_modifier > 0) {
        ctx.ap_modifier -= 1;
    }
}

void sturdy_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Sturdy - +1 defense when shot/charged from >9"
    // This effectively gives defender +1 to defense rolls
    ctx.defense_modifier += 1;
}

void knightborn_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 /*value*/) {
    // Knightborn - 6+ to ignore wounds (4+ vs spells)
    // Flag for wound allocation phase to check
    if (ext) {
        ext->knightborn_active = true;
    }
}

void plaguebound_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 /*value*/) {
    // Plaguebound - 6+ to ignore wounds
    if (ext) {
        ext->plaguebound_active = true;
    }
}

void changebound_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Changebound - Enemies get -1 to hit from >9"
    // Condition checked separately
    ctx.hit_modifier -= 1;
}

// ==============================================================================
// Category E: Extra Attack Generation & Distance-based Combat Effect Implementations
// ==============================================================================

void bloodborn_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Bloodborn - 6s to hit generate +1 attack (non-recursive)
    // Unlike PredatorFighter, these bonus attacks don't trigger more attacks
    u32 bonus = ctx.natural_sixes;
    ctx.hits += bonus;
    if (ext) {
        ext->bonus_hits += bonus;
        // Note: predator_fighter_active stays false - no recursion
    }
}

void clan_warrior_boost_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Clan Warrior Boost - 5s AND 6s to hit generate +1 attack (non-recursive)
    // Unlike Bloodborn which only triggers on 6s, this triggers on both 5s and 6s
    u32 bonus = ctx.natural_sixes + ctx.natural_fives;
    ctx.hits += bonus;
    if (ext) {
        ext->bonus_hits += bonus;
        // Note: These bonus attacks don't trigger more attacks
    }
}

bool shooting_over_9_condition(const CombatContextCore& ctx) {
    // Only when shooting from >9"
    return ctx.combat_type == CombatType::SHOOTING && ctx.distance > 9;
}

void targeting_visor_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Targeting Visor - +1 to hit when shooting over 9"
    ctx.hit_modifier += 1;
}

bool havocbound_condition(const CombatContextCore& ctx) {
    // Havocbound - applies when shooting over 9" or charging
    return (ctx.combat_type == CombatType::SHOOTING && ctx.distance > 9) || ctx.is_charge;
}

void havocbound_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Havocbound - AP(+1) when shooting over 9" or charging
    ctx.ap_modifier += 1;
}

void warbound_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 /*value*/) {
    // Warbound - Extra wound on defense 1s (like Shred)
    if (ext) {
        ext->shred_active = true;
    }
}

bool melee_only_condition(const CombatContextCore& ctx) {
    return ctx.combat_type == CombatType::MELEE;
}

void brutal_fighter_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Brutal Fighter - 6s to hit deal extra hit in melee
    u32 bonus = ctx.natural_sixes;
    ctx.hits += bonus;
    if (ext) {
        ext->bonus_hits += bonus;
    }
}

// ==============================================================================
// Category F: Combat Choice & Retaliation Effect Implementations
// ==============================================================================

void unpredictable_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Unpredictable - Roll D6: 1-3 = AP+1, 4-6 = +1 hit
    // Like VersatileAttack but general purpose
    if (ext && ext->versatile_roll > 0) {
        if (ext->versatile_roll <= 3) {
            ctx.ap_modifier += 1;
        } else {
            ctx.hit_modifier += 1;
        }
    }
}

void unpredictable_fighter_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Unpredictable Fighter - Roll D6 in melee: 1-3 = AP+1, 4-6 = +1 hit
    if (ext && ext->versatile_roll > 0) {
        if (ext->versatile_roll <= 3) {
            ctx.ap_modifier += 1;
        } else {
            ctx.hit_modifier += 1;
        }
    }
}

void unpredictable_shooter_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Unpredictable Shooter - Roll D6 when shooting: 1-3 = AP+1, 4-6 = +1 hit
    if (ext && ext->versatile_roll > 0) {
        if (ext->versatile_roll <= 3) {
            ctx.ap_modifier += 1;
        } else {
            ctx.hit_modifier += 1;
        }
    }
}

bool shooting_only_condition(const CombatContextCore& ctx) {
    return ctx.combat_type == CombatType::SHOOTING;
}

void retaliate_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 value) {
    // Retaliate(X) - When this model takes a wound in melee, attacker takes X hits
    // This is tracked in extended context for later resolution
    if (ext) {
        ext->retaliate_hits += value;
    }
}

void deathstrike_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 value) {
    // Deathstrike(X) - If killed in melee, attacker takes X hits
    // Similar to SelfDestruct but triggers on model death
    if (ext) {
        ext->deathstrike_hits += value;
    }
}

// ==============================================================================
// Category G: Enhanced Combat Modifiers & Boost Rules Effect Implementations
// ==============================================================================

void ferocious_boost_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Ferocious Boost - Extra hits on 5-6 to hit (instead of just 6)
    // Count both 5s and 6s for bonus hits
    u32 bonus = ctx.natural_sixes;  // Already counted 6s
    // Note: natural_fives would need to be tracked separately if we want exact 5s
    // For now, we double the bonus since 5-6 is twice as likely as just 6
    ctx.hits += bonus;  // Additional bonus for the 5s (approximation)
    if (ext) {
        ext->bonus_hits += bonus;
        ext->ferocious_boost_active = true;
    }
}

void changebound_boost_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Changebound Boost - -1 to hit always (not just from >9")
    // No distance condition needed
    ctx.hit_modifier -= 1;
}

void warbound_boost_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 /*value*/) {
    // Warbound Boost - Extra wound on defense 1-2 (not just 1)
    if (ext) {
        ext->warbound_boost_active = true;
    }
}

void plaguebound_boost_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 /*value*/) {
    // Plaguebound Boost - 5-6 to ignore wounds (not just 6)
    if (ext) {
        ext->plaguebound_boost_active = true;
    }
}

void lustbound_boost_effect(MovementContext& ctx, u8 /*value*/) {
    // Lustbound Boost - +2" Advance, +6" Rush/Charge (enhanced Lustbound)
    // Use distance_modifier for advance (+2) and charge_bonus for charge (+6)
    ctx.distance_modifier += 2;
    ctx.charge_bonus += 6;
}

bool target_has_tough_3_plus(const CombatContextCore& ctx) {
    // Check if defender has Tough(3) or higher
    // This would need access to defender's rules
    return ctx.defender != nullptr;  // Simplified - actual check in combat resolver
}

void melee_slayer_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Melee Slayer - AP(+2) vs Tough(3+) in melee
    // Condition checked separately
    ctx.ap_modifier += 2;
    if (ext) {
        ext->melee_slayer_active = true;
    }
}

void heavy_impact_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Heavy Impact - Impact hits get AP(1)
    // Applied during Impact hit resolution
    if (ext && ext->impact_attacks > 0) {
        ctx.ap_modifier += 1;
    }
}

void watchborn_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Watchborn - Pick AP(+1) or +1 to hit when activated
    // For simulation, roll d6: 1-3 = AP+1, 4-6 = +1 hit
    if (ext && ext->dice) {
        u8 roll = ext->dice->roll_d6();
        if (roll <= 3) {
            // AP+1 bonus - tracked for later application in DEFENSE_RESOLUTION
            ext->versatile_ap_chosen = true;
            if (ext->logger) {
                ext->logger->on_rule_triggered("Watchborn", "chose_ap+1", roll);
            }
        } else {
            // +1 hit bonus - applied immediately
            ctx.hit_modifier += 1;
            ext->versatile_ap_chosen = false;
            if (ext->logger) {
                ext->logger->on_hit_modifier("Watchborn", +1, "chose_+1_hit");
            }
        }
    }
}

// ==============================================================================
// Category H: Dice-Based Special Attacks & Post-Combat Movement Effect Implementations
// ==============================================================================

void crush_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 value) {
    // Crush(X) - Roll X dice in melee, each 4+ is an AP(2) hit
    // This generates additional hits with AP(2)
    // The dice rolling would be done in the combat resolver
    // Here we just set up the parameters
    if (ext && ctx.combat_type == CombatType::MELEE) {
        // Mark that crush hits should have AP(2)
        ctx.ap_modifier += 2;  // AP(2) for crush hits
        // value = number of dice to roll (handled in resolver)
        (void)value;
    }
}

void ravage_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 value) {
    // Ravage(X) - Roll X dice in melee, each 6+ causes a wound
    // Direct wounds, not attacks
    if (ctx.combat_type == CombatType::MELEE) {
        // value = number of dice to roll
        // In simplified simulation, treat as ~16.67% chance per die
        // For now, store value for resolver to handle
        (void)value;
    }
}

void hazardous_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Hazardous - Attacks have AP(4) but take wound on hit roll of 1
    // Apply AP(4) bonus
    ctx.ap_modifier += 4;
    if (ext) {
        // Mark hazardous active for self-damage tracking
        // Self-damage on 1s handled in combat resolver
    }
}

void quake_when_shooting_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 /*value*/) {
    // Quake when Shooting - Apply Shred effect (extra wound on defense 1s)
    // Also bypasses regeneration
    if (ext) {
        ext->shred_active = true;  // Use same mechanism as Shred
    }
}

void harassing_effect(MovementContext& ctx, u8 /*value*/) {
    // Harassing - Move 3" after shooting or melee
    ctx.hit_and_run_pending = true;
    ctx.hit_and_run_distance = 3;
}

void guerrilla_effect(MovementContext& ctx, u8 /*value*/) {
    // Guerrilla - Move 3" after shooting or melee (once per round)
    // Same as harassing but tracked per-round in game state
    ctx.hit_and_run_pending = true;
    ctx.hit_and_run_distance = 3;
}

// ==============================================================================
// Category I-O: Batch Implementation Effect Functions
// ==============================================================================

// --- Category I: Aura Effects (map to enhanced base rules) ---
void versatile_reach_aura_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Versatile Reach - extended melee range (handled at targeting phase)
    // No combat context modification needed
}

void precision_fighter_aura_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // +1 to hit in melee for unit
    ctx.hit_modifier += 1;
}

void piercing_fighter_aura_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // AP+1 in melee for unit
    ctx.ap_modifier += 1;
}

void precision_charge_aura_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // +1 to hit when charging
    if (ctx.is_charge) {
        ctx.hit_modifier += 1;
    }
}

void piercing_shooter_aura_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // AP+1 when shooting
    ctx.ap_modifier += 1;
}

void quick_shot_aura_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Quick Shot - additional shooting capability
    ctx.hit_modifier += 1;
}

// --- Category J: Buff Effects ---
void righteous_fury_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Grant Piercing Assault to friendlies - AP(1) minimum on charge
    ctx.ap_modifier = std::max(ctx.ap_modifier, static_cast<i8>(1));
}

void precision_fighter_buff_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // +1 to hit in melee
    ctx.hit_modifier += 1;
}

void precision_shooter_buff_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // +1 to hit when shooting
    ctx.hit_modifier += 1;
}

void bane_in_melee_buff_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Grant Bane in melee - bypass regen, force defense 6 rerolls
    ctx.trait_flags |= CombatContextCore::FORCE_REROLL;
    ctx.trait_flags |= CombatContextCore::BYPASS_REGEN;
}

void primal_boost_buff_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Enhanced primal - extra hits on 5-6
    u32 bonus = ctx.natural_sixes;
    ctx.hits += bonus;
    if (ext) {
        ext->bonus_hits += bonus;
    }
}

void precision_attacks_buff_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // +1 to hit attacking
    ctx.hit_modifier += 1;
}

// --- Category K: Mark Effects ---
void shred_mark_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 /*value*/) {
    // Grant Shred vs target - extra wound on defense 1s
    if (ext) {
        ext->shred_active = true;
    }
}

void piercing_shooting_mark_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // AP+1 shooting vs target
    ctx.ap_modifier += 1;
}

void precision_fighting_mark_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // +1 hit in melee vs target
    ctx.hit_modifier += 1;
}

void slayer_mark_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Slayer vs target - AP+2 vs Tough
    ctx.ap_modifier += 2;
}

void relentless_mark_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Grant Relentless vs target
    u32 bonus = ctx.natural_sixes;
    ctx.hits += bonus;
    if (ext) {
        ext->bonus_hits += bonus;
    }
}

void rending_in_melee_mark_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Grant Rending in melee vs target
    if (ext) {
        ext->rending_hits += ctx.natural_sixes;
    }
}

void furious_mark_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Grant Furious vs target
    u32 bonus = ctx.natural_sixes;
    ctx.hits += bonus;
    if (ext) {
        ext->bonus_hits += bonus;
    }
}

void indirect_mark_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 /*value*/) {
    // Grant Indirect vs target - ignore cover
    if (ext) {
        ext->ignores_cover = true;
    }
}

void bane_mark_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Grant Bane vs target - force reroll 6s on defense, bypass regen
    ctx.trait_flags |= CombatContextCore::FORCE_REROLL;
    ctx.trait_flags |= CombatContextCore::BYPASS_REGEN;
}

void precision_shooting_mark_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // +1 hit shooting vs target
    ctx.hit_modifier += 1;
}

void quick_shot_mark_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Quick Shot vs target
    ctx.hit_modifier += 1;
}

// --- Category L: Debuff Effects ---
void unwieldy_debuff_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Grant Unwieldy to enemy - -1 to hit in melee
    ctx.hit_modifier -= 1;
}

void precision_debuff_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // -1 to hit for enemy
    ctx.hit_modifier -= 1;
}

void piercing_shooting_debuff_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Remove AP+1 from enemy shooting
    ctx.ap_modifier -= 1;
}

// --- Category M: Scaling/Frenzy/Growth Effects ---
void piercing_growth_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 value) {
    // Marker-based AP scaling - value = current markers
    ctx.ap_modifier += static_cast<i8>(value);
}

void ruinous_frenzy_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 value) {
    // Marker-based hit/defense bonus
    ctx.hit_modifier += static_cast<i8>(value);
}

void devastating_frenzy_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 value) {
    // Marker-based AP/defense bonus
    ctx.ap_modifier += static_cast<i8>(value);
}

void precision_growth_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 value) {
    // Marker-based hit bonus
    ctx.hit_modifier += static_cast<i8>(value);
}

void destructive_frenzy_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 value) {
    // Marker-based hit/AP bonus
    ctx.hit_modifier += static_cast<i8>(value / 2);
    ctx.ap_modifier += static_cast<i8>(value / 2);
}

// --- Category N: Storm AoE Effects ---
void storm_of_change_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 /*value*/) {
    // Roll 3 dice for AoE Shred hits
    if (ext) {
        ext->shred_active = true;
        ext->bonus_hits += 3;  // Simulate 3 dice of storm damage
    }
}

void storm_of_lust_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Roll 3 dice for AoE Surge hits
    u32 bonus = ctx.natural_sixes;
    ctx.hits += bonus;
    if (ext) {
        ext->bonus_hits += bonus + 3;  // Surge + storm dice
    }
}

void storm_of_plague_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Roll 3 dice for AoE Bane hits
    ctx.trait_flags |= CombatContextCore::FORCE_REROLL;
    ctx.trait_flags |= CombatContextCore::BYPASS_REGEN;
    if (ext) {
        ext->bonus_hits += 3;  // Storm dice
    }
}

void storm_of_war_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Roll 3 dice for AoE AP(1) hits
    ctx.ap_modifier += 1;
    if (ext) {
        ext->bonus_hits += 3;  // Storm dice
    }
}

// --- Category O: Special Combat Effects ---
void breath_attack_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Breath Attack is handled separately in game_runner.hpp (process_breath_attack)
    // It's a once-per-activation ability that happens BEFORE attacks, not during weapon attacks
    // No-op here to avoid double-application
}

void increased_shooting_range_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // +6" range when shooting (handled at targeting phase)
    // No combat context modification needed
}

void regenerative_strength_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 value) {
    // Gain attacks when ignoring wounds - value represents bonus
    ctx.attacks += value;
}

void strafing_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Attack when moving through enemy units
    ctx.hits += 1;  // Strafing hit
    if (ext) {
        ext->bonus_hits += 1;
    }
}

void surprise_attack_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Infiltrate variant with damage on deploy
    ctx.hits += 2;  // Surprise attack bonus
    if (ext) {
        ext->bonus_hits += 2;
    }
}

void takedown_strike_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Once per game targeted attack at Quality 2+
    ctx.quality_used = 2;
    if (ext) {
        ext->quality_override = 2;
    }
}

void instinctive_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // Must attack closest target with +1 hit
    ctx.hit_modifier += 1;
}

void crossing_attack_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 value) {
    // Roll X dice moving through enemies for hits
    u32 bonus = value / 2;  // Approximation of successful rolls
    ctx.hits += bonus;
    if (ext) {
        ext->bonus_hits += bonus;
    }
}

void quick_readjustment_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 /*value*/) {
    // Ignore Indirect move penalty - flag as not moved
    if (ext) {
        ext->moved_this_activation = false;
    }
}

void crossing_strike_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Attack through movement
    ctx.hits += 2;
    if (ext) {
        ext->bonus_hits += 2;
    }
}

void surprise_piercing_shot_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) {
    // AP+2 when deploying via Ambush
    ctx.ap_modifier += 2;
}

void mind_wound_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // Enemy takes 2 hits with Demolish effect
    ctx.hits += 2;
    if (ext) {
        ext->bonus_hits += 2;
        ext->ignores_cover = true;  // Demolish ignores cover
    }
}

void mobile_artillery_effect(CombatContextCore& ctx, CombatContextExtended* ext, u8 /*value*/) {
    // +1 hit from Hold, -2 to be hit when still
    if (ext && !ext->moved_this_activation) {
        ctx.hit_modifier += 1;
    }
}

void vengeance_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 value) {
    // Markers for hit bonus when destroyed - value = marker count
    ctx.hit_modifier += static_cast<i8>(value);
}

// === Category P-W: Remaining Rules Effects ===
// Most of these are handled at activation/deployment/movement phases
// Stub implementations for combat context compatibility

void martial_prowess_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}
void coordinate_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}
void inquisitorial_agent_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}
void delayed_action_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}

// Category Q: Boost Auras - using existing CombatContextCore fields
void sturdy_boost_aura_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) { ctx.defense_modifier += 1; }
void versatile_defense_aura_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) { ctx.defense_modifier += 1; }
void changebound_boost_aura_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) { ctx.hit_modifier -= 1; }
void plaguebound_boost_aura_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 /*value*/) { if (ext) ext->resistance_active = true; }
void defensive_growth_aura_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 value) { ctx.ap_modifier -= static_cast<i8>(value); }
void machine_fog_boost_aura_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) { ctx.hit_modifier -= 1; }
void reanimation_aura_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // End round handling
void grounded_reinforcement_aura_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) { ctx.ap_modifier -= 1; }
void devout_boost_aura_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 /*value*/) { if (ext) ext->bonus_hits += 1; }  // Extra hits on 5-6
void courage_aura_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Morale phase handling
void hold_the_line_boost_aura_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Morale phase handling
void rapid_rush_aura_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Movement phase handling
void harassing_boost_aura_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Post-combat movement
void swift_aura_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Movement phase handling
void rapid_blink_boost_aura_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Movement phase handling
void rapid_advance_aura_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Movement phase handling
void lustbound_boost_aura_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Movement phase handling
void bounding_aura_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Movement phase handling
void highborn_boost_aura_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Movement phase handling
void speed_feat_aura_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Movement phase handling
void scurry_boost_aura_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Movement phase handling
void guerrilla_boost_aura_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Post-combat movement

// Category R: Buffs - using existing CombatContextCore fields
void stealth_buff_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) { ctx.hit_modifier -= 1; }  // Harder to hit
void protective_dome_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) { ctx.hit_modifier -= 1; }  // Evasive
void guarded_buff_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) { ctx.defense_modifier += 1; }
void regeneration_buff_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // End round handling
void entrenched_buff_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) { ctx.defense_modifier += 2; }
void self_repair_boost_buff_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // End round handling
void courage_buff_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Morale phase handling
void steadfast_buff_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Morale phase handling
void no_retreat_buff_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Morale phase handling
void rapid_advance_buff_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Movement phase handling
void swift_buff_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Movement phase handling
void speed_buff_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Movement phase handling
void increased_shooting_range_mark_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Range modifier
void extended_buff_range_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Buff range modifier
void increased_shooting_range_buff_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Range modifier
void casting_buff_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) { ctx.hit_modifier += 1; }

// Category S: Debuffs - using existing CombatContextCore fields
void fatigue_debuff_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) { ctx.trait_flags |= CombatContextCore::IS_FATIGUED; }
void defense_debuff_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) { ctx.defense_modifier -= 1; }
void morale_debuff_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Morale phase handling
void speed_debuff_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Movement phase handling
void dangerous_terrain_debuff_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Movement phase handling
void mind_control_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Special handling
void difficult_terrain_debuff_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Movement phase handling
void casting_debuff_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) { ctx.hit_modifier -= 1; }

// Category T: Defense/Growth Rules
void fortified_growth_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 value) { ctx.ap_modifier -= static_cast<i8>(value); }
void grounded_stealth_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) { ctx.hit_modifier -= 1; }
void protection_feat_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 /*value*/) { if (ext) ext->resistance_active = true; }

void infiltrate_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}
void spawn_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}
void re_deployment_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}
void rapid_ambush_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}
void ambush_beacon_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}
void ambush_re_deployment_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}
void repel_ambushers_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}
void reinforcement_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}
void fanatic_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}
void split_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}

// Category V: Movement Rules - stubs for movement phase handling
void teleport_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Movement phase handling
void wolfborn_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Movement phase handling
void hit_and_run_shooter_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Post-combat movement
void rapid_blink_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Movement phase handling
void bounding_movement_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Movement phase handling

// Category W: Faction/Morale/Misc - using existing fields or stubs
void devout_boost_effect(CombatContextCore& /*ctx*/, CombatContextExtended* ext, u8 /*value*/) { if (ext) ext->bonus_hits += 1; }  // Extra hits on 5-6
void mend_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Activation phase handling
void hive_bond_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Morale phase handling
void re_position_artillery_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Movement phase handling

// Category X: Spell & Targeting Rules - stub implementations
void caster_group_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Caster pooling mechanic
void spell_conduit_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Cast from this position
void spell_accumulator_effect(CombatContextCore& /*ctx*/, CombatContextExtended* /*ext*/, u8 /*value*/) {}  // Store spell energy
void precision_target_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) { ctx.hit_modifier += 1; }  // Hit bonus vs target
void piercing_target_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) { ctx.ap_modifier += 1; }  // AP bonus vs target
void precision_spotter_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) { ctx.hit_modifier += 1; }  // Spotter hit bonus
void piercing_spotter_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) { ctx.ap_modifier += 1; }  // Spotter AP bonus
void precision_tag_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) { ctx.hit_modifier += 1; }  // Tagged for precision
void piercing_tag_effect(CombatContextCore& ctx, CombatContextExtended* /*ext*/, u8 /*value*/) { ctx.ap_modifier += 1; }  // Tagged for piercing

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

// Counter-Attack: Unit rule that makes ALL weapons strike first when charged (no Impact reduction)
const RuleHotData CounterAttack_HotData{
    RuleId::CounterAttack,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::PRE_ATTACK),
    CombatType::MELEE,
    Target::ATTACKER,  // The unit with Counter-Attack (defender when charged)
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
    Target::ATTACKER,  // Indirect is a unit rule, not a weapon rule
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
// Category A: Simple Modifier Hot Data
// ==============================================================================

const RuleHotData Evasive_HotData{
    RuleId::Evasive,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_MODIFIERS),
    CombatType::BOTH,
    Target::DEFENDER,
    Trigger::ALWAYS,
    RulePriority::EARLY,
    static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL)
};

const RuleHotData Steadfast_HotData{
    RuleId::Steadfast,
    GamePhase::END_ROUND,
    static_cast<u8>(EndRoundSubPhase::MORALE),
    CombatType::BOTH,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::FIRST,
    static_cast<TraitMask>(RuleTrait::AFFECTS_MORALE)
};

const RuleHotData Swift_HotData{
    RuleId::Swift,
    GamePhase::MOVEMENT,
    static_cast<u8>(MoveSubPhase::CALCULATE_DISTANCE),
    CombatType::BOTH,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::FIRST,
    0
};

const RuleHotData Ferocious_HotData{
    RuleId::Ferocious,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_BONUSES),
    CombatType::BOTH,
    Target::ATTACKER,
    Trigger::ON_HIT_ROLL_6,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS)
};

const RuleHotData Lacerate_HotData{
    RuleId::Lacerate,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::FORCES_DEFENSE_REROLL)
};

const RuleHotData Mischievous_HotData{
    RuleId::Mischievous,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::ATTACKER,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::FORCES_DEFENSE_REROLL)
};

const RuleHotData Scrapper_HotData{
    RuleId::Scrapper,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::ATTACKER,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::FORCES_DEFENSE_REROLL)
};

// ==============================================================================
// Category B: Weapon Conditional Hot Data
// ==============================================================================

const RuleHotData Bash_HotData{
    RuleId::Bash,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MULTIPLIES_HITS)
};

const RuleHotData Thrash_HotData{
    RuleId::Thrash,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MULTIPLIES_HITS)
};

const RuleHotData Crack_HotData{
    RuleId::Crack,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_SEPARATION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ON_HIT_ROLL_6,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP)
};

const RuleHotData Destructive_HotData{
    RuleId::Destructive,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_SEPARATION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ON_HIT_ROLL_6,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP)
};

const RuleHotData Fracture_HotData{
    RuleId::Fracture,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_SEPARATION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ON_HIT_ROLL_6,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP) |
    static_cast<TraitMask>(RuleTrait::IGNORES_COVER)
};

const RuleHotData Break_HotData{
    RuleId::Break,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_SEPARATION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ON_HIT_ROLL_6,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP) |
    static_cast<TraitMask>(RuleTrait::BYPASSES_REGENERATION)
};

const RuleHotData Slash_HotData{
    RuleId::Slash,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_BONUSES),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ON_HIT_ROLL_6,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS) |
    static_cast<TraitMask>(RuleTrait::IGNORES_COVER)
};

const RuleHotData Butcher_HotData{
    RuleId::Butcher,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_BONUSES),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ON_HIT_ROLL_6,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS) |
    static_cast<TraitMask>(RuleTrait::BYPASSES_REGENERATION)
};

const RuleHotData Slam_HotData{
    RuleId::Slam,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_WOUNDS) |
    static_cast<TraitMask>(RuleTrait::IGNORES_COVER)
};

const RuleHotData Quake_HotData{
    RuleId::Quake,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_WOUNDS) |
    static_cast<TraitMask>(RuleTrait::BYPASSES_REGENERATION)
};

const RuleHotData Tear_HotData{
    RuleId::Tear,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::LATE,
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP)
};

const RuleHotData Scratch_HotData{
    RuleId::Scratch,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::LATE,
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP)
};

const RuleHotData Puncture_HotData{
    RuleId::Puncture,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::LATE,
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP) |
    static_cast<TraitMask>(RuleTrait::BYPASSES_REGENERATION)
};

const RuleHotData Shatter_HotData{
    RuleId::Shatter,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::LATE,
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP) |
    static_cast<TraitMask>(RuleTrait::BYPASSES_REGENERATION)
};

const RuleHotData Demolish_HotData{
    RuleId::Demolish,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::LATE,
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP) |
    static_cast<TraitMask>(RuleTrait::IGNORES_COVER)
};

const RuleHotData Impale_HotData{
    RuleId::Impale,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::WOUND_ALLOCATION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::LATE,
    static_cast<TraitMask>(RuleTrait::MULTIPLIES_WOUNDS)
};

const RuleHotData Skewer_HotData{
    RuleId::Skewer,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::WOUND_ALLOCATION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::LATE,
    static_cast<TraitMask>(RuleTrait::MULTIPLIES_WOUNDS) |
    static_cast<TraitMask>(RuleTrait::IGNORES_COVER)
};

const RuleHotData Reap_HotData{
    RuleId::Reap,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::LATE,
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP)
};

const RuleHotData Disintegrate_HotData{
    RuleId::Disintegrate,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::LATE,
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP) |
    static_cast<TraitMask>(RuleTrait::BYPASSES_REGENERATION)
};

const RuleHotData Decimate_HotData{
    RuleId::Decimate,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::LATE,
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP) |
    static_cast<TraitMask>(RuleTrait::IGNORES_COVER)
};

const RuleHotData Fragment_HotData{
    RuleId::Fragment,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::LATE,
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP) |
    static_cast<TraitMask>(RuleTrait::IGNORES_COVER)
};

const RuleHotData Wreck_HotData{
    RuleId::Wreck,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::FORCES_DEFENSE_REROLL) |
    static_cast<TraitMask>(RuleTrait::IGNORES_COVER)
};

// ==============================================================================
// Category C: Movement Bonus Hot Data
// ==============================================================================

const RuleHotData Lustbound_HotData{
    RuleId::Lustbound,
    GamePhase::MOVEMENT,
    static_cast<u8>(MoveSubPhase::CALCULATE_DISTANCE),
    CombatType::BOTH,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    0
};

const RuleHotData Highborn_HotData{
    RuleId::Highborn,
    GamePhase::MOVEMENT,
    static_cast<u8>(MoveSubPhase::CALCULATE_DISTANCE),
    CombatType::BOTH,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    0
};

const RuleHotData Scurry_HotData{
    RuleId::Scurry,
    GamePhase::MOVEMENT,
    static_cast<u8>(MoveSubPhase::CALCULATE_DISTANCE),
    CombatType::BOTH,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    0
};

const RuleHotData Darkborn_HotData{
    RuleId::Darkborn,
    GamePhase::MOVEMENT,
    static_cast<u8>(MoveSubPhase::CHARGE_RESOLVE),
    CombatType::BOTH,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    0
};

const RuleHotData HitAndRunShooter_HotData{
    RuleId::HitAndRunShooter,
    GamePhase::MOVEMENT,
    static_cast<u8>(MoveSubPhase::POST_MOVE),
    CombatType::SHOOTING,
    Target::SELF,
    Trigger::AFTER_COMBAT,
    RulePriority::NORMAL,
    0
};

// ==============================================================================
// Category D: Defense Modifier Hot Data
// ==============================================================================

const RuleHotData Fortified_HotData{
    RuleId::Fortified,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::DEFENDER,
    Trigger::ALWAYS,
    RulePriority::EARLY,
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP)
};

const RuleHotData Guardian_HotData{
    RuleId::Guardian,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::DEFENDER,
    Trigger::ALWAYS,
    RulePriority::EARLY,
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP)
};

const RuleHotData GuardianBoost_HotData{
    RuleId::GuardianBoost,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::DEFENDER,
    Trigger::ALWAYS,
    RulePriority::EARLY,
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP)
};

const RuleHotData Sturdy_HotData{
    RuleId::Sturdy,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::DEFENDER,
    Trigger::ALWAYS,
    RulePriority::EARLY,
    static_cast<TraitMask>(RuleTrait::MODIFIES_DEFENSE)
};

const RuleHotData Knightborn_HotData{
    RuleId::Knightborn,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::WOUND_ALLOCATION),
    CombatType::BOTH,
    Target::DEFENDER,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    0  // Wound ignore effect
};

const RuleHotData Plaguebound_HotData{
    RuleId::Plaguebound,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::WOUND_ALLOCATION),
    CombatType::BOTH,
    Target::DEFENDER,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    0  // Wound ignore effect
};

const RuleHotData Changebound_HotData{
    RuleId::Changebound,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_MODIFIERS),
    CombatType::BOTH,
    Target::DEFENDER,
    Trigger::ALWAYS,
    RulePriority::EARLY,
    static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL)
};

// ==============================================================================
// Category E: Extra Attack Generation Hot Data
// ==============================================================================

const RuleHotData Bloodborn_HotData{
    RuleId::Bloodborn,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_BONUSES),
    CombatType::BOTH,
    Target::ATTACKER,  // Unit rule like Furious, not weapon rule
    Trigger::ON_HIT_ROLL_6,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS)
};

const RuleHotData TargetingVisor_HotData{
    RuleId::TargetingVisor,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_MODIFIERS),
    CombatType::SHOOTING,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL)
};

const RuleHotData Havocbound_HotData{
    RuleId::Havocbound,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP)
};

const RuleHotData Warbound_HotData{
    RuleId::Warbound,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_WOUNDS)
};

const RuleHotData BrutalFighter_HotData{
    RuleId::BrutalFighter,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_BONUSES),
    CombatType::MELEE,
    Target::SELF,
    Trigger::ON_HIT_ROLL_6,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS)
};

// ==============================================================================
// Category F: Combat Choice & Retaliation Hot Data
// ==============================================================================

const RuleHotData Unpredictable_HotData{
    RuleId::Unpredictable,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_MODIFIERS),
    CombatType::BOTH,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL) |
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP)
};

const RuleHotData UnpredictableFighter_HotData{
    RuleId::UnpredictableFighter,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_MODIFIERS),
    CombatType::MELEE,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL) |
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP)
};

const RuleHotData UnpredictableShooter_HotData{
    RuleId::UnpredictableShooter,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_MODIFIERS),
    CombatType::SHOOTING,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL) |
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP)
};

const RuleHotData Retaliate_HotData{
    RuleId::Retaliate,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::WOUND_ALLOCATION),
    CombatType::MELEE,
    Target::DEFENDER,
    Trigger::WHEN_WOUNDED,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS)
};

const RuleHotData Deathstrike_HotData{
    RuleId::Deathstrike,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::WOUND_ALLOCATION),
    CombatType::MELEE,
    Target::DEFENDER,
    Trigger::ON_MODEL_DEATH,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS)
};

// ==============================================================================
// Category G: Enhanced Combat Modifiers Hot Data
// ==============================================================================

const RuleHotData FerociousBoost_HotData{
    RuleId::FerociousBoost,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_BONUSES),
    CombatType::BOTH,
    Target::SELF,
    Trigger::ON_HIT_ROLL_6,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS)
};

const RuleHotData ChangeboundBoost_HotData{
    RuleId::ChangeboundBoost,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_MODIFIERS),
    CombatType::BOTH,
    Target::DEFENDER,
    Trigger::ALWAYS,
    RulePriority::EARLY,
    static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL)
};

const RuleHotData WarboundBoost_HotData{
    RuleId::WarboundBoost,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_WOUNDS)
};

const RuleHotData PlaegueboundBoost_HotData{
    RuleId::PlaegueboundBoost,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::WOUND_ALLOCATION),
    CombatType::BOTH,
    Target::DEFENDER,
    Trigger::WHEN_WOUNDED,
    RulePriority::NORMAL,
    0  // Wound ignore effect
};

const RuleHotData LustboundBoost_HotData{
    RuleId::LustboundBoost,
    GamePhase::MOVEMENT,
    0,
    CombatType::BOTH,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MODIFIES_MOVEMENT)
};

const RuleHotData MeleeSlayer_HotData{
    RuleId::MeleeSlayer,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::MELEE,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP)
};

const RuleHotData HeavyImpact_HotData{
    RuleId::HeavyImpact,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::MELEE,
    Target::WEAPON,
    Trigger::ON_CHARGE,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP)
};

const RuleHotData Watchborn_HotData{
    RuleId::Watchborn,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::PRE_ATTACK),
    CombatType::BOTH,
    Target::ATTACKER,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL) |
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP)
};

// ==============================================================================
// Category H: Dice-Based Special Attacks Hot Data
// ==============================================================================

const RuleHotData Crush_HotData{
    RuleId::Crush,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::HIT_BONUSES),
    CombatType::MELEE,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS) |
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP)
};

const RuleHotData Ravage_HotData{
    RuleId::Ravage,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::WOUND_ALLOCATION),
    CombatType::MELEE,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_WOUNDS)
};

const RuleHotData Hazardous_HotData{
    RuleId::Hazardous,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::BOTH,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MODIFIES_AP)
};

const RuleHotData QuakeWhenShooting_HotData{
    RuleId::QuakeWhenShooting,
    GamePhase::COMBAT,
    static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION),
    CombatType::SHOOTING,
    Target::WEAPON,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_WOUNDS) |
    static_cast<TraitMask>(RuleTrait::BYPASSES_REGENERATION)
};

const RuleHotData Harassing_HotData{
    RuleId::Harassing,
    GamePhase::MOVEMENT,
    0,
    CombatType::BOTH,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MODIFIES_MOVEMENT)
};

const RuleHotData Guerrilla_HotData{
    RuleId::Guerrilla,
    GamePhase::MOVEMENT,
    0,
    CombatType::BOTH,
    Target::SELF,
    Trigger::ALWAYS,
    RulePriority::NORMAL,
    static_cast<TraitMask>(RuleTrait::MODIFIES_MOVEMENT)
};

// ==============================================================================
// Category I-O: Batch Implementation Hot Data
// ==============================================================================

// Category I: Aura Rules Hot Data
const RuleHotData VersatileReachAura_HotData{RuleId::VersatileReachAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::PRE_ATTACK), CombatType::MELEE, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT)};
const RuleHotData UnpredictableFighterAura_HotData{RuleId::UnpredictableFighterAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::MELEE, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT) | static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL)};
const RuleHotData PointBlankPiercingAura_HotData{RuleId::PointBlankPiercingAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::SHOOTING, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT) | static_cast<TraitMask>(RuleTrait::MODIFIES_AP)};
const RuleHotData RangedSlayerAura_HotData{RuleId::RangedSlayerAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::SHOOTING, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT) | static_cast<TraitMask>(RuleTrait::MODIFIES_AP)};
const RuleHotData TargetingVisorBoostAura_HotData{RuleId::TargetingVisorBoostAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::SHOOTING, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT) | static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL)};
const RuleHotData PiercingHunterAura_HotData{RuleId::PiercingHunterAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT) | static_cast<TraitMask>(RuleTrait::MODIFIES_AP)};
const RuleHotData ClanWarriorBoostAura_HotData{RuleId::ClanWarriorBoostAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_BONUSES), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT) | static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS)};
const RuleHotData PrecisionFighterAura_HotData{RuleId::PrecisionFighterAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::MELEE, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT) | static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL)};
const RuleHotData MischievousBoostAura_HotData{RuleId::MischievousBoostAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT) | static_cast<TraitMask>(RuleTrait::FORCES_DEFENSE_REROLL)};
const RuleHotData QuickShotAura_HotData{RuleId::QuickShotAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::SHOOTING, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT) | static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL)};
const RuleHotData HavocboundBoostAura_HotData{RuleId::HavocboundBoostAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT) | static_cast<TraitMask>(RuleTrait::MODIFIES_AP)};
const RuleHotData WarboundBoostAura_HotData{RuleId::WarboundBoostAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::WOUND_ALLOCATION), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT) | static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_WOUNDS)};
const RuleHotData InfectedBoostAura_HotData{RuleId::InfectedBoostAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::WOUND_ALLOCATION), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT) | static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_WOUNDS)};
const RuleHotData UnpredictableShooterAura_HotData{RuleId::UnpredictableShooterAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::SHOOTING, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT) | static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL)};
const RuleHotData ScrapperBoostAura_HotData{RuleId::ScrapperBoostAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT) | static_cast<TraitMask>(RuleTrait::FORCES_DEFENSE_REROLL)};
const RuleHotData FerociousBoostAura_HotData{RuleId::FerociousBoostAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_BONUSES), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT) | static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS)};
const RuleHotData PiercingFighterAura_HotData{RuleId::PiercingFighterAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::MELEE, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT) | static_cast<TraitMask>(RuleTrait::MODIFIES_AP)};
const RuleHotData PrecisionChargeAura_HotData{RuleId::PrecisionChargeAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::MELEE, Target::SELF, Trigger::ON_CHARGE, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT) | static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL) | static_cast<TraitMask>(RuleTrait::CHARGE_ONLY)};
const RuleHotData GroundedPrecisionAura_HotData{RuleId::GroundedPrecisionAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT) | static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL)};
const RuleHotData MeleeSlayerAura_HotData{RuleId::MeleeSlayerAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::MELEE, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT) | static_cast<TraitMask>(RuleTrait::MODIFIES_AP)};
const RuleHotData PiercingShooterAura_HotData{RuleId::PiercingShooterAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::SHOOTING, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT) | static_cast<TraitMask>(RuleTrait::MODIFIES_AP)};

// Category J: Buff Rules Hot Data
const RuleHotData RighteousFury_HotData{RuleId::RighteousFury, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::MELEE, Target::SELF, Trigger::ON_CHARGE, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::MODIFIES_AP) | static_cast<TraitMask>(RuleTrait::CHARGE_ONLY)};
const RuleHotData PrecisionFighterBuff_HotData{RuleId::PrecisionFighterBuff, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::MELEE, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL)};
const RuleHotData PrecisionShooterBuff_HotData{RuleId::PrecisionShooterBuff, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::SHOOTING, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL)};
const RuleHotData BaneInMeleeBuff_HotData{RuleId::BaneInMeleeBuff, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::MELEE, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::BYPASSES_REGENERATION) | static_cast<TraitMask>(RuleTrait::FORCES_DEFENSE_REROLL)};
const RuleHotData PrimalBoostBuff_HotData{RuleId::PrimalBoostBuff, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_BONUSES), CombatType::BOTH, Target::SELF, Trigger::ON_HIT_ROLL_6, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS)};
const RuleHotData PrecisionAttacksBuff_HotData{RuleId::PrecisionAttacksBuff, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL)};

// Category K: Mark Rules Hot Data
const RuleHotData UnpredictableFighterMark_HotData{RuleId::UnpredictableFighterMark, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::MELEE, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL) | static_cast<TraitMask>(RuleTrait::MODIFIES_AP)};
const RuleHotData UnstoppableShootingMark_HotData{RuleId::UnstoppableShootingMark, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::SHOOTING, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::BYPASSES_REGENERATION)};
const RuleHotData ShredMark_HotData{RuleId::ShredMark, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::WOUND_ALLOCATION), CombatType::BOTH, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_WOUNDS) | static_cast<TraitMask>(RuleTrait::BYPASSES_REGENERATION)};
const RuleHotData PiercingShootingMark_HotData{RuleId::PiercingShootingMark, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::SHOOTING, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::MODIFIES_AP)};
const RuleHotData PrecisionFightingMark_HotData{RuleId::PrecisionFightingMark, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::MELEE, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL)};
const RuleHotData SlayerMark_HotData{RuleId::SlayerMark, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::BOTH, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::MODIFIES_AP)};
const RuleHotData RelentlessMark_HotData{RuleId::RelentlessMark, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_BONUSES), CombatType::SHOOTING, Target::DEFENDER, Trigger::ON_HIT_ROLL_6, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS)};
const RuleHotData RendingInMeleeMark_HotData{RuleId::RendingInMeleeMark, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_SEPARATION), CombatType::MELEE, Target::DEFENDER, Trigger::ON_HIT_ROLL_6, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::MODIFIES_AP) | static_cast<TraitMask>(RuleTrait::BYPASSES_REGENERATION)};
const RuleHotData FuriousMark_HotData{RuleId::FuriousMark, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_BONUSES), CombatType::MELEE, Target::DEFENDER, Trigger::ON_HIT_ROLL_6, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS) | static_cast<TraitMask>(RuleTrait::CHARGE_ONLY)};
const RuleHotData IndirectMark_HotData{RuleId::IndirectMark, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::SHOOTING, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::IGNORES_COVER)};
const RuleHotData BaneMark_HotData{RuleId::BaneMark, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::BOTH, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::BYPASSES_REGENERATION) | static_cast<TraitMask>(RuleTrait::FORCES_DEFENSE_REROLL)};
const RuleHotData PrecisionShootingMark_HotData{RuleId::PrecisionShootingMark, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::SHOOTING, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL)};
const RuleHotData QuickShotMark_HotData{RuleId::QuickShotMark, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::SHOOTING, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL)};

// Category L: Debuff Rules Hot Data
const RuleHotData UnwieldyDebuff_HotData{RuleId::UnwieldyDebuff, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::MELEE, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL)};
const RuleHotData PrecisionDebuff_HotData{RuleId::PrecisionDebuff, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::BOTH, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL)};
const RuleHotData PiercingShootingDebuff_HotData{RuleId::PiercingShootingDebuff, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::SHOOTING, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::MODIFIES_AP)};

// Category M: Scaling/Frenzy/Growth Rules Hot Data
const RuleHotData PiercingGrowth_HotData{RuleId::PiercingGrowth, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::MODIFIES_AP)};
const RuleHotData RuinousFrenzy_HotData{RuleId::RuinousFrenzy, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL) | static_cast<TraitMask>(RuleTrait::MODIFIES_DEFENSE)};
const RuleHotData DevastatingFrenzy_HotData{RuleId::DevastatingFrenzy, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::MODIFIES_AP) | static_cast<TraitMask>(RuleTrait::MODIFIES_DEFENSE)};
const RuleHotData PrecisionGrowth_HotData{RuleId::PrecisionGrowth, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL)};
const RuleHotData DestructiveFrenzy_HotData{RuleId::DestructiveFrenzy, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL) | static_cast<TraitMask>(RuleTrait::MODIFIES_AP)};

// Category N: Storm AoE Rules Hot Data
const RuleHotData StormOfChange_HotData{RuleId::StormOfChange, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::WOUND_ALLOCATION), CombatType::BOTH, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_WOUNDS) | static_cast<TraitMask>(RuleTrait::BYPASSES_REGENERATION)};
const RuleHotData StormOfLust_HotData{RuleId::StormOfLust, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_BONUSES), CombatType::BOTH, Target::DEFENDER, Trigger::ON_HIT_ROLL_6, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS)};
const RuleHotData StormOfPlague_HotData{RuleId::StormOfPlague, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::BOTH, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::BYPASSES_REGENERATION) | static_cast<TraitMask>(RuleTrait::FORCES_DEFENSE_REROLL)};
const RuleHotData StormOfWar_HotData{RuleId::StormOfWar, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::BOTH, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::MODIFIES_AP)};

// Category O: Special Combat Rules Hot Data
const RuleHotData BreathAttack_HotData{RuleId::BreathAttack, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::PRE_ATTACK), CombatType::BOTH, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS)};
const RuleHotData IncreasedShootingRange_HotData{RuleId::IncreasedShootingRange, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::PRE_ATTACK), CombatType::SHOOTING, Target::WEAPON, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData RegenerativeStrength_HotData{RuleId::RegenerativeStrength, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::PRE_ATTACK), CombatType::BOTH, Target::SELF, Trigger::WHEN_WOUNDED, RulePriority::NORMAL, 0};
const RuleHotData Strafing_HotData{RuleId::Strafing, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::PRE_ATTACK), CombatType::BOTH, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS)};
const RuleHotData SurpriseAttack_HotData{RuleId::SurpriseAttack, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::PRE_ATTACK), CombatType::BOTH, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS)};
const RuleHotData TakedownStrike_HotData{RuleId::TakedownStrike, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::ROLL_HITS), CombatType::BOTH, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::OVERRIDES_QUALITY)};
const RuleHotData Instinctive_HotData{RuleId::Instinctive, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL)};
const RuleHotData CrossingAttack_HotData{RuleId::CrossingAttack, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::PRE_ATTACK), CombatType::MELEE, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS)};
const RuleHotData QuickReadjustment_HotData{RuleId::QuickReadjustment, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::SHOOTING, Target::WEAPON, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData CrossingStrike_HotData{RuleId::CrossingStrike, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::PRE_ATTACK), CombatType::MELEE, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS)};
const RuleHotData SurprisePiercingShot_HotData{RuleId::SurprisePiercingShot, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::SHOOTING, Target::WEAPON, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::MODIFIES_AP)};
const RuleHotData MindWound_HotData{RuleId::MindWound, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::PRE_ATTACK), CombatType::BOTH, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS) | static_cast<TraitMask>(RuleTrait::IGNORES_COVER)};
const RuleHotData MobileArtillery_HotData{RuleId::MobileArtillery, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::SHOOTING, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL)};
const RuleHotData Vengeance_HotData{RuleId::Vengeance, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::PRE_ATTACK), CombatType::BOTH, Target::SELF, Trigger::ON_MODEL_DEATH, RulePriority::NORMAL, 0};

// Category P-W: Remaining Rules Hot Data
const RuleHotData MartialProwess_HotData{RuleId::MartialProwess, GamePhase::PASSIVE, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData Coordinate_HotData{RuleId::Coordinate, GamePhase::PASSIVE, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData InquisitorialAgent_HotData{RuleId::InquisitorialAgent, GamePhase::PASSIVE, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData DelayedAction_HotData{RuleId::DelayedAction, GamePhase::PASSIVE, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};

const RuleHotData SturdyBoostAura_HotData{RuleId::SturdyBoostAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT)};
const RuleHotData VersatileDefenseAura_HotData{RuleId::VersatileDefenseAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT)};
const RuleHotData ChangeboundBoostAura_HotData{RuleId::ChangeboundBoostAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::BOTH, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT)};
const RuleHotData PlageboundBoostAura_HotData{RuleId::PlageboundBoostAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::WOUND_ALLOCATION), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT)};
const RuleHotData DefensiveGrowthAura_HotData{RuleId::DefensiveGrowthAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT)};
const RuleHotData MachineFogBoostAura_HotData{RuleId::MachineFogBoostAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::BOTH, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT)};
const RuleHotData ReanimationAura_HotData{RuleId::ReanimationAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::WOUND_ALLOCATION), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT)};
const RuleHotData GroundedReinforcementAura_HotData{RuleId::GroundedReinforcementAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT)};
const RuleHotData DevoutBoostAura_HotData{RuleId::DevoutBoostAura, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_BONUSES), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT)};
const RuleHotData CourageAura_HotData{RuleId::CourageAura, GamePhase::END_ROUND, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT)};
const RuleHotData HoldTheLineBoostAura_HotData{RuleId::HoldTheLineBoostAura, GamePhase::END_ROUND, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT)};
const RuleHotData RapidRushAura_HotData{RuleId::RapidRushAura, GamePhase::MOVEMENT, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT)};
const RuleHotData HarassingBoostAura_HotData{RuleId::HarassingBoostAura, GamePhase::MOVEMENT, static_cast<u8>(MoveSubPhase::POST_MOVE), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT)};
const RuleHotData SwiftAura_HotData{RuleId::SwiftAura, GamePhase::MOVEMENT, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT)};
const RuleHotData RapidBlinkBoostAura_HotData{RuleId::RapidBlinkBoostAura, GamePhase::MOVEMENT, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT)};
const RuleHotData RapidAdvanceAura_HotData{RuleId::RapidAdvanceAura, GamePhase::MOVEMENT, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT)};
const RuleHotData LustboundBoostAura_HotData{RuleId::LustboundBoostAura, GamePhase::MOVEMENT, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT)};
const RuleHotData BoundingAura_HotData{RuleId::BoundingAura, GamePhase::MOVEMENT, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT)};
const RuleHotData HighbornBoostAura_HotData{RuleId::HighbornBoostAura, GamePhase::MOVEMENT, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT)};
const RuleHotData SpeedFeatAura_HotData{RuleId::SpeedFeatAura, GamePhase::MOVEMENT, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT)};
const RuleHotData ScurryBoostAura_HotData{RuleId::ScurryBoostAura, GamePhase::MOVEMENT, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT)};
const RuleHotData GuerrillaBoostAura_HotData{RuleId::GuerrillaBoostAura, GamePhase::MOVEMENT, static_cast<u8>(MoveSubPhase::POST_MOVE), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::AURA_EFFECT)};

const RuleHotData StealthBuff_HotData{RuleId::StealthBuff, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::SHOOTING, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData ProtectiveDome_HotData{RuleId::ProtectiveDome, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData GuardedBuff_HotData{RuleId::GuardedBuff, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData RegenerationBuff_HotData{RuleId::RegenerationBuff, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::WOUND_ALLOCATION), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData EntrenchedBuff_HotData{RuleId::EntrenchedBuff, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData SelfRepairBoostBuff_HotData{RuleId::SelfRepairBoostBuff, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::WOUND_ALLOCATION), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData CourageBuff_HotData{RuleId::CourageBuff, GamePhase::END_ROUND, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData SteadfastBuff_HotData{RuleId::SteadfastBuff, GamePhase::END_ROUND, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData NoRetreatBuff_HotData{RuleId::NoRetreatBuff, GamePhase::END_ROUND, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData RapidAdvanceBuff_HotData{RuleId::RapidAdvanceBuff, GamePhase::MOVEMENT, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData SwiftBuff_HotData{RuleId::SwiftBuff, GamePhase::MOVEMENT, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData SpeedBuff_HotData{RuleId::SpeedBuff, GamePhase::MOVEMENT, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData IncreasedShootingRangeMark_HotData{RuleId::IncreasedShootingRangeMark, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::PRE_ATTACK), CombatType::SHOOTING, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData ExtendedBuffRange_HotData{RuleId::ExtendedBuffRange, GamePhase::PASSIVE, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData IncreasedShootingRangeBuff_HotData{RuleId::IncreasedShootingRangeBuff, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::PRE_ATTACK), CombatType::SHOOTING, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData CastingBuff_HotData{RuleId::CastingBuff, GamePhase::COMBAT, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};

const RuleHotData FatigueDebuff_HotData{RuleId::FatigueDebuff, GamePhase::PASSIVE, 0, CombatType::BOTH, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData DefenseDebuff_HotData{RuleId::DefenseDebuff, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::BOTH, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData MoraleDebuff_HotData{RuleId::MoraleDebuff, GamePhase::END_ROUND, 0, CombatType::BOTH, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData SpeedDebuff_HotData{RuleId::SpeedDebuff, GamePhase::MOVEMENT, 0, CombatType::BOTH, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData DangerousTerrainDebuff_HotData{RuleId::DangerousTerrainDebuff, GamePhase::MOVEMENT, 0, CombatType::BOTH, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData MindControl_HotData{RuleId::MindControl, GamePhase::END_ROUND, 0, CombatType::BOTH, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData DifficultTerrainDebuff_HotData{RuleId::DifficultTerrainDebuff, GamePhase::MOVEMENT, 0, CombatType::BOTH, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData CastingDebuff_HotData{RuleId::CastingDebuff, GamePhase::COMBAT, 0, CombatType::BOTH, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, 0};

const RuleHotData FortifiedGrowth_HotData{RuleId::FortifiedGrowth, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData GroundedStealth_HotData{RuleId::GroundedStealth, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::BOTH, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData ProtectionFeat_HotData{RuleId::ProtectionFeat, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::WOUND_ALLOCATION), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};

const RuleHotData Infiltrate_HotData{RuleId::Infiltrate, GamePhase::DEPLOYMENT, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData Spawn_HotData{RuleId::Spawn, GamePhase::DEPLOYMENT, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData ReDeployment_HotData{RuleId::ReDeployment, GamePhase::DEPLOYMENT, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData RapidAmbush_HotData{RuleId::RapidAmbush, GamePhase::DEPLOYMENT, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData AmbushBeacon_HotData{RuleId::AmbushBeacon, GamePhase::DEPLOYMENT, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData AmbushReDeployment_HotData{RuleId::AmbushReDeployment, GamePhase::DEPLOYMENT, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData RepelAmbushers_HotData{RuleId::RepelAmbushers, GamePhase::DEPLOYMENT, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData Reinforcement_HotData{RuleId::Reinforcement, GamePhase::END_ROUND, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData Fanatic_HotData{RuleId::Fanatic, GamePhase::DEPLOYMENT, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData Split_HotData{RuleId::Split, GamePhase::END_ROUND, 0, CombatType::BOTH, Target::SELF, Trigger::ON_MODEL_DEATH, RulePriority::NORMAL, 0};

const RuleHotData Teleport_HotData{RuleId::Teleport, GamePhase::MOVEMENT, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData Wolfborn_HotData{RuleId::Wolfborn, GamePhase::MOVEMENT, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
// Note: HitAndRunShooter_HotData already defined above
const RuleHotData RapidBlink_HotData{RuleId::RapidBlink, GamePhase::MOVEMENT, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData Bounding_HotData{RuleId::Bounding, GamePhase::MOVEMENT, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};

const RuleHotData DevoutBoost_HotData{RuleId::DevoutBoost, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_BONUSES), CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS)};
const RuleHotData Mend_HotData{RuleId::Mend, GamePhase::PASSIVE, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData HiveBond_HotData{RuleId::HiveBond, GamePhase::END_ROUND, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData RePositionArtillery_HotData{RuleId::RePositionArtillery, GamePhase::MOVEMENT, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};

// Category X: Spell & Targeting Rules Hot Data
const RuleHotData CasterGroup_HotData{RuleId::CasterGroup, GamePhase::PASSIVE, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData SpellConduit_HotData{RuleId::SpellConduit, GamePhase::PASSIVE, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData SpellAccumulator_HotData{RuleId::SpellAccumulator, GamePhase::PASSIVE, 0, CombatType::BOTH, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData PrecisionTarget_HotData{RuleId::PrecisionTarget, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::BOTH, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData PiercingTarget_HotData{RuleId::PiercingTarget, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::BOTH, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::MODIFIES_AP)};
const RuleHotData PrecisionSpotter_HotData{RuleId::PrecisionSpotter, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::SHOOTING, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData PiercingSpotter_HotData{RuleId::PiercingSpotter, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::SHOOTING, Target::SELF, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::MODIFIES_AP)};
const RuleHotData PrecisionTag_HotData{RuleId::PrecisionTag, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::HIT_MODIFIERS), CombatType::BOTH, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, 0};
const RuleHotData PiercingTag_HotData{RuleId::PiercingTag, GamePhase::COMBAT, static_cast<u8>(CombatSubPhase::DEFENSE_RESOLUTION), CombatType::BOTH, Target::DEFENDER, Trigger::ALWAYS, RulePriority::NORMAL, static_cast<TraitMask>(RuleTrait::MODIFIES_AP)};

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
// Category A: Simple Modifier Cold Data
// ==============================================================================

const RuleColdData Evasive_ColdData{
    "Evasive",
    nullptr,
    0,
    "Evasive: always -1 to hit",
    "Enemies attacking this unit always get -1 to hit."
};

const RuleColdData Steadfast_ColdData{
    "Steadfast",
    nullptr,
    0,
    "Steadfast: rally on 4+",
    "At the start of each round, roll 4+ to stop being Shaken."
};

const RuleColdData Swift_ColdData{
    "Swift",
    nullptr,
    0,
    "Swift: ignore Slow",
    "This unit ignores the Slow rule."
};

const RuleColdData Ferocious_ColdData{
    "Ferocious",
    nullptr,
    0,
    "Ferocious: extra hit on 6s",
    "Unmodified 6s to hit generate an extra hit."
};

const RuleColdData Lacerate_ColdData{
    "Lacerate",
    nullptr,
    0,
    "Lacerate: reroll defense 6s",
    "Defender must reroll successful defense rolls of 6."
};

const RuleColdData Mischievous_ColdData{
    "Mischievous",
    nullptr,
    0,
    "Mischievous: reroll defense 6s",
    "Defender must reroll successful defense rolls of 6."
};

const RuleColdData Scrapper_ColdData{
    "Scrapper",
    nullptr,
    0,
    "Scrapper: reroll defense 6s",
    "Defender must reroll successful defense rolls of 6."
};

// ==============================================================================
// Category B: Weapon Conditional Cold Data
// ==============================================================================

const RuleColdData Bash_ColdData{
    "Bash",
    nullptr,
    0,
    "Bash: +Blast(3) vs heavy armor",
    "Gains Blast(3) vs targets with Defense 5+ or 6+."
};

const RuleColdData Thrash_ColdData{
    "Thrash",
    nullptr,
    0,
    "Thrash: +Blast(3) vs heavy armor",
    "Gains Blast(3) vs targets with Defense 5+ or 6+."
};

const RuleColdData Crack_ColdData{
    "Crack",
    nullptr,
    0,
    "Crack: AP(+2) on 6s",
    "Unmodified 6s to hit gain AP(+2)."
};

const RuleColdData Destructive_ColdData{
    "Destructive",
    nullptr,
    0,
    "Destructive: AP(+4) on 6s",
    "Unmodified 6s to hit gain AP(+4)."
};

const RuleColdData Fracture_ColdData{
    "Fracture",
    nullptr,
    0,
    "Fracture: AP(+2) on 6s, ignores cover",
    "Unmodified 6s to hit gain AP(+2). Ignores cover."
};

const RuleColdData Break_ColdData{
    "Break",
    nullptr,
    0,
    "Break: AP(+2) on 6s, ignores regen",
    "Unmodified 6s to hit gain AP(+2). Bypasses regeneration."
};

const RuleColdData Slash_ColdData{
    "Slash",
    nullptr,
    0,
    "Slash: extra hit on 6s, ignores cover",
    "Unmodified 6s to hit generate an extra hit. Ignores cover."
};

const RuleColdData Butcher_ColdData{
    "Butcher",
    nullptr,
    0,
    "Butcher: extra hit on 6s, ignores regen",
    "Unmodified 6s to hit generate an extra hit. Bypasses regeneration."
};

const RuleColdData Slam_ColdData{
    "Slam",
    nullptr,
    0,
    "Slam: extra wound on 1s, ignores cover",
    "Unmodified defense roll of 1 causes extra wound. Ignores cover."
};

const RuleColdData Quake_ColdData{
    "Quake",
    nullptr,
    0,
    "Quake: extra wound on 1s, ignores regen",
    "Unmodified defense roll of 1 causes extra wound. Bypasses regeneration."
};

const RuleColdData Tear_ColdData{
    "Tear",
    nullptr,
    0,
    "Tear: AP(+4) vs Tough(3-9)",
    "Gains AP(+4) against targets with Tough(3-9)."
};

const RuleColdData Scratch_ColdData{
    "Scratch",
    nullptr,
    0,
    "Scratch: AP(+2) vs Tough(3-9)",
    "Gains AP(+2) against targets with Tough(3-9)."
};

const RuleColdData Puncture_ColdData{
    "Puncture",
    nullptr,
    0,
    "Puncture: AP(+4) vs Tough, ignores regen",
    "Gains AP(+4) vs Tough(3-9). Bypasses regeneration."
};

const RuleColdData Shatter_ColdData{
    "Shatter",
    nullptr,
    0,
    "Shatter: AP(+2) vs Tough, ignores regen",
    "Gains AP(+2) vs Tough(3-9). Bypasses regeneration."
};

const RuleColdData Demolish_ColdData{
    "Demolish",
    nullptr,
    0,
    "Demolish: AP(+2) vs Tough, ignores cover",
    "Gains AP(+2) vs Tough(3-9). Ignores cover."
};

const RuleColdData Impale_ColdData{
    "Impale",
    nullptr,
    0,
    "Impale: Deadly(+3) vs Tough(3-9)",
    "Gains Deadly(+3) against targets with Tough(3-9)."
};

const RuleColdData Skewer_ColdData{
    "Skewer",
    nullptr,
    0,
    "Skewer: Deadly(+3) vs Tough, ignores cover",
    "Gains Deadly(+3) vs Tough(3-9). Ignores cover."
};

const RuleColdData Reap_ColdData{
    "Reap",
    nullptr,
    0,
    "Reap: AP(+2) vs light armor",
    "Gains AP(+2) against targets with Defense 2-3+."
};

const RuleColdData Disintegrate_ColdData{
    "Disintegrate",
    nullptr,
    0,
    "Disintegrate: AP(+2) vs light, ignores regen",
    "Gains AP(+2) vs Defense 2-3+. Bypasses regeneration."
};

const RuleColdData Decimate_ColdData{
    "Decimate",
    nullptr,
    0,
    "Decimate: AP(+2) vs light, ignores cover",
    "Gains AP(+2) vs Defense 2-3+. Ignores cover."
};

const RuleColdData Fragment_ColdData{
    "Fragment",
    nullptr,
    0,
    "Fragment: AP(+1) vs light/medium, ignores cover",
    "Gains AP(+1) vs Defense 2-4+. Ignores cover."
};

const RuleColdData Wreck_ColdData{
    "Wreck",
    nullptr,
    0,
    "Wreck: reroll 6s, ignores cover",
    "Force reroll of defense 6s. Ignores cover."
};

// ==============================================================================
// Category C: Movement Bonus Cold Data
// ==============================================================================

const RuleColdData Lustbound_ColdData{
    "Lustbound",
    nullptr,
    0,
    "Lustbound: +1\" Adv, +3\" Rush/Charge",
    "Gains +1\" to Advance and +3\" to Rush/Charge."
};

const RuleColdData Highborn_ColdData{
    "Highborn",
    nullptr,
    0,
    "Highborn: +2\" Adv, +2\" Rush/Charge",
    "Gains +2\" to Advance and +2\" to Rush/Charge."
};

const RuleColdData Scurry_ColdData{
    "Scurry",
    nullptr,
    0,
    "Scurry: +2\" Adv, +2\" Rush/Charge",
    "Gains +2\" to Advance and +2\" to Rush/Charge."
};

const RuleColdData Darkborn_ColdData{
    "Darkborn",
    nullptr,
    0,
    "Darkborn: +3\" range, +3\" Charge",
    "Gains +3\" to weapon range and +3\" to Charge."
};

const RuleColdData HitAndRunShooter_ColdData{
    "HitAndRunShooter",
    nullptr,
    0,
    "Hit & Run Shooter: move 3\" after shooting",
    "Can move 3\" after shooting."
};

// ==============================================================================
// Category D: Defense Modifier Cold Data
// ==============================================================================

const RuleColdData Fortified_ColdData{
    "Fortified",
    nullptr,
    0,
    "Fortified: AP(-1) on all hits",
    "All hits count as having AP(-1), to a min of AP(0)."
};

const RuleColdData Guardian_ColdData{
    "Guardian",
    nullptr,
    0,
    "Guardian: AP(-1) from >9\"",
    "When shot/charged from over 9\" away, hits count as AP(-1)."
};

const RuleColdData GuardianBoost_ColdData{
    "GuardianBoost",
    nullptr,
    0,
    "Guardian Boost: AP(-1) always",
    "All hits count as AP(-1), regardless of distance."
};

const RuleColdData Sturdy_ColdData{
    "Sturdy",
    nullptr,
    0,
    "Sturdy: +1 defense from >9\"",
    "When shot/charged from over 9\" away, +1 to defense rolls."
};

const RuleColdData Knightborn_ColdData{
    "Knightborn",
    nullptr,
    0,
    "Knightborn: 6+ ignore wounds",
    "Roll 6+ to ignore wounds (4+ vs spells)."
};

const RuleColdData Plaguebound_ColdData{
    "Plaguebound",
    nullptr,
    0,
    "Plaguebound: 6+ ignore wounds",
    "Roll 6+ to ignore each wound."
};

const RuleColdData Changebound_ColdData{
    "Changebound",
    nullptr,
    0,
    "Changebound: -1 to hit from >9\"",
    "Enemies shooting/charging from over 9\" away get -1 to hit."
};

// ==============================================================================
// Category E: Extra Attack Generation Cold Data
// ==============================================================================

const RuleColdData Bloodborn_ColdData{
    "Bloodborn",
    nullptr,
    0,
    "Bloodborn: +1 attack on 6s",
    "Unmodified 6s to hit generate +1 attack (non-recursive)."
};

const RuleColdData TargetingVisor_ColdData{
    "TargetingVisor",
    nullptr,
    0,
    "Targeting Visor: +1 hit from >9\"",
    "Get +1 to hit when shooting at enemies over 9\" away."
};

const RuleColdData Havocbound_ColdData{
    "Havocbound",
    nullptr,
    0,
    "Havocbound: AP(+1) at range/charge",
    "Get AP(+1) when shooting over 9\" or charging."
};

const RuleColdData Warbound_ColdData{
    "Warbound",
    nullptr,
    0,
    "Warbound: extra wound on def 1s",
    "Unmodified defense roll of 1 causes an extra wound."
};

const RuleColdData BrutalFighter_ColdData{
    "BrutalFighter",
    nullptr,
    0,
    "Brutal Fighter: extra hit on 6s in melee",
    "Unmodified 6s to hit deal 1 extra hit in melee."
};

// ==============================================================================
// Category F: Combat Choice & Retaliation Cold Data
// ==============================================================================

const RuleColdData Unpredictable_ColdData{
    "Unpredictable",
    nullptr,
    0,
    "Unpredictable: D6 for AP or hit",
    "Roll D6: 1-3 = AP(+1), 4-6 = +1 to hit."
};

const RuleColdData UnpredictableFighter_ColdData{
    "UnpredictableFighter",
    nullptr,
    0,
    "Unpredictable Fighter: D6 in melee",
    "Roll D6 in melee: 1-3 = AP(+1), 4-6 = +1 to hit."
};

const RuleColdData UnpredictableShooter_ColdData{
    "UnpredictableShooter",
    nullptr,
    0,
    "Unpredictable Shooter: D6 when shooting",
    "Roll D6 when shooting: 1-3 = AP(+1), 4-6 = +1 to hit."
};

const RuleColdData Retaliate_ColdData{
    "Retaliate",
    nullptr,
    0,
    "Retaliate(X): X hits when wounded",
    "When this model takes a wound in melee, the attacker takes X hits."
};

const RuleColdData Deathstrike_ColdData{
    "Deathstrike",
    nullptr,
    0,
    "Deathstrike(X): X hits when killed",
    "When this model is killed in melee, the attacker takes X hits."
};

// ==============================================================================
// Category G: Enhanced Combat Modifiers Cold Data
// ==============================================================================

const RuleColdData FerociousBoost_ColdData{
    "FerociousBoost",
    nullptr,
    0,
    "Ferocious Boost: extra hits on 5-6",
    "Unmodified 5-6 to hit deal extra hits."
};

const RuleColdData ChangeboundBoost_ColdData{
    "ChangeboundBoost",
    nullptr,
    0,
    "Changebound Boost: -1 to hit always",
    "Enemies always get -1 to hit (not just from >9\")."
};

const RuleColdData WarboundBoost_ColdData{
    "WarboundBoost",
    nullptr,
    0,
    "Warbound Boost: wound on def 1-2",
    "Unmodified defense roll of 1-2 causes extra wound."
};

const RuleColdData PlaegueboundBoost_ColdData{
    "PlaegueboundBoost",
    nullptr,
    0,
    "Plaguebound Boost: 5-6 ignore wounds",
    "Roll 5-6 to ignore each wound."
};

const RuleColdData LustboundBoost_ColdData{
    "LustboundBoost",
    nullptr,
    0,
    "Lustbound Boost: +2/+6\" movement",
    "Get +2\" Advance and +6\" Rush/Charge."
};

const RuleColdData MeleeSlayer_ColdData{
    "MeleeSlayer",
    nullptr,
    0,
    "Melee Slayer: AP+2 vs Tough",
    "Get AP(+2) against targets with Tough(3+) in melee."
};

const RuleColdData HeavyImpact_ColdData{
    "HeavyImpact",
    nullptr,
    0,
    "Heavy Impact: Impact gets AP(1)",
    "Impact hits have AP(1)."
};

const RuleColdData Watchborn_ColdData{
    "Watchborn",
    nullptr,
    0,
    "Watchborn: pick AP or hit bonus",
    "Pick AP(+1) or +1 to hit when activated."
};

// ==============================================================================
// Category H: Dice-Based Special Attacks Cold Data
// ==============================================================================

const RuleColdData Crush_ColdData{
    "Crush",
    nullptr,
    0,
    "Crush: roll dice for AP(2) hits",
    "Roll X dice in melee, each 4+ is an AP(2) hit."
};

const RuleColdData Ravage_ColdData{
    "Ravage",
    nullptr,
    0,
    "Ravage: roll dice for wounds",
    "Roll X dice in melee, each 6+ causes a wound."
};

const RuleColdData Hazardous_ColdData{
    "Hazardous",
    nullptr,
    0,
    "Hazardous: AP(4) with self-damage",
    "Attacks have AP(4) but take wound on hit roll of 1."
};

const RuleColdData QuakeWhenShooting_ColdData{
    "QuakeWhenShooting",
    nullptr,
    0,
    "Quake when Shooting: Shred effect",
    "Defense 1s cause extra wounds when shooting. Bypasses regeneration."
};

const RuleColdData Harassing_ColdData{
    "Harassing",
    nullptr,
    0,
    "Harassing: 3\" post-combat move",
    "May move 3\" after shooting or melee."
};

const RuleColdData Guerrilla_ColdData{
    "Guerrilla",
    nullptr,
    0,
    "Guerrilla: 3\" post-combat move",
    "May move 3\" after shooting or melee (once per round)."
};

// ==============================================================================
// Category I-O: Batch Implementation Cold Data
// ==============================================================================

// Category I: Aura Cold Data
const RuleColdData VersatileReachAura_ColdData{"VersatileReachAura", nullptr, 0, "Versatile Reach Aura", "Unit gains extended melee range."};
const RuleColdData UnpredictableFighterAura_ColdData{"UnpredictableFighterAura", nullptr, 0, "Unpredictable Fighter Aura", "Unit gains Unpredictable Fighter."};
const RuleColdData PointBlankPiercingAura_ColdData{"PointBlankPiercingAura", nullptr, 0, "Point-Blank Piercing Aura", "Unit gains AP bonus at close range."};
const RuleColdData RangedSlayerAura_ColdData{"RangedSlayerAura", nullptr, 0, "Ranged Slayer Aura", "Unit gains Ranged Slayer."};
const RuleColdData TargetingVisorBoostAura_ColdData{"TargetingVisorBoostAura", nullptr, 0, "Targeting Visor Boost Aura", "Unit gains enhanced Targeting Visor."};
const RuleColdData PiercingHunterAura_ColdData{"PiercingHunterAura", nullptr, 0, "Piercing Hunter Aura", "Unit gains Piercing Hunter."};
const RuleColdData ClanWarriorBoostAura_ColdData{"ClanWarriorBoostAura", nullptr, 0, "Clan Warrior Boost Aura", "Unit gains enhanced Clan Warrior."};
const RuleColdData PrecisionFighterAura_ColdData{"PrecisionFighterAura", nullptr, 0, "Precision Fighter Aura", "Unit gains +1 to hit in melee."};
const RuleColdData MischievousBoostAura_ColdData{"MischievousBoostAura", nullptr, 0, "Mischievous Boost Aura", "Unit gains enhanced Mischievous."};
const RuleColdData QuickShotAura_ColdData{"QuickShotAura", nullptr, 0, "Quick Shot Aura", "Unit gains Quick Shot."};
const RuleColdData HavocboundBoostAura_ColdData{"HavocboundBoostAura", nullptr, 0, "Havocbound Boost Aura", "Unit gains enhanced Havocbound."};
const RuleColdData WarboundBoostAura_ColdData{"WarboundBoostAura", nullptr, 0, "Warbound Boost Aura", "Unit gains enhanced Warbound."};
const RuleColdData InfectedBoostAura_ColdData{"InfectedBoostAura", nullptr, 0, "Infected Boost Aura", "Unit gains enhanced Infected."};
const RuleColdData UnpredictableShooterAura_ColdData{"UnpredictableShooterAura", nullptr, 0, "Unpredictable Shooter Aura", "Unit gains Unpredictable Shooter."};
const RuleColdData ScrapperBoostAura_ColdData{"ScrapperBoostAura", nullptr, 0, "Scrapper Boost Aura", "Unit gains enhanced Scrapper."};
const RuleColdData FerociousBoostAura_ColdData{"FerociousBoostAura", nullptr, 0, "Ferocious Boost Aura", "Unit gains enhanced Ferocious."};
const RuleColdData PiercingFighterAura_ColdData{"PiercingFighterAura", nullptr, 0, "Piercing Fighter Aura", "Unit gains AP+1 in melee."};
const RuleColdData PrecisionChargeAura_ColdData{"PrecisionChargeAura", nullptr, 0, "Precision Charge Aura", "Unit gains +1 to hit when charging."};
const RuleColdData GroundedPrecisionAura_ColdData{"GroundedPrecisionAura", nullptr, 0, "Grounded Precision Aura", "Unit gains hit bonus near terrain."};
const RuleColdData MeleeSlayerAura_ColdData{"MeleeSlayerAura", nullptr, 0, "Melee Slayer Aura", "Unit gains Melee Slayer."};
const RuleColdData PiercingShooterAura_ColdData{"PiercingShooterAura", nullptr, 0, "Piercing Shooter Aura", "Unit gains AP+1 when shooting."};

// Category J: Buff Cold Data
const RuleColdData RighteousFury_ColdData{"RighteousFury", nullptr, 0, "Righteous Fury", "Grant Piercing Assault to friendlies."};
const RuleColdData PrecisionFighterBuff_ColdData{"PrecisionFighterBuff", nullptr, 0, "Precision Fighter Buff", "Grant +1 to hit in melee to friendly."};
const RuleColdData PrecisionShooterBuff_ColdData{"PrecisionShooterBuff", nullptr, 0, "Precision Shooter Buff", "Grant +1 to hit shooting to friendly."};
const RuleColdData BaneInMeleeBuff_ColdData{"BaneInMeleeBuff", nullptr, 0, "Bane in Melee Buff", "Grant Bane in melee to friendly."};
const RuleColdData PrimalBoostBuff_ColdData{"PrimalBoostBuff", nullptr, 0, "Primal Boost Buff", "Grant enhanced Primal to friendly."};
const RuleColdData PrecisionAttacksBuff_ColdData{"PrecisionAttacksBuff", nullptr, 0, "Precision Attacks Buff", "Grant +1 to hit to friendly."};

// Category K: Mark Cold Data
const RuleColdData UnpredictableFighterMark_ColdData{"UnpredictableFighterMark", nullptr, 0, "Unpredictable Fighter Mark", "Grant Unpredictable Fighter vs target."};
const RuleColdData UnstoppableShootingMark_ColdData{"UnstoppableShootingMark", nullptr, 0, "Unstoppable Shooting Mark", "Grant Unstoppable when shooting vs target."};
const RuleColdData ShredMark_ColdData{"ShredMark", nullptr, 0, "Shred Mark", "Grant Shred vs target."};
const RuleColdData PiercingShootingMark_ColdData{"PiercingShootingMark", nullptr, 0, "Piercing Shooting Mark", "Grant AP+1 shooting vs target."};
const RuleColdData PrecisionFightingMark_ColdData{"PrecisionFightingMark", nullptr, 0, "Precision Fighting Mark", "Grant +1 to hit in melee vs target."};
const RuleColdData SlayerMark_ColdData{"SlayerMark", nullptr, 0, "Slayer Mark", "Grant Slayer vs target."};
const RuleColdData RelentlessMark_ColdData{"RelentlessMark", nullptr, 0, "Relentless Mark", "Grant Relentless vs target."};
const RuleColdData RendingInMeleeMark_ColdData{"RendingInMeleeMark", nullptr, 0, "Rending in Melee Mark", "Grant Rending in melee vs target."};
const RuleColdData FuriousMark_ColdData{"FuriousMark", nullptr, 0, "Furious Mark", "Grant Furious vs target."};
const RuleColdData IndirectMark_ColdData{"IndirectMark", nullptr, 0, "Indirect Mark", "Grant Indirect vs target."};
const RuleColdData BaneMark_ColdData{"BaneMark", nullptr, 0, "Bane Mark", "Grant Bane vs target."};
const RuleColdData PrecisionShootingMark_ColdData{"PrecisionShootingMark", nullptr, 0, "Precision Shooting Mark", "Grant +1 to hit shooting vs target."};
const RuleColdData QuickShotMark_ColdData{"QuickShotMark", nullptr, 0, "Quick Shot Mark", "Grant Quick Shot vs target."};

// Category L: Debuff Cold Data
const RuleColdData UnwieldyDebuff_ColdData{"UnwieldyDebuff", nullptr, 0, "Unwieldy Debuff", "Grant Unwieldy to enemy in melee."};
const RuleColdData PrecisionDebuff_ColdData{"PrecisionDebuff", nullptr, 0, "Precision Debuff", "-1 to hit for enemy unit."};
const RuleColdData PiercingShootingDebuff_ColdData{"PiercingShootingDebuff", nullptr, 0, "Piercing Shooting Debuff", "Remove AP+1 from enemy shooting."};

// Category M: Frenzy/Growth Cold Data
const RuleColdData PiercingGrowth_ColdData{"PiercingGrowth", nullptr, 0, "Piercing Growth", "Marker-based AP scaling over rounds."};
const RuleColdData RuinousFrenzy_ColdData{"RuinousFrenzy", nullptr, 0, "Ruinous Frenzy", "Marker-based hit/defense bonus."};
const RuleColdData DevastatingFrenzy_ColdData{"DevastatingFrenzy", nullptr, 0, "Devastating Frenzy", "Marker-based AP/defense bonus."};
const RuleColdData PrecisionGrowth_ColdData{"PrecisionGrowth", nullptr, 0, "Precision Growth", "Marker-based hit bonus over rounds."};
const RuleColdData DestructiveFrenzy_ColdData{"DestructiveFrenzy", nullptr, 0, "Destructive Frenzy", "Marker-based hit/AP bonus."};

// Category N: Storm Cold Data
const RuleColdData StormOfChange_ColdData{"StormOfChange", nullptr, 0, "Storm of Change", "Roll 3 dice for AoE Shred hits."};
const RuleColdData StormOfLust_ColdData{"StormOfLust", nullptr, 0, "Storm of Lust", "Roll 3 dice for AoE Surge hits."};
const RuleColdData StormOfPlague_ColdData{"StormOfPlague", nullptr, 0, "Storm of Plague", "Roll 3 dice for AoE Bane hits."};
const RuleColdData StormOfWar_ColdData{"StormOfWar", nullptr, 0, "Storm of War", "Roll 3 dice for AoE AP(1) hits."};

// Category O: Special Combat Cold Data
const RuleColdData BreathAttack_ColdData{"BreathAttack", nullptr, 0, "Breath Attack", "Once per activation roll D6 for area damage."};
const RuleColdData IncreasedShootingRange_ColdData{"IncreasedShootingRange", nullptr, 0, "Increased Shooting Range", "+6\" range when shooting."};
const RuleColdData RegenerativeStrength_ColdData{"RegenerativeStrength", nullptr, 0, "Regenerative Strength", "Gain attacks when ignoring wounds."};
const RuleColdData Strafing_ColdData{"Strafing", nullptr, 0, "Strafing", "Attack when moving through enemy units."};
const RuleColdData SurpriseAttack_ColdData{"SurpriseAttack", nullptr, 0, "Surprise Attack", "Infiltrate variant with damage on deploy."};
const RuleColdData TakedownStrike_ColdData{"TakedownStrike", nullptr, 0, "Takedown Strike", "Once per game targeted attack at Quality 2+."};
const RuleColdData Instinctive_ColdData{"Instinctive", nullptr, 0, "Instinctive", "Must attack closest target with +1 hit."};
const RuleColdData CrossingAttack_ColdData{"CrossingAttack", nullptr, 0, "Crossing Attack", "Roll X dice moving through enemies for hits."};
const RuleColdData QuickReadjustment_ColdData{"QuickReadjustment", nullptr, 0, "Quick Readjustment", "Ignore Indirect move penalty."};
const RuleColdData CrossingStrike_ColdData{"CrossingStrike", nullptr, 0, "Crossing Strike", "Once per game attack through movement."};
const RuleColdData SurprisePiercingShot_ColdData{"SurprisePiercingShot", nullptr, 0, "Surprise Piercing Shot", "AP+2 when deploying via Ambush."};
const RuleColdData MindWound_ColdData{"MindWound", nullptr, 0, "Mind Wound", "Enemy takes 2 hits with Demolish."};
const RuleColdData MobileArtillery_ColdData{"MobileArtillery", nullptr, 0, "Mobile Artillery", "+1 hit from Hold, -2 to be hit when still."};
const RuleColdData Vengeance_ColdData{"Vengeance", nullptr, 0, "Vengeance", "Markers for hit bonus when destroyed."};

// ==============================================================================
// Category P-W: Remaining Rules Cold Data
// ==============================================================================

// Category P: Activation Rules Cold Data
const RuleColdData MartialProwess_ColdData{"MartialProwess", nullptr, 0, "Martial Prowess", "Activate twice per round (once per game)."};
const RuleColdData Coordinate_ColdData{"Coordinate", nullptr, 0, "Coordinate", "Activate another friendly unit after this."};
const RuleColdData InquisitorialAgent_ColdData{"InquisitorialAgent", nullptr, 0, "Inquisitorial Agent", "Activate twice per round (once per game)."};
const RuleColdData DelayedAction_ColdData{"DelayedAction", nullptr, 0, "Delayed Action", "Pass turn if opponent has more units."};

// Category Q: Remaining Boost Auras Cold Data
const RuleColdData SturdyBoostAura_ColdData{"SturdyBoostAura", nullptr, 0, "Sturdy Boost Aura", "Unit gets Sturdy Boost."};
const RuleColdData VersatileDefenseAura_ColdData{"VersatileDefenseAura", nullptr, 0, "Versatile Defense Aura", "Unit gets Versatile Defense."};
const RuleColdData ChangeboundBoostAura_ColdData{"ChangeboundBoostAura", nullptr, 0, "Changebound Boost Aura", "Unit gets -1 to be hit always."};
const RuleColdData PlageboundBoostAura_ColdData{"PlageboundBoostAura", nullptr, 0, "Plaguebound Boost Aura", "Unit gets 5-6 wound ignore."};
const RuleColdData DefensiveGrowthAura_ColdData{"DefensiveGrowthAura", nullptr, 0, "Defensive Growth Aura", "Unit gets scaling defense bonus."};
const RuleColdData MachineFogBoostAura_ColdData{"MachineFogBoostAura", nullptr, 0, "Machine-Fog Aura", "Unit gets fog obscurement."};
const RuleColdData ReanimationAura_ColdData{"ReanimationAura", nullptr, 0, "Reanimation Aura", "Unit gets model revival."};
const RuleColdData LustboundBoostAura_ColdData{"LustboundBoostAura", nullptr, 0, "Lustbound Boost Aura", "Unit gets +2/+6\" movement."};
const RuleColdData RapidRushAura_ColdData{"RapidRushAura", nullptr, 0, "Unstoppable Aura", "Unit gets Unstoppable."};
const RuleColdData GroundedReinforcementAura_ColdData{"GroundedReinforcementAura", nullptr, 0, "Grounded Defense Aura", "Unit gets +1 defense near terrain."};
const RuleColdData HoldTheLineBoostAura_ColdData{"HoldTheLineBoostAura", nullptr, 0, "Guardian Boost Aura", "Unit gets enhanced Guardian."};
const RuleColdData CourageAura_ColdData{"CourageAura", nullptr, 0, "Shield Wall Aura", "Unit gets Shield Wall."};
const RuleColdData DevoutBoostAura_ColdData{"DevoutBoostAura", nullptr, 0, "Devout Aura", "Unit gets extra hits on 5-6."};
const RuleColdData HarassingBoostAura_ColdData{"HarassingBoostAura", nullptr, 0, "Self-Repair Aura", "Unit gets automatic wound healing."};
const RuleColdData SwiftAura_ColdData{"SwiftAura", nullptr, 0, "Venomous Attack Aura", "Unit gets Poison attacks."};
const RuleColdData RapidBlinkBoostAura_ColdData{"RapidBlinkBoostAura", nullptr, 0, "Counter Aura", "Unit gets Counter."};
const RuleColdData RapidAdvanceAura_ColdData{"RapidAdvanceAura", nullptr, 0, "Infiltrate Aura", "Unit gets Infiltrate."};
const RuleColdData BoundingAura_ColdData{"BoundingAura", nullptr, 0, "Slow Aura", "Unit gets Slow."};
const RuleColdData HighbornBoostAura_ColdData{"HighbornBoostAura", nullptr, 0, "Takedown Aura", "Unit gets Takedown."};
const RuleColdData SpeedFeatAura_ColdData{"SpeedFeatAura", nullptr, 0, "Deadly Aura", "Unit gets Deadly attacks."};
const RuleColdData GuerrillaBoostAura_ColdData{"GuerrillaBoostAura", nullptr, 0, "Guerrilla Boost Aura", "Unit gets enhanced Guerrilla."};

// Category R: Remaining Buffs Cold Data
const RuleColdData StealthBuff_ColdData{"StealthBuff", nullptr, 0, "Stealth Buff", "Grant Stealth to friendly unit."};
const RuleColdData ProtectiveDome_ColdData{"ProtectiveDome", nullptr, 0, "Protective Dome Buff", "Grant dome protection to friendlies."};
const RuleColdData GuardedBuff_ColdData{"GuardedBuff", nullptr, 0, "Guarded Buff", "Grant Guarded to friendly unit."};
const RuleColdData RegenerationBuff_ColdData{"RegenerationBuff", nullptr, 0, "Regeneration Buff", "Grant Regeneration to friendly unit."};
const RuleColdData EntrenchedBuff_ColdData{"EntrenchedBuff", nullptr, 0, "Entrenched Buff", "Grant Entrenched to friendly unit."};
const RuleColdData SelfRepairBoostBuff_ColdData{"SelfRepairBoostBuff", nullptr, 0, "Self-Repair Boost Buff", "Grant enhanced Self-Repair."};
const RuleColdData CourageBuff_ColdData{"CourageBuff", nullptr, 0, "Courage Buff", "Grant morale immunity to friendlies."};
const RuleColdData SteadfastBuff_ColdData{"SteadfastBuff", nullptr, 0, "Fearless Buff", "Grant Fearless to friendly unit."};
const RuleColdData NoRetreatBuff_ColdData{"NoRetreatBuff", nullptr, 0, "Relentless Buff", "Grant Relentless to friendly unit."};
const RuleColdData RapidAdvanceBuff_ColdData{"RapidAdvanceBuff", nullptr, 0, "Fast Buff", "Grant Fast to friendly unit."};
const RuleColdData SwiftBuff_ColdData{"SwiftBuff", nullptr, 0, "Furious Buff", "Grant Furious to friendly unit."};
const RuleColdData SpeedBuff_ColdData{"SpeedBuff", nullptr, 0, "Indirect Buff", "Grant Indirect to friendly unit."};
const RuleColdData IncreasedShootingRangeMark_ColdData{"IncreasedShootingRangeMark", nullptr, 0, "Versatile Attack Buff", "Grant Versatile Attack to friendly."};
const RuleColdData ExtendedBuffRange_ColdData{"ExtendedBuffRange", nullptr, 0, "Resistance Buff", "Grant Resistance to friendly unit."};
const RuleColdData IncreasedShootingRangeBuff_ColdData{"IncreasedShootingRangeBuff", nullptr, 0, "Stride Buff", "Grant Strider to friendly unit."};
const RuleColdData CastingBuff_ColdData{"CastingBuff", nullptr, 0, "Casting Buff", "Grant casting bonus to friendly unit."};

// Category S: Remaining Debuffs Cold Data
const RuleColdData FatigueDebuff_ColdData{"FatigueDebuff", nullptr, 0, "Fatigue Debuff", "Force enemy to become fatigued."};
const RuleColdData DefenseDebuff_ColdData{"DefenseDebuff", nullptr, 0, "Defense Debuff", "-1 defense for enemy unit."};
const RuleColdData MoraleDebuff_ColdData{"MoraleDebuff", nullptr, 0, "Morale Debuff", "-1 morale for enemy unit."};
const RuleColdData SpeedDebuff_ColdData{"SpeedDebuff", nullptr, 0, "Speed Debuff", "Halve movement for enemy unit."};
const RuleColdData DangerousTerrainDebuff_ColdData{"DangerousTerrainDebuff", nullptr, 0, "Dangerous Terrain Debuff", "Enemy treats terrain as dangerous."};
const RuleColdData MindControl_ColdData{"MindControl", nullptr, 0, "Mind Control", "Control enemy unit for one turn."};
const RuleColdData DifficultTerrainDebuff_ColdData{"DifficultTerrainDebuff", nullptr, 0, "Difficult Terrain Debuff", "Enemy treats terrain as difficult."};
const RuleColdData CastingDebuff_ColdData{"CastingDebuff", nullptr, 0, "Casting Debuff", "Reduce casting ability for enemy."};

// Category T: Defense/Growth Rules Cold Data
const RuleColdData FortifiedGrowth_ColdData{"FortifiedGrowth", nullptr, 0, "Fortified Growth", "Marker-based AP reduction over rounds."};
const RuleColdData GroundedStealth_ColdData{"GroundedStealth", nullptr, 0, "Grounded Stealth", "-1 to be hit when near terrain."};
const RuleColdData ProtectionFeat_ColdData{"ProtectionFeat", nullptr, 0, "Protection Feat", "Once per game 4+ to ignore wounds."};

// Category U: Deployment Rules Cold Data
const RuleColdData Infiltrate_ColdData{"Infiltrate", nullptr, 0, "Infiltrate", "Ambush variant - deploy up to 1\" from enemy."};
const RuleColdData Spawn_ColdData{"Spawn", nullptr, 0, "Spawn", "Create new unit during game."};
const RuleColdData ReDeployment_ColdData{"ReDeployment", nullptr, 0, "Re-Deployment", "Move unit to new position."};
const RuleColdData RapidAmbush_ColdData{"RapidAmbush", nullptr, 0, "Rapid Ambush", "Ambush with immediate charge."};
const RuleColdData AmbushBeacon_ColdData{"AmbushBeacon", nullptr, 0, "Ambush Beacon", "Allow allies to Ambush near this unit."};
const RuleColdData AmbushReDeployment_ColdData{"AmbushReDeployment", nullptr, 0, "Ambush Re-Deployment", "Ambush and redeploy combination."};
const RuleColdData RepelAmbushers_ColdData{"RepelAmbushers", nullptr, 0, "Repel Ambushers", "Force Ambushers further away."};
const RuleColdData Reinforcement_ColdData{"Reinforcement", nullptr, 0, "Reinforcement", "Bring destroyed unit back at end of round."};
const RuleColdData Fanatic_ColdData{"Fanatic", nullptr, 0, "Fanatic", "Deploy forward with charge bonus."};
const RuleColdData Split_ColdData{"Split", nullptr, 0, "Split", "Spawn new units when model dies."};

// Category V: Movement Rules Cold Data
const RuleColdData Teleport_ColdData{"Teleport", nullptr, 0, "Teleport", "Place model anywhere within 6\" before attacking."};
const RuleColdData Wolfborn_ColdData{"Wolfborn", nullptr, 0, "Wolfborn", "Enhanced movement through terrain."};
// Note: HitAndRunShooter_ColdData already defined above
const RuleColdData RapidBlink_ColdData{"RapidBlink", nullptr, 0, "Rapid Blink", "Short teleport during movement."};
const RuleColdData Bounding_ColdData{"Bounding", nullptr, 0, "Bounding", "Extra movement during advance."};

// Category W: Faction/Morale/Misc Cold Data
const RuleColdData DevoutBoost_ColdData{"DevoutBoost", nullptr, 0, "Devout Boost", "Extra hits on 5-6 instead of just 6."};
const RuleColdData Mend_ColdData{"Mend", nullptr, 0, "Mend", "Remove D3 wounds from Tough model."};
const RuleColdData HiveBond_ColdData{"HiveBond", nullptr, 0, "Hive Bond", "Units with this rule get +1 morale."};
const RuleColdData RePositionArtillery_ColdData{"RePositionArtillery", nullptr, 0, "Re-Position Artillery", "Move Artillery model up to 9\"."};

// Category X: Spell & Targeting Rules Cold Data
const RuleColdData CasterGroup_ColdData{"CasterGroup", nullptr, 0, "Caster Group", "Complex caster pooling mechanic."};
const RuleColdData SpellConduit_ColdData{"SpellConduit", nullptr, 0, "Spell Conduit", "Casters within 12\" can cast from this position."};
const RuleColdData SpellAccumulator_ColdData{"SpellAccumulator", nullptr, 0, "Spell Accumulator", "Store and release spell energy."};
const RuleColdData PrecisionTarget_ColdData{"PrecisionTarget", nullptr, 0, "Precision Target", "Place markers for hit bonus vs target."};
const RuleColdData PiercingTarget_ColdData{"PiercingTarget", nullptr, 0, "Piercing Target", "Place markers for AP bonus vs target."};
const RuleColdData PrecisionSpotter_ColdData{"PrecisionSpotter", nullptr, 0, "Precision Spotter", "Spotter that grants hit bonus to allies."};
const RuleColdData PiercingSpotter_ColdData{"PiercingSpotter", nullptr, 0, "Piercing Spotter", "Spotter that grants AP bonus to allies."};
const RuleColdData PrecisionTag_ColdData{"PrecisionTag", nullptr, 0, "Precision Tag", "Mark target for precision bonus."};
const RuleColdData PiercingTag_ColdData{"PiercingTag", nullptr, 0, "Piercing Tag", "Mark target for piercing bonus."};

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
// Category A: Simple Modifier Effect Entries
// ==============================================================================

const RuleEffectEntry Evasive_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_MODIFIERS, evasive_effect)
    .build();

const RuleEffectEntry Steadfast_Effects = EffectBuilder()
    .endround(EndRoundSubPhase::MORALE, steadfast_morale_effect)
    .build();

const RuleEffectEntry Swift_Effects = EffectBuilder()
    .movement(MoveSubPhase::CALCULATE_DISTANCE, swift_effect)
    .build();

const RuleEffectEntry Ferocious_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_BONUSES, ferocious_effect)
    .build();

const RuleEffectEntry Lacerate_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, lacerate_effect)
    .build();

const RuleEffectEntry Mischievous_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, mischievous_effect)
    .build();

const RuleEffectEntry Scrapper_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, scrapper_effect)
    .build();

// ==============================================================================
// Category B: Weapon Conditional Effect Entries
// ==============================================================================

const RuleEffectEntry Bash_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, bash_effect)
    .build();

const RuleEffectEntry Thrash_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, thrash_effect)
    .build();

const RuleEffectEntry Crack_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_SEPARATION, crack_effect)
    .build();

const RuleEffectEntry Destructive_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_SEPARATION, destructive_effect)
    .build();

const RuleEffectEntry Fracture_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_SEPARATION, fracture_effect)
    .build();

const RuleEffectEntry Break_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_SEPARATION, break_effect)
    .build();

const RuleEffectEntry Slash_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_BONUSES, slash_effect)
    .build();

const RuleEffectEntry Butcher_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_BONUSES, butcher_effect)
    .build();

const RuleEffectEntry Slam_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, slam_effect)
    .build();

const RuleEffectEntry Quake_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, quake_effect)
    .build();

const RuleEffectEntry Tear_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, tear_effect)
    .condition(CombatSubPhase::DEFENSE_RESOLUTION, tough_target_condition)
    .build();

const RuleEffectEntry Scratch_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, scratch_effect)
    .condition(CombatSubPhase::DEFENSE_RESOLUTION, tough_target_condition)
    .build();

const RuleEffectEntry Puncture_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, puncture_effect)
    .condition(CombatSubPhase::DEFENSE_RESOLUTION, tough_target_condition)
    .build();

const RuleEffectEntry Shatter_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, shatter_effect)
    .condition(CombatSubPhase::DEFENSE_RESOLUTION, tough_target_condition)
    .build();

const RuleEffectEntry Demolish_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, demolish_effect)
    .condition(CombatSubPhase::DEFENSE_RESOLUTION, tough_target_condition)
    .build();

const RuleEffectEntry Impale_Effects = EffectBuilder()
    .combat(CombatSubPhase::WOUND_ALLOCATION, impale_effect)
    .condition(CombatSubPhase::WOUND_ALLOCATION, tough_target_condition)
    .build();

const RuleEffectEntry Skewer_Effects = EffectBuilder()
    .combat(CombatSubPhase::WOUND_ALLOCATION, skewer_effect)
    .condition(CombatSubPhase::WOUND_ALLOCATION, tough_target_condition)
    .build();

const RuleEffectEntry Reap_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, reap_effect)
    .condition(CombatSubPhase::DEFENSE_RESOLUTION, light_armor_condition)
    .build();

const RuleEffectEntry Disintegrate_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, disintegrate_effect)
    .condition(CombatSubPhase::DEFENSE_RESOLUTION, light_armor_condition)
    .build();

const RuleEffectEntry Decimate_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, decimate_effect)
    .condition(CombatSubPhase::DEFENSE_RESOLUTION, light_armor_condition)
    .build();

const RuleEffectEntry Fragment_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, fragment_effect)
    .condition(CombatSubPhase::DEFENSE_RESOLUTION, light_medium_armor_condition)
    .build();

const RuleEffectEntry Wreck_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, wreck_effect)
    .build();

// ==============================================================================
// Category C: Movement Bonus Effect Entries
// ==============================================================================

const RuleEffectEntry Lustbound_Effects = EffectBuilder()
    .movement(MoveSubPhase::CALCULATE_DISTANCE, lustbound_effect)
    .build();

const RuleEffectEntry Highborn_Effects = EffectBuilder()
    .movement(MoveSubPhase::CALCULATE_DISTANCE, highborn_effect)
    .build();

const RuleEffectEntry Scurry_Effects = EffectBuilder()
    .movement(MoveSubPhase::CALCULATE_DISTANCE, scurry_effect)
    .build();

const RuleEffectEntry Darkborn_Effects = EffectBuilder()
    .movement(MoveSubPhase::CHARGE_RESOLVE, darkborn_effect)
    .build();

const RuleEffectEntry HitAndRunShooter_Effects = EffectBuilder()
    .movement(MoveSubPhase::POST_MOVE, hit_and_run_shooter_effect)
    .build();

// ==============================================================================
// Category D: Defense Modifier Effect Entries
// ==============================================================================

const RuleEffectEntry Fortified_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, fortified_effect)
    .build();

const RuleEffectEntry Guardian_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, guardian_effect)
    .condition(CombatSubPhase::DEFENSE_RESOLUTION, distance_over_9_condition)
    .build();

const RuleEffectEntry GuardianBoost_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, guardian_boost_effect)
    .build();

const RuleEffectEntry Sturdy_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, sturdy_effect)
    .condition(CombatSubPhase::DEFENSE_RESOLUTION, distance_over_9_condition)
    .build();

const RuleEffectEntry Knightborn_Effects = EffectBuilder()
    .combat(CombatSubPhase::WOUND_ALLOCATION, knightborn_effect)
    .build();

const RuleEffectEntry Plaguebound_Effects = EffectBuilder()
    .combat(CombatSubPhase::WOUND_ALLOCATION, plaguebound_effect)
    .build();

const RuleEffectEntry Changebound_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_MODIFIERS, changebound_effect)
    .condition(CombatSubPhase::HIT_MODIFIERS, distance_over_9_condition)
    .build();

// ==============================================================================
// Category E: Extra Attack Generation Effect Entries
// ==============================================================================

const RuleEffectEntry Bloodborn_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_BONUSES, bloodborn_effect)
    .build();

const RuleEffectEntry TargetingVisor_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_MODIFIERS, targeting_visor_effect)
    .condition(CombatSubPhase::HIT_MODIFIERS, shooting_over_9_condition)
    .build();

const RuleEffectEntry Havocbound_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, havocbound_effect)
    .condition(CombatSubPhase::DEFENSE_RESOLUTION, havocbound_condition)
    .build();

const RuleEffectEntry Warbound_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, warbound_effect)
    .build();

const RuleEffectEntry BrutalFighter_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_BONUSES, brutal_fighter_effect)
    .condition(CombatSubPhase::HIT_BONUSES, melee_only_condition)
    .build();

// ==============================================================================
// Category F: Combat Choice & Retaliation Effect Entries
// ==============================================================================

const RuleEffectEntry Unpredictable_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_MODIFIERS, unpredictable_effect)
    .build();

const RuleEffectEntry UnpredictableFighter_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_MODIFIERS, unpredictable_fighter_effect)
    .condition(CombatSubPhase::HIT_MODIFIERS, melee_only_condition)
    .build();

const RuleEffectEntry UnpredictableShooter_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_MODIFIERS, unpredictable_shooter_effect)
    .condition(CombatSubPhase::HIT_MODIFIERS, shooting_only_condition)
    .build();

const RuleEffectEntry Retaliate_Effects = EffectBuilder()
    .combat(CombatSubPhase::WOUND_ALLOCATION, retaliate_effect)
    .condition(CombatSubPhase::WOUND_ALLOCATION, melee_only_condition)
    .build();

const RuleEffectEntry Deathstrike_Effects = EffectBuilder()
    .combat(CombatSubPhase::WOUND_ALLOCATION, deathstrike_effect)
    .condition(CombatSubPhase::WOUND_ALLOCATION, melee_only_condition)
    .build();

// ==============================================================================
// Category G: Enhanced Combat Modifiers Effect Entries
// ==============================================================================

const RuleEffectEntry FerociousBoost_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_BONUSES, ferocious_boost_effect)
    .build();

const RuleEffectEntry ChangeboundBoost_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_MODIFIERS, changebound_boost_effect)
    .build();

const RuleEffectEntry WarboundBoost_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, warbound_boost_effect)
    .build();

const RuleEffectEntry PlaegueboundBoost_Effects = EffectBuilder()
    .combat(CombatSubPhase::WOUND_ALLOCATION, plaguebound_boost_effect)
    .build();

const RuleEffectEntry MeleeSlayer_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, melee_slayer_effect)
    .condition(CombatSubPhase::DEFENSE_RESOLUTION, melee_only_condition)
    .build();

const RuleEffectEntry HeavyImpact_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, heavy_impact_effect)
    .build();

const RuleEffectEntry Watchborn_Effects = EffectBuilder()
    .combat(CombatSubPhase::PRE_ATTACK, watchborn_effect)
    .build();

// ==============================================================================
// Category H: Dice-Based Special Attacks Effect Entries
// ==============================================================================

const RuleEffectEntry Crush_Effects = EffectBuilder()
    .combat(CombatSubPhase::HIT_BONUSES, crush_effect)
    .condition(CombatSubPhase::HIT_BONUSES, melee_only_condition)
    .build();

const RuleEffectEntry Ravage_Effects = EffectBuilder()
    .combat(CombatSubPhase::WOUND_ALLOCATION, ravage_effect)
    .condition(CombatSubPhase::WOUND_ALLOCATION, melee_only_condition)
    .build();

const RuleEffectEntry Hazardous_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, hazardous_effect)
    .build();

const RuleEffectEntry QuakeWhenShooting_Effects = EffectBuilder()
    .combat(CombatSubPhase::DEFENSE_RESOLUTION, quake_when_shooting_effect)
    .condition(CombatSubPhase::DEFENSE_RESOLUTION, shooting_only_condition)
    .build();

const RuleEffectEntry Harassing_Effects = EffectBuilder()
    .movement(MoveSubPhase::POST_MOVE, harassing_effect)
    .build();

const RuleEffectEntry Guerrilla_Effects = EffectBuilder()
    .movement(MoveSubPhase::POST_MOVE, guerrilla_effect)
    .build();

// ==============================================================================
// Category I-O: Batch Implementation Effect Entries
// ==============================================================================

// Category I: Aura Effect Entries
const RuleEffectEntry VersatileReachAura_Effects = EffectBuilder().combat(CombatSubPhase::PRE_ATTACK, versatile_reach_aura_effect).build();
const RuleEffectEntry UnpredictableFighterAura_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, unpredictable_fighter_effect).condition(CombatSubPhase::HIT_MODIFIERS, melee_only_condition).build();
const RuleEffectEntry PointBlankPiercingAura_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, point_blank_surge_effect).condition(CombatSubPhase::DEFENSE_RESOLUTION, shooting_only_condition).build();
const RuleEffectEntry RangedSlayerAura_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, melee_slayer_effect).condition(CombatSubPhase::DEFENSE_RESOLUTION, shooting_only_condition).build();
const RuleEffectEntry TargetingVisorBoostAura_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, targeting_visor_effect).condition(CombatSubPhase::HIT_MODIFIERS, shooting_only_condition).build();
const RuleEffectEntry PiercingHunterAura_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, piercing_fighter_aura_effect).build();
const RuleEffectEntry ClanWarriorBoostAura_Effects = EffectBuilder().combat(CombatSubPhase::HIT_BONUSES, clan_warrior_boost_effect).build();
const RuleEffectEntry PrecisionFighterAura_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, precision_fighter_aura_effect).condition(CombatSubPhase::HIT_MODIFIERS, melee_only_condition).build();
const RuleEffectEntry MischievousBoostAura_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, lacerate_effect).build();
const RuleEffectEntry QuickShotAura_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, quick_shot_aura_effect).condition(CombatSubPhase::HIT_MODIFIERS, shooting_only_condition).build();
const RuleEffectEntry HavocboundBoostAura_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, havocbound_effect).build();
const RuleEffectEntry WarboundBoostAura_Effects = EffectBuilder().combat(CombatSubPhase::WOUND_ALLOCATION, warbound_boost_effect).build();
const RuleEffectEntry InfectedBoostAura_Effects = EffectBuilder().combat(CombatSubPhase::WOUND_ALLOCATION, warbound_boost_effect).build();
const RuleEffectEntry UnpredictableShooterAura_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, unpredictable_shooter_effect).condition(CombatSubPhase::HIT_MODIFIERS, shooting_only_condition).build();
const RuleEffectEntry ScrapperBoostAura_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, lacerate_effect).build();
const RuleEffectEntry FerociousBoostAura_Effects = EffectBuilder().combat(CombatSubPhase::HIT_BONUSES, ferocious_boost_effect).build();
const RuleEffectEntry PiercingFighterAura_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, piercing_fighter_aura_effect).condition(CombatSubPhase::DEFENSE_RESOLUTION, melee_only_condition).build();
const RuleEffectEntry PrecisionChargeAura_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, precision_charge_aura_effect).condition(CombatSubPhase::HIT_MODIFIERS, melee_only_condition).build();
const RuleEffectEntry GroundedPrecisionAura_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, precision_fighter_aura_effect).build();
const RuleEffectEntry MeleeSlayerAura_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, melee_slayer_effect).condition(CombatSubPhase::DEFENSE_RESOLUTION, melee_only_condition).build();
const RuleEffectEntry PiercingShooterAura_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, piercing_shooter_aura_effect).condition(CombatSubPhase::DEFENSE_RESOLUTION, shooting_only_condition).build();

// Category J: Buff Effect Entries
const RuleEffectEntry RighteousFury_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, righteous_fury_effect).build();
const RuleEffectEntry PrecisionFighterBuff_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, precision_fighter_buff_effect).condition(CombatSubPhase::HIT_MODIFIERS, melee_only_condition).build();
const RuleEffectEntry PrecisionShooterBuff_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, precision_shooter_buff_effect).condition(CombatSubPhase::HIT_MODIFIERS, shooting_only_condition).build();
const RuleEffectEntry BaneInMeleeBuff_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, bane_in_melee_buff_effect).condition(CombatSubPhase::DEFENSE_RESOLUTION, melee_only_condition).build();
const RuleEffectEntry PrimalBoostBuff_Effects = EffectBuilder().combat(CombatSubPhase::HIT_BONUSES, primal_boost_buff_effect).build();
const RuleEffectEntry PrecisionAttacksBuff_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, precision_attacks_buff_effect).build();

// Category K: Mark Effect Entries
const RuleEffectEntry UnpredictableFighterMark_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, unpredictable_fighter_effect).condition(CombatSubPhase::HIT_MODIFIERS, melee_only_condition).build();
const RuleEffectEntry UnstoppableShootingMark_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, unstoppable_effect).condition(CombatSubPhase::HIT_MODIFIERS, shooting_only_condition).build();
const RuleEffectEntry ShredMark_Effects = EffectBuilder().combat(CombatSubPhase::WOUND_ALLOCATION, shred_mark_effect).build();
const RuleEffectEntry PiercingShootingMark_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, piercing_shooting_mark_effect).condition(CombatSubPhase::DEFENSE_RESOLUTION, shooting_only_condition).build();
const RuleEffectEntry PrecisionFightingMark_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, precision_fighting_mark_effect).condition(CombatSubPhase::HIT_MODIFIERS, melee_only_condition).build();
const RuleEffectEntry SlayerMark_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, slayer_mark_effect).build();
const RuleEffectEntry RelentlessMark_Effects = EffectBuilder().combat(CombatSubPhase::HIT_BONUSES, relentless_mark_effect).condition(CombatSubPhase::HIT_BONUSES, shooting_only_condition).build();
const RuleEffectEntry RendingInMeleeMark_Effects = EffectBuilder().combat(CombatSubPhase::HIT_SEPARATION, rending_in_melee_mark_effect).condition(CombatSubPhase::HIT_SEPARATION, melee_only_condition).build();
const RuleEffectEntry FuriousMark_Effects = EffectBuilder().combat(CombatSubPhase::HIT_BONUSES, furious_mark_effect).condition(CombatSubPhase::HIT_BONUSES, melee_only_condition).build();
const RuleEffectEntry IndirectMark_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, indirect_mark_effect).condition(CombatSubPhase::HIT_MODIFIERS, shooting_only_condition).build();
const RuleEffectEntry BaneMark_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, bane_mark_effect).build();
const RuleEffectEntry PrecisionShootingMark_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, precision_shooting_mark_effect).condition(CombatSubPhase::HIT_MODIFIERS, shooting_only_condition).build();
const RuleEffectEntry QuickShotMark_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, quick_shot_mark_effect).condition(CombatSubPhase::HIT_MODIFIERS, shooting_only_condition).build();

// Category L: Debuff Effect Entries
const RuleEffectEntry UnwieldyDebuff_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, unwieldy_debuff_effect).condition(CombatSubPhase::HIT_MODIFIERS, melee_only_condition).build();
const RuleEffectEntry PrecisionDebuff_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, precision_debuff_effect).build();
const RuleEffectEntry PiercingShootingDebuff_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, piercing_shooting_debuff_effect).condition(CombatSubPhase::DEFENSE_RESOLUTION, shooting_only_condition).build();

// Category M: Frenzy/Growth Effect Entries
const RuleEffectEntry PiercingGrowth_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, piercing_growth_effect).build();
const RuleEffectEntry RuinousFrenzy_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, ruinous_frenzy_effect).build();
const RuleEffectEntry DevastatingFrenzy_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, devastating_frenzy_effect).build();
const RuleEffectEntry PrecisionGrowth_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, precision_growth_effect).build();
const RuleEffectEntry DestructiveFrenzy_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, destructive_frenzy_effect).build();

// Category N: Storm Effect Entries
const RuleEffectEntry StormOfChange_Effects = EffectBuilder().combat(CombatSubPhase::WOUND_ALLOCATION, storm_of_change_effect).build();
const RuleEffectEntry StormOfLust_Effects = EffectBuilder().combat(CombatSubPhase::HIT_BONUSES, storm_of_lust_effect).build();
const RuleEffectEntry StormOfPlague_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, storm_of_plague_effect).build();
const RuleEffectEntry StormOfWar_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, storm_of_war_effect).build();

// Category O: Special Combat Effect Entries
const RuleEffectEntry BreathAttack_Effects = EffectBuilder().combat(CombatSubPhase::PRE_ATTACK, breath_attack_effect).build();
const RuleEffectEntry IncreasedShootingRange_Effects = EffectBuilder().combat(CombatSubPhase::PRE_ATTACK, increased_shooting_range_effect).condition(CombatSubPhase::PRE_ATTACK, shooting_only_condition).build();
const RuleEffectEntry RegenerativeStrength_Effects = EffectBuilder().combat(CombatSubPhase::PRE_ATTACK, regenerative_strength_effect).build();
const RuleEffectEntry Strafing_Effects = EffectBuilder().combat(CombatSubPhase::PRE_ATTACK, strafing_effect).build();
const RuleEffectEntry SurpriseAttack_Effects = EffectBuilder().combat(CombatSubPhase::PRE_ATTACK, surprise_attack_effect).build();
const RuleEffectEntry TakedownStrike_Effects = EffectBuilder().combat(CombatSubPhase::ROLL_HITS, takedown_strike_effect).build();
const RuleEffectEntry Instinctive_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, instinctive_effect).build();
const RuleEffectEntry CrossingAttack_Effects = EffectBuilder().combat(CombatSubPhase::PRE_ATTACK, crossing_attack_effect).condition(CombatSubPhase::PRE_ATTACK, melee_only_condition).build();
const RuleEffectEntry QuickReadjustment_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, quick_readjustment_effect).condition(CombatSubPhase::HIT_MODIFIERS, shooting_only_condition).build();
const RuleEffectEntry CrossingStrike_Effects = EffectBuilder().combat(CombatSubPhase::PRE_ATTACK, crossing_strike_effect).condition(CombatSubPhase::PRE_ATTACK, melee_only_condition).build();
const RuleEffectEntry SurprisePiercingShot_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, surprise_piercing_shot_effect).condition(CombatSubPhase::DEFENSE_RESOLUTION, shooting_only_condition).build();
const RuleEffectEntry MindWound_Effects = EffectBuilder().combat(CombatSubPhase::PRE_ATTACK, mind_wound_effect).build();
const RuleEffectEntry MobileArtillery_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, mobile_artillery_effect).condition(CombatSubPhase::HIT_MODIFIERS, shooting_only_condition).build();
const RuleEffectEntry Vengeance_Effects = EffectBuilder().combat(CombatSubPhase::PRE_ATTACK, vengeance_effect).build();

// ==============================================================================
// Category P-W: Remaining Rules Effect Entries
// ==============================================================================

// Category P: Activation Rules (stub - handled at activation phase level)
const RuleEffectEntry MartialProwess_Effects = EffectBuilder().build();
const RuleEffectEntry Coordinate_Effects = EffectBuilder().build();
const RuleEffectEntry InquisitorialAgent_Effects = EffectBuilder().build();
const RuleEffectEntry DelayedAction_Effects = EffectBuilder().build();

// Category Q: Remaining Boost Auras Effect Entries - using correct function names
const RuleEffectEntry SturdyBoostAura_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, sturdy_boost_aura_effect).build();
const RuleEffectEntry VersatileDefenseAura_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, versatile_defense_aura_effect).build();
const RuleEffectEntry ChangeboundBoostAura_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, changebound_boost_aura_effect).build();
const RuleEffectEntry PlageboundBoostAura_Effects = EffectBuilder().combat(CombatSubPhase::WOUND_ALLOCATION, plaguebound_boost_aura_effect).build();
const RuleEffectEntry DefensiveGrowthAura_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, defensive_growth_aura_effect).build();
const RuleEffectEntry MachineFogBoostAura_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, machine_fog_boost_aura_effect).build();
const RuleEffectEntry ReanimationAura_Effects = EffectBuilder().build();  // End round handling
const RuleEffectEntry GroundedReinforcementAura_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, grounded_reinforcement_aura_effect).build();
const RuleEffectEntry DevoutBoostAura_Effects = EffectBuilder().combat(CombatSubPhase::HIT_BONUSES, devout_boost_aura_effect).build();
const RuleEffectEntry CourageAura_Effects = EffectBuilder().build();  // Morale handling
const RuleEffectEntry HoldTheLineBoostAura_Effects = EffectBuilder().build();  // Morale handling
const RuleEffectEntry RapidRushAura_Effects = EffectBuilder().build();  // Movement handling
const RuleEffectEntry HarassingBoostAura_Effects = EffectBuilder().build();  // Post-combat movement
const RuleEffectEntry SwiftAura_Effects = EffectBuilder().build();  // Movement handling
const RuleEffectEntry RapidBlinkBoostAura_Effects = EffectBuilder().build();  // Movement handling
const RuleEffectEntry RapidAdvanceAura_Effects = EffectBuilder().build();  // Movement handling
const RuleEffectEntry LustboundBoostAura_Effects = EffectBuilder().build();  // Movement handling
const RuleEffectEntry BoundingAura_Effects = EffectBuilder().build();  // Movement handling
const RuleEffectEntry HighbornBoostAura_Effects = EffectBuilder().build();  // Movement handling
const RuleEffectEntry SpeedFeatAura_Effects = EffectBuilder().build();  // Movement handling
const RuleEffectEntry ScurryBoostAura_Effects = EffectBuilder().build();  // Movement handling
const RuleEffectEntry GuerrillaBoostAura_Effects = EffectBuilder().build();  // Post-combat movement

// Category R: Remaining Buffs Effect Entries - using correct function names
const RuleEffectEntry StealthBuff_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, stealth_buff_effect).build();
const RuleEffectEntry ProtectiveDome_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, protective_dome_effect).build();
const RuleEffectEntry GuardedBuff_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, guarded_buff_effect).build();
const RuleEffectEntry RegenerationBuff_Effects = EffectBuilder().build();  // End round handling
const RuleEffectEntry EntrenchedBuff_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, entrenched_buff_effect).build();
const RuleEffectEntry SelfRepairBoostBuff_Effects = EffectBuilder().build();  // End round handling
const RuleEffectEntry CourageBuff_Effects = EffectBuilder().build();  // Morale handling
const RuleEffectEntry SteadfastBuff_Effects = EffectBuilder().build();  // Morale handling
const RuleEffectEntry NoRetreatBuff_Effects = EffectBuilder().build();  // Morale handling
const RuleEffectEntry RapidAdvanceBuff_Effects = EffectBuilder().build();  // Movement handling
const RuleEffectEntry SwiftBuff_Effects = EffectBuilder().build();  // Movement handling
const RuleEffectEntry SpeedBuff_Effects = EffectBuilder().build();  // Movement handling
const RuleEffectEntry IncreasedShootingRangeMark_Effects = EffectBuilder().build();  // Range modifier
const RuleEffectEntry ExtendedBuffRange_Effects = EffectBuilder().build();  // Buff range modifier
const RuleEffectEntry IncreasedShootingRangeBuff_Effects = EffectBuilder().build();  // Range modifier
const RuleEffectEntry CastingBuff_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, casting_buff_effect).build();

// Category S: Remaining Debuffs Effect Entries
const RuleEffectEntry FatigueDebuff_Effects = EffectBuilder().combat(CombatSubPhase::PRE_ATTACK, fatigue_debuff_effect).build();
const RuleEffectEntry DefenseDebuff_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, defense_debuff_effect).build();
const RuleEffectEntry MoraleDebuff_Effects = EffectBuilder().build();  // Morale handling
const RuleEffectEntry SpeedDebuff_Effects = EffectBuilder().build();  // Movement handling
const RuleEffectEntry DangerousTerrainDebuff_Effects = EffectBuilder().build();  // Movement handling
const RuleEffectEntry MindControl_Effects = EffectBuilder().build();  // Special handling
const RuleEffectEntry DifficultTerrainDebuff_Effects = EffectBuilder().build();  // Movement handling
const RuleEffectEntry CastingDebuff_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, casting_debuff_effect).build();

// Category T: Defense/Growth Rules Effect Entries
const RuleEffectEntry FortifiedGrowth_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, fortified_growth_effect).build();
const RuleEffectEntry GroundedStealth_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, grounded_stealth_effect).build();
const RuleEffectEntry ProtectionFeat_Effects = EffectBuilder().combat(CombatSubPhase::WOUND_ALLOCATION, protection_feat_effect).build();

// Category U: Deployment Rules Effect Entries (deployment handled separately)
const RuleEffectEntry Infiltrate_Effects = EffectBuilder().build();
const RuleEffectEntry Spawn_Effects = EffectBuilder().build();
const RuleEffectEntry ReDeployment_Effects = EffectBuilder().build();
const RuleEffectEntry RapidAmbush_Effects = EffectBuilder().build();
const RuleEffectEntry AmbushBeacon_Effects = EffectBuilder().build();
const RuleEffectEntry AmbushReDeployment_Effects = EffectBuilder().build();
const RuleEffectEntry RepelAmbushers_Effects = EffectBuilder().build();
const RuleEffectEntry Reinforcement_Effects = EffectBuilder().build();
const RuleEffectEntry Fanatic_Effects = EffectBuilder().build();
const RuleEffectEntry Split_Effects = EffectBuilder().build();

// Category V: Movement Rules Effect Entries (stubs - movement phase handled separately)
const RuleEffectEntry Teleport_Effects = EffectBuilder().build();
const RuleEffectEntry Wolfborn_Effects = EffectBuilder().build();
const RuleEffectEntry HitAndRunShooter_Cat_Effects = EffectBuilder().build();
const RuleEffectEntry RapidBlink_Effects = EffectBuilder().build();
const RuleEffectEntry Bounding_Effects = EffectBuilder().build();

// Category W: Faction/Morale/Misc Effect Entries
const RuleEffectEntry DevoutBoost_Effects = EffectBuilder().combat(CombatSubPhase::HIT_BONUSES, devout_boost_effect).build();
const RuleEffectEntry Mend_Effects = EffectBuilder().build();  // Activation handling
const RuleEffectEntry HiveBond_Effects = EffectBuilder().build();  // Morale handling
const RuleEffectEntry RePositionArtillery_Effects = EffectBuilder().build();  // Movement handling

// Category X: Spell & Targeting Rules Effect Entries
const RuleEffectEntry CasterGroup_Effects = EffectBuilder().build();  // Caster pooling mechanic
const RuleEffectEntry SpellConduit_Effects = EffectBuilder().build();  // Cast from this position
const RuleEffectEntry SpellAccumulator_Effects = EffectBuilder().build();  // Store spell energy
const RuleEffectEntry PrecisionTarget_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, precision_target_effect).build();
const RuleEffectEntry PiercingTarget_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, piercing_target_effect).build();
const RuleEffectEntry PrecisionSpotter_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, precision_spotter_effect).build();
const RuleEffectEntry PiercingSpotter_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, piercing_spotter_effect).build();
const RuleEffectEntry PrecisionTag_Effects = EffectBuilder().combat(CombatSubPhase::HIT_MODIFIERS, precision_tag_effect).build();
const RuleEffectEntry PiercingTag_Effects = EffectBuilder().combat(CombatSubPhase::DEFENSE_RESOLUTION, piercing_tag_effect).build();

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
    registry.register_hot_data(RuleId::CounterAttack, CounterAttack_HotData);
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

    // Category A: Simple Modifier hot data
    registry.register_hot_data(RuleId::Evasive, Evasive_HotData);
    registry.register_hot_data(RuleId::Steadfast, Steadfast_HotData);
    registry.register_hot_data(RuleId::Swift, Swift_HotData);
    registry.register_hot_data(RuleId::Ferocious, Ferocious_HotData);
    registry.register_hot_data(RuleId::Lacerate, Lacerate_HotData);
    registry.register_hot_data(RuleId::Mischievous, Mischievous_HotData);
    registry.register_hot_data(RuleId::Scrapper, Scrapper_HotData);

    // Category B: Weapon Conditional hot data
    registry.register_hot_data(RuleId::Bash, Bash_HotData);
    registry.register_hot_data(RuleId::Thrash, Thrash_HotData);
    registry.register_hot_data(RuleId::Crack, Crack_HotData);
    registry.register_hot_data(RuleId::Destructive, Destructive_HotData);
    registry.register_hot_data(RuleId::Fracture, Fracture_HotData);
    registry.register_hot_data(RuleId::Break, Break_HotData);
    registry.register_hot_data(RuleId::Slash, Slash_HotData);
    registry.register_hot_data(RuleId::Butcher, Butcher_HotData);
    registry.register_hot_data(RuleId::Slam, Slam_HotData);
    registry.register_hot_data(RuleId::Quake, Quake_HotData);
    registry.register_hot_data(RuleId::Tear, Tear_HotData);
    registry.register_hot_data(RuleId::Scratch, Scratch_HotData);
    registry.register_hot_data(RuleId::Puncture, Puncture_HotData);
    registry.register_hot_data(RuleId::Shatter, Shatter_HotData);
    registry.register_hot_data(RuleId::Demolish, Demolish_HotData);
    registry.register_hot_data(RuleId::Impale, Impale_HotData);
    registry.register_hot_data(RuleId::Skewer, Skewer_HotData);
    registry.register_hot_data(RuleId::Reap, Reap_HotData);
    registry.register_hot_data(RuleId::Disintegrate, Disintegrate_HotData);
    registry.register_hot_data(RuleId::Decimate, Decimate_HotData);
    registry.register_hot_data(RuleId::Fragment, Fragment_HotData);
    registry.register_hot_data(RuleId::Wreck, Wreck_HotData);

    // Category C: Movement Bonus hot data
    registry.register_hot_data(RuleId::Lustbound, Lustbound_HotData);
    registry.register_hot_data(RuleId::Highborn, Highborn_HotData);
    registry.register_hot_data(RuleId::Scurry, Scurry_HotData);
    registry.register_hot_data(RuleId::Darkborn, Darkborn_HotData);
    registry.register_hot_data(RuleId::HitAndRunShooter, HitAndRunShooter_HotData);

    // Category D: Defense Modifier hot data
    registry.register_hot_data(RuleId::Fortified, Fortified_HotData);
    registry.register_hot_data(RuleId::Guardian, Guardian_HotData);
    registry.register_hot_data(RuleId::GuardianBoost, GuardianBoost_HotData);
    registry.register_hot_data(RuleId::Sturdy, Sturdy_HotData);
    registry.register_hot_data(RuleId::Knightborn, Knightborn_HotData);
    registry.register_hot_data(RuleId::Plaguebound, Plaguebound_HotData);
    registry.register_hot_data(RuleId::Changebound, Changebound_HotData);

    // Category E: Extra Attack Generation hot data
    registry.register_hot_data(RuleId::Bloodborn, Bloodborn_HotData);
    registry.register_hot_data(RuleId::TargetingVisor, TargetingVisor_HotData);
    registry.register_hot_data(RuleId::Havocbound, Havocbound_HotData);
    registry.register_hot_data(RuleId::Warbound, Warbound_HotData);
    registry.register_hot_data(RuleId::BrutalFighter, BrutalFighter_HotData);

    // Category F: Combat Choice & Retaliation hot data
    registry.register_hot_data(RuleId::Unpredictable, Unpredictable_HotData);
    registry.register_hot_data(RuleId::UnpredictableFighter, UnpredictableFighter_HotData);
    registry.register_hot_data(RuleId::UnpredictableShooter, UnpredictableShooter_HotData);
    registry.register_hot_data(RuleId::Retaliate, Retaliate_HotData);
    registry.register_hot_data(RuleId::Deathstrike, Deathstrike_HotData);

    // Category G: Enhanced Combat Modifiers hot data
    registry.register_hot_data(RuleId::FerociousBoost, FerociousBoost_HotData);
    registry.register_hot_data(RuleId::ChangeboundBoost, ChangeboundBoost_HotData);
    registry.register_hot_data(RuleId::WarboundBoost, WarboundBoost_HotData);
    registry.register_hot_data(RuleId::PlaegueboundBoost, PlaegueboundBoost_HotData);
    registry.register_hot_data(RuleId::LustboundBoost, LustboundBoost_HotData);
    registry.register_hot_data(RuleId::MeleeSlayer, MeleeSlayer_HotData);
    registry.register_hot_data(RuleId::HeavyImpact, HeavyImpact_HotData);
    registry.register_hot_data(RuleId::Watchborn, Watchborn_HotData);

    // Category H: Dice-Based Special Attacks hot data
    registry.register_hot_data(RuleId::Crush, Crush_HotData);
    registry.register_hot_data(RuleId::Ravage, Ravage_HotData);
    registry.register_hot_data(RuleId::Hazardous, Hazardous_HotData);
    registry.register_hot_data(RuleId::QuakeWhenShooting, QuakeWhenShooting_HotData);
    registry.register_hot_data(RuleId::Harassing, Harassing_HotData);
    registry.register_hot_data(RuleId::Guerrilla, Guerrilla_HotData);

    // Category I-O: Batch hot data registration
    registry.register_hot_data(RuleId::VersatileReachAura, VersatileReachAura_HotData);
    registry.register_hot_data(RuleId::UnpredictableFighterAura, UnpredictableFighterAura_HotData);
    registry.register_hot_data(RuleId::PointBlankPiercingAura, PointBlankPiercingAura_HotData);
    registry.register_hot_data(RuleId::RangedSlayerAura, RangedSlayerAura_HotData);
    registry.register_hot_data(RuleId::TargetingVisorBoostAura, TargetingVisorBoostAura_HotData);
    registry.register_hot_data(RuleId::PiercingHunterAura, PiercingHunterAura_HotData);
    registry.register_hot_data(RuleId::ClanWarriorBoostAura, ClanWarriorBoostAura_HotData);
    registry.register_hot_data(RuleId::PrecisionFighterAura, PrecisionFighterAura_HotData);
    registry.register_hot_data(RuleId::MischievousBoostAura, MischievousBoostAura_HotData);
    registry.register_hot_data(RuleId::QuickShotAura, QuickShotAura_HotData);
    registry.register_hot_data(RuleId::HavocboundBoostAura, HavocboundBoostAura_HotData);
    registry.register_hot_data(RuleId::WarboundBoostAura, WarboundBoostAura_HotData);
    registry.register_hot_data(RuleId::InfectedBoostAura, InfectedBoostAura_HotData);
    registry.register_hot_data(RuleId::UnpredictableShooterAura, UnpredictableShooterAura_HotData);
    registry.register_hot_data(RuleId::ScrapperBoostAura, ScrapperBoostAura_HotData);
    registry.register_hot_data(RuleId::FerociousBoostAura, FerociousBoostAura_HotData);
    registry.register_hot_data(RuleId::PiercingFighterAura, PiercingFighterAura_HotData);
    registry.register_hot_data(RuleId::PrecisionChargeAura, PrecisionChargeAura_HotData);
    registry.register_hot_data(RuleId::GroundedPrecisionAura, GroundedPrecisionAura_HotData);
    registry.register_hot_data(RuleId::MeleeSlayerAura, MeleeSlayerAura_HotData);
    registry.register_hot_data(RuleId::PiercingShooterAura, PiercingShooterAura_HotData);
    registry.register_hot_data(RuleId::RighteousFury, RighteousFury_HotData);
    registry.register_hot_data(RuleId::PrecisionFighterBuff, PrecisionFighterBuff_HotData);
    registry.register_hot_data(RuleId::PrecisionShooterBuff, PrecisionShooterBuff_HotData);
    registry.register_hot_data(RuleId::BaneInMeleeBuff, BaneInMeleeBuff_HotData);
    registry.register_hot_data(RuleId::PrimalBoostBuff, PrimalBoostBuff_HotData);
    registry.register_hot_data(RuleId::PrecisionAttacksBuff, PrecisionAttacksBuff_HotData);
    registry.register_hot_data(RuleId::UnpredictableFighterMark, UnpredictableFighterMark_HotData);
    registry.register_hot_data(RuleId::UnstoppableShootingMark, UnstoppableShootingMark_HotData);
    registry.register_hot_data(RuleId::ShredMark, ShredMark_HotData);
    registry.register_hot_data(RuleId::PiercingShootingMark, PiercingShootingMark_HotData);
    registry.register_hot_data(RuleId::PrecisionFightingMark, PrecisionFightingMark_HotData);
    registry.register_hot_data(RuleId::SlayerMark, SlayerMark_HotData);
    registry.register_hot_data(RuleId::RelentlessMark, RelentlessMark_HotData);
    registry.register_hot_data(RuleId::RendingInMeleeMark, RendingInMeleeMark_HotData);
    registry.register_hot_data(RuleId::FuriousMark, FuriousMark_HotData);
    registry.register_hot_data(RuleId::IndirectMark, IndirectMark_HotData);
    registry.register_hot_data(RuleId::BaneMark, BaneMark_HotData);
    registry.register_hot_data(RuleId::PrecisionShootingMark, PrecisionShootingMark_HotData);
    registry.register_hot_data(RuleId::QuickShotMark, QuickShotMark_HotData);
    registry.register_hot_data(RuleId::UnwieldyDebuff, UnwieldyDebuff_HotData);
    registry.register_hot_data(RuleId::PrecisionDebuff, PrecisionDebuff_HotData);
    registry.register_hot_data(RuleId::PiercingShootingDebuff, PiercingShootingDebuff_HotData);
    registry.register_hot_data(RuleId::PiercingGrowth, PiercingGrowth_HotData);
    registry.register_hot_data(RuleId::RuinousFrenzy, RuinousFrenzy_HotData);
    registry.register_hot_data(RuleId::DevastatingFrenzy, DevastatingFrenzy_HotData);
    registry.register_hot_data(RuleId::PrecisionGrowth, PrecisionGrowth_HotData);
    registry.register_hot_data(RuleId::DestructiveFrenzy, DestructiveFrenzy_HotData);
    registry.register_hot_data(RuleId::StormOfChange, StormOfChange_HotData);
    registry.register_hot_data(RuleId::StormOfLust, StormOfLust_HotData);
    registry.register_hot_data(RuleId::StormOfPlague, StormOfPlague_HotData);
    registry.register_hot_data(RuleId::StormOfWar, StormOfWar_HotData);
    registry.register_hot_data(RuleId::BreathAttack, BreathAttack_HotData);
    registry.register_hot_data(RuleId::IncreasedShootingRange, IncreasedShootingRange_HotData);
    registry.register_hot_data(RuleId::RegenerativeStrength, RegenerativeStrength_HotData);
    registry.register_hot_data(RuleId::Strafing, Strafing_HotData);
    registry.register_hot_data(RuleId::SurpriseAttack, SurpriseAttack_HotData);
    registry.register_hot_data(RuleId::TakedownStrike, TakedownStrike_HotData);
    registry.register_hot_data(RuleId::Instinctive, Instinctive_HotData);
    registry.register_hot_data(RuleId::CrossingAttack, CrossingAttack_HotData);
    registry.register_hot_data(RuleId::QuickReadjustment, QuickReadjustment_HotData);
    registry.register_hot_data(RuleId::CrossingStrike, CrossingStrike_HotData);
    registry.register_hot_data(RuleId::SurprisePiercingShot, SurprisePiercingShot_HotData);
    registry.register_hot_data(RuleId::MindWound, MindWound_HotData);
    registry.register_hot_data(RuleId::MobileArtillery, MobileArtillery_HotData);
    registry.register_hot_data(RuleId::Vengeance, Vengeance_HotData);

    // === Category P: Activation Rules ===
    registry.register_hot_data(RuleId::MartialProwess, MartialProwess_HotData);
    registry.register_hot_data(RuleId::Coordinate, Coordinate_HotData);
    registry.register_hot_data(RuleId::InquisitorialAgent, InquisitorialAgent_HotData);
    registry.register_hot_data(RuleId::DelayedAction, DelayedAction_HotData);

    // === Category Q: Remaining Boost Auras ===
    registry.register_hot_data(RuleId::SturdyBoostAura, SturdyBoostAura_HotData);
    registry.register_hot_data(RuleId::VersatileDefenseAura, VersatileDefenseAura_HotData);
    registry.register_hot_data(RuleId::ChangeboundBoostAura, ChangeboundBoostAura_HotData);
    registry.register_hot_data(RuleId::PlageboundBoostAura, PlageboundBoostAura_HotData);
    registry.register_hot_data(RuleId::DefensiveGrowthAura, DefensiveGrowthAura_HotData);
    registry.register_hot_data(RuleId::MachineFogBoostAura, MachineFogBoostAura_HotData);
    registry.register_hot_data(RuleId::ReanimationAura, ReanimationAura_HotData);
    registry.register_hot_data(RuleId::LustboundBoostAura, LustboundBoostAura_HotData);
    registry.register_hot_data(RuleId::RapidRushAura, RapidRushAura_HotData);
    registry.register_hot_data(RuleId::GroundedReinforcementAura, GroundedReinforcementAura_HotData);
    registry.register_hot_data(RuleId::HoldTheLineBoostAura, HoldTheLineBoostAura_HotData);
    registry.register_hot_data(RuleId::CourageAura, CourageAura_HotData);
    registry.register_hot_data(RuleId::DevoutBoostAura, DevoutBoostAura_HotData);
    registry.register_hot_data(RuleId::HarassingBoostAura, HarassingBoostAura_HotData);
    registry.register_hot_data(RuleId::SwiftAura, SwiftAura_HotData);
    registry.register_hot_data(RuleId::RapidBlinkBoostAura, RapidBlinkBoostAura_HotData);
    registry.register_hot_data(RuleId::RapidAdvanceAura, RapidAdvanceAura_HotData);
    registry.register_hot_data(RuleId::BoundingAura, BoundingAura_HotData);
    registry.register_hot_data(RuleId::HighbornBoostAura, HighbornBoostAura_HotData);
    registry.register_hot_data(RuleId::SpeedFeatAura, SpeedFeatAura_HotData);
    registry.register_hot_data(RuleId::GuerrillaBoostAura, GuerrillaBoostAura_HotData);

    // === Category R: Remaining Buffs ===
    registry.register_hot_data(RuleId::StealthBuff, StealthBuff_HotData);
    registry.register_hot_data(RuleId::ProtectiveDome, ProtectiveDome_HotData);
    registry.register_hot_data(RuleId::GuardedBuff, GuardedBuff_HotData);
    registry.register_hot_data(RuleId::RegenerationBuff, RegenerationBuff_HotData);
    registry.register_hot_data(RuleId::EntrenchedBuff, EntrenchedBuff_HotData);
    registry.register_hot_data(RuleId::SelfRepairBoostBuff, SelfRepairBoostBuff_HotData);
    registry.register_hot_data(RuleId::CourageBuff, CourageBuff_HotData);
    registry.register_hot_data(RuleId::SteadfastBuff, SteadfastBuff_HotData);
    registry.register_hot_data(RuleId::NoRetreatBuff, NoRetreatBuff_HotData);
    registry.register_hot_data(RuleId::RapidAdvanceBuff, RapidAdvanceBuff_HotData);
    registry.register_hot_data(RuleId::SwiftBuff, SwiftBuff_HotData);
    registry.register_hot_data(RuleId::SpeedBuff, SpeedBuff_HotData);
    registry.register_hot_data(RuleId::IncreasedShootingRangeMark, IncreasedShootingRangeMark_HotData);
    registry.register_hot_data(RuleId::ExtendedBuffRange, ExtendedBuffRange_HotData);
    registry.register_hot_data(RuleId::IncreasedShootingRangeBuff, IncreasedShootingRangeBuff_HotData);
    registry.register_hot_data(RuleId::CastingBuff, CastingBuff_HotData);

    // === Category S: Remaining Debuffs ===
    registry.register_hot_data(RuleId::FatigueDebuff, FatigueDebuff_HotData);
    registry.register_hot_data(RuleId::DefenseDebuff, DefenseDebuff_HotData);
    registry.register_hot_data(RuleId::MoraleDebuff, MoraleDebuff_HotData);
    registry.register_hot_data(RuleId::SpeedDebuff, SpeedDebuff_HotData);
    registry.register_hot_data(RuleId::DangerousTerrainDebuff, DangerousTerrainDebuff_HotData);
    registry.register_hot_data(RuleId::MindControl, MindControl_HotData);
    registry.register_hot_data(RuleId::DifficultTerrainDebuff, DifficultTerrainDebuff_HotData);
    registry.register_hot_data(RuleId::CastingDebuff, CastingDebuff_HotData);

    // === Category T: Defense/Growth Rules ===
    registry.register_hot_data(RuleId::FortifiedGrowth, FortifiedGrowth_HotData);
    registry.register_hot_data(RuleId::GroundedStealth, GroundedStealth_HotData);
    registry.register_hot_data(RuleId::ProtectionFeat, ProtectionFeat_HotData);

    // === Category U: Deployment Rules ===
    registry.register_hot_data(RuleId::Infiltrate, Infiltrate_HotData);
    registry.register_hot_data(RuleId::Spawn, Spawn_HotData);
    registry.register_hot_data(RuleId::ReDeployment, ReDeployment_HotData);
    registry.register_hot_data(RuleId::RapidAmbush, RapidAmbush_HotData);
    registry.register_hot_data(RuleId::AmbushBeacon, AmbushBeacon_HotData);
    registry.register_hot_data(RuleId::AmbushReDeployment, AmbushReDeployment_HotData);
    registry.register_hot_data(RuleId::RepelAmbushers, RepelAmbushers_HotData);
    registry.register_hot_data(RuleId::Reinforcement, Reinforcement_HotData);
    registry.register_hot_data(RuleId::Fanatic, Fanatic_HotData);
    registry.register_hot_data(RuleId::Split, Split_HotData);

    // === Category V: Movement Rules ===
    registry.register_hot_data(RuleId::Teleport, Teleport_HotData);
    registry.register_hot_data(RuleId::Wolfborn, Wolfborn_HotData);
    // Note: HitAndRunShooter already registered above
    registry.register_hot_data(RuleId::RapidBlink, RapidBlink_HotData);
    registry.register_hot_data(RuleId::Bounding, Bounding_HotData);

    // === Category W: Faction/Morale/Misc ===
    registry.register_hot_data(RuleId::DevoutBoost, DevoutBoost_HotData);
    registry.register_hot_data(RuleId::Mend, Mend_HotData);
    registry.register_hot_data(RuleId::HiveBond, HiveBond_HotData);
    registry.register_hot_data(RuleId::RePositionArtillery, RePositionArtillery_HotData);

    // === Category X: Spell & Targeting Rules ===
    registry.register_hot_data(RuleId::CasterGroup, CasterGroup_HotData);
    registry.register_hot_data(RuleId::SpellConduit, SpellConduit_HotData);
    registry.register_hot_data(RuleId::SpellAccumulator, SpellAccumulator_HotData);
    registry.register_hot_data(RuleId::PrecisionTarget, PrecisionTarget_HotData);
    registry.register_hot_data(RuleId::PiercingTarget, PiercingTarget_HotData);
    registry.register_hot_data(RuleId::PrecisionSpotter, PrecisionSpotter_HotData);
    registry.register_hot_data(RuleId::PiercingSpotter, PiercingSpotter_HotData);
    registry.register_hot_data(RuleId::PrecisionTag, PrecisionTag_HotData);
    registry.register_hot_data(RuleId::PiercingTag, PiercingTag_HotData);

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

    // Category A: Simple Modifier cold data
    registry.register_cold_data(RuleId::Evasive, Evasive_ColdData);
    registry.register_cold_data(RuleId::Steadfast, Steadfast_ColdData);
    registry.register_cold_data(RuleId::Swift, Swift_ColdData);
    registry.register_cold_data(RuleId::Ferocious, Ferocious_ColdData);
    registry.register_cold_data(RuleId::Lacerate, Lacerate_ColdData);
    registry.register_cold_data(RuleId::Mischievous, Mischievous_ColdData);
    registry.register_cold_data(RuleId::Scrapper, Scrapper_ColdData);

    // Category B: Weapon Conditional cold data
    registry.register_cold_data(RuleId::Bash, Bash_ColdData);
    registry.register_cold_data(RuleId::Thrash, Thrash_ColdData);
    registry.register_cold_data(RuleId::Crack, Crack_ColdData);
    registry.register_cold_data(RuleId::Destructive, Destructive_ColdData);
    registry.register_cold_data(RuleId::Fracture, Fracture_ColdData);
    registry.register_cold_data(RuleId::Break, Break_ColdData);
    registry.register_cold_data(RuleId::Slash, Slash_ColdData);
    registry.register_cold_data(RuleId::Butcher, Butcher_ColdData);
    registry.register_cold_data(RuleId::Slam, Slam_ColdData);
    registry.register_cold_data(RuleId::Quake, Quake_ColdData);
    registry.register_cold_data(RuleId::Tear, Tear_ColdData);
    registry.register_cold_data(RuleId::Scratch, Scratch_ColdData);
    registry.register_cold_data(RuleId::Puncture, Puncture_ColdData);
    registry.register_cold_data(RuleId::Shatter, Shatter_ColdData);
    registry.register_cold_data(RuleId::Demolish, Demolish_ColdData);
    registry.register_cold_data(RuleId::Impale, Impale_ColdData);
    registry.register_cold_data(RuleId::Skewer, Skewer_ColdData);
    registry.register_cold_data(RuleId::Reap, Reap_ColdData);
    registry.register_cold_data(RuleId::Disintegrate, Disintegrate_ColdData);
    registry.register_cold_data(RuleId::Decimate, Decimate_ColdData);
    registry.register_cold_data(RuleId::Fragment, Fragment_ColdData);
    registry.register_cold_data(RuleId::Wreck, Wreck_ColdData);

    // Category C: Movement Bonus cold data
    registry.register_cold_data(RuleId::Lustbound, Lustbound_ColdData);
    registry.register_cold_data(RuleId::Highborn, Highborn_ColdData);
    registry.register_cold_data(RuleId::Scurry, Scurry_ColdData);
    registry.register_cold_data(RuleId::Darkborn, Darkborn_ColdData);
    registry.register_cold_data(RuleId::HitAndRunShooter, HitAndRunShooter_ColdData);

    // Category D: Defense Modifier cold data
    registry.register_cold_data(RuleId::Fortified, Fortified_ColdData);
    registry.register_cold_data(RuleId::Guardian, Guardian_ColdData);
    registry.register_cold_data(RuleId::GuardianBoost, GuardianBoost_ColdData);
    registry.register_cold_data(RuleId::Sturdy, Sturdy_ColdData);
    registry.register_cold_data(RuleId::Knightborn, Knightborn_ColdData);
    registry.register_cold_data(RuleId::Plaguebound, Plaguebound_ColdData);
    registry.register_cold_data(RuleId::Changebound, Changebound_ColdData);

    // Category E: Extra Attack Generation cold data
    registry.register_cold_data(RuleId::Bloodborn, Bloodborn_ColdData);
    registry.register_cold_data(RuleId::TargetingVisor, TargetingVisor_ColdData);
    registry.register_cold_data(RuleId::Havocbound, Havocbound_ColdData);
    registry.register_cold_data(RuleId::Warbound, Warbound_ColdData);
    registry.register_cold_data(RuleId::BrutalFighter, BrutalFighter_ColdData);

    // Category F: Combat Choice & Retaliation cold data
    registry.register_cold_data(RuleId::Unpredictable, Unpredictable_ColdData);
    registry.register_cold_data(RuleId::UnpredictableFighter, UnpredictableFighter_ColdData);
    registry.register_cold_data(RuleId::UnpredictableShooter, UnpredictableShooter_ColdData);
    registry.register_cold_data(RuleId::Retaliate, Retaliate_ColdData);
    registry.register_cold_data(RuleId::Deathstrike, Deathstrike_ColdData);

    // Category G: Enhanced Combat Modifiers cold data
    registry.register_cold_data(RuleId::FerociousBoost, FerociousBoost_ColdData);
    registry.register_cold_data(RuleId::ChangeboundBoost, ChangeboundBoost_ColdData);
    registry.register_cold_data(RuleId::WarboundBoost, WarboundBoost_ColdData);
    registry.register_cold_data(RuleId::PlaegueboundBoost, PlaegueboundBoost_ColdData);
    registry.register_cold_data(RuleId::LustboundBoost, LustboundBoost_ColdData);
    registry.register_cold_data(RuleId::MeleeSlayer, MeleeSlayer_ColdData);
    registry.register_cold_data(RuleId::HeavyImpact, HeavyImpact_ColdData);
    registry.register_cold_data(RuleId::Watchborn, Watchborn_ColdData);

    // Category H: Dice-Based Special Attacks cold data
    registry.register_cold_data(RuleId::Crush, Crush_ColdData);
    registry.register_cold_data(RuleId::Ravage, Ravage_ColdData);
    registry.register_cold_data(RuleId::Hazardous, Hazardous_ColdData);
    registry.register_cold_data(RuleId::QuakeWhenShooting, QuakeWhenShooting_ColdData);
    registry.register_cold_data(RuleId::Harassing, Harassing_ColdData);
    registry.register_cold_data(RuleId::Guerrilla, Guerrilla_ColdData);

    // Category I-O: Batch cold data registration
    registry.register_cold_data(RuleId::VersatileReachAura, VersatileReachAura_ColdData);
    registry.register_cold_data(RuleId::UnpredictableFighterAura, UnpredictableFighterAura_ColdData);
    registry.register_cold_data(RuleId::PointBlankPiercingAura, PointBlankPiercingAura_ColdData);
    registry.register_cold_data(RuleId::RangedSlayerAura, RangedSlayerAura_ColdData);
    registry.register_cold_data(RuleId::TargetingVisorBoostAura, TargetingVisorBoostAura_ColdData);
    registry.register_cold_data(RuleId::PiercingHunterAura, PiercingHunterAura_ColdData);
    registry.register_cold_data(RuleId::ClanWarriorBoostAura, ClanWarriorBoostAura_ColdData);
    registry.register_cold_data(RuleId::PrecisionFighterAura, PrecisionFighterAura_ColdData);
    registry.register_cold_data(RuleId::MischievousBoostAura, MischievousBoostAura_ColdData);
    registry.register_cold_data(RuleId::QuickShotAura, QuickShotAura_ColdData);
    registry.register_cold_data(RuleId::HavocboundBoostAura, HavocboundBoostAura_ColdData);
    registry.register_cold_data(RuleId::WarboundBoostAura, WarboundBoostAura_ColdData);
    registry.register_cold_data(RuleId::InfectedBoostAura, InfectedBoostAura_ColdData);
    registry.register_cold_data(RuleId::UnpredictableShooterAura, UnpredictableShooterAura_ColdData);
    registry.register_cold_data(RuleId::ScrapperBoostAura, ScrapperBoostAura_ColdData);
    registry.register_cold_data(RuleId::FerociousBoostAura, FerociousBoostAura_ColdData);
    registry.register_cold_data(RuleId::PiercingFighterAura, PiercingFighterAura_ColdData);
    registry.register_cold_data(RuleId::PrecisionChargeAura, PrecisionChargeAura_ColdData);
    registry.register_cold_data(RuleId::GroundedPrecisionAura, GroundedPrecisionAura_ColdData);
    registry.register_cold_data(RuleId::MeleeSlayerAura, MeleeSlayerAura_ColdData);
    registry.register_cold_data(RuleId::PiercingShooterAura, PiercingShooterAura_ColdData);
    registry.register_cold_data(RuleId::RighteousFury, RighteousFury_ColdData);
    registry.register_cold_data(RuleId::PrecisionFighterBuff, PrecisionFighterBuff_ColdData);
    registry.register_cold_data(RuleId::PrecisionShooterBuff, PrecisionShooterBuff_ColdData);
    registry.register_cold_data(RuleId::BaneInMeleeBuff, BaneInMeleeBuff_ColdData);
    registry.register_cold_data(RuleId::PrimalBoostBuff, PrimalBoostBuff_ColdData);
    registry.register_cold_data(RuleId::PrecisionAttacksBuff, PrecisionAttacksBuff_ColdData);
    registry.register_cold_data(RuleId::UnpredictableFighterMark, UnpredictableFighterMark_ColdData);
    registry.register_cold_data(RuleId::UnstoppableShootingMark, UnstoppableShootingMark_ColdData);
    registry.register_cold_data(RuleId::ShredMark, ShredMark_ColdData);
    registry.register_cold_data(RuleId::PiercingShootingMark, PiercingShootingMark_ColdData);
    registry.register_cold_data(RuleId::PrecisionFightingMark, PrecisionFightingMark_ColdData);
    registry.register_cold_data(RuleId::SlayerMark, SlayerMark_ColdData);
    registry.register_cold_data(RuleId::RelentlessMark, RelentlessMark_ColdData);
    registry.register_cold_data(RuleId::RendingInMeleeMark, RendingInMeleeMark_ColdData);
    registry.register_cold_data(RuleId::FuriousMark, FuriousMark_ColdData);
    registry.register_cold_data(RuleId::IndirectMark, IndirectMark_ColdData);
    registry.register_cold_data(RuleId::BaneMark, BaneMark_ColdData);
    registry.register_cold_data(RuleId::PrecisionShootingMark, PrecisionShootingMark_ColdData);
    registry.register_cold_data(RuleId::QuickShotMark, QuickShotMark_ColdData);
    registry.register_cold_data(RuleId::UnwieldyDebuff, UnwieldyDebuff_ColdData);
    registry.register_cold_data(RuleId::PrecisionDebuff, PrecisionDebuff_ColdData);
    registry.register_cold_data(RuleId::PiercingShootingDebuff, PiercingShootingDebuff_ColdData);
    registry.register_cold_data(RuleId::PiercingGrowth, PiercingGrowth_ColdData);
    registry.register_cold_data(RuleId::RuinousFrenzy, RuinousFrenzy_ColdData);
    registry.register_cold_data(RuleId::DevastatingFrenzy, DevastatingFrenzy_ColdData);
    registry.register_cold_data(RuleId::PrecisionGrowth, PrecisionGrowth_ColdData);
    registry.register_cold_data(RuleId::DestructiveFrenzy, DestructiveFrenzy_ColdData);
    registry.register_cold_data(RuleId::StormOfChange, StormOfChange_ColdData);
    registry.register_cold_data(RuleId::StormOfLust, StormOfLust_ColdData);
    registry.register_cold_data(RuleId::StormOfPlague, StormOfPlague_ColdData);
    registry.register_cold_data(RuleId::StormOfWar, StormOfWar_ColdData);
    registry.register_cold_data(RuleId::BreathAttack, BreathAttack_ColdData);
    registry.register_cold_data(RuleId::IncreasedShootingRange, IncreasedShootingRange_ColdData);
    registry.register_cold_data(RuleId::RegenerativeStrength, RegenerativeStrength_ColdData);
    registry.register_cold_data(RuleId::Strafing, Strafing_ColdData);
    registry.register_cold_data(RuleId::SurpriseAttack, SurpriseAttack_ColdData);
    registry.register_cold_data(RuleId::TakedownStrike, TakedownStrike_ColdData);
    registry.register_cold_data(RuleId::Instinctive, Instinctive_ColdData);
    registry.register_cold_data(RuleId::CrossingAttack, CrossingAttack_ColdData);
    registry.register_cold_data(RuleId::QuickReadjustment, QuickReadjustment_ColdData);
    registry.register_cold_data(RuleId::CrossingStrike, CrossingStrike_ColdData);
    registry.register_cold_data(RuleId::SurprisePiercingShot, SurprisePiercingShot_ColdData);
    registry.register_cold_data(RuleId::MindWound, MindWound_ColdData);
    registry.register_cold_data(RuleId::MobileArtillery, MobileArtillery_ColdData);
    registry.register_cold_data(RuleId::Vengeance, Vengeance_ColdData);

    // === Category P: Activation Rules ===
    registry.register_cold_data(RuleId::MartialProwess, MartialProwess_ColdData);
    registry.register_cold_data(RuleId::Coordinate, Coordinate_ColdData);
    registry.register_cold_data(RuleId::InquisitorialAgent, InquisitorialAgent_ColdData);
    registry.register_cold_data(RuleId::DelayedAction, DelayedAction_ColdData);

    // === Category Q: Remaining Boost Auras ===
    registry.register_cold_data(RuleId::SturdyBoostAura, SturdyBoostAura_ColdData);
    registry.register_cold_data(RuleId::VersatileDefenseAura, VersatileDefenseAura_ColdData);
    registry.register_cold_data(RuleId::ChangeboundBoostAura, ChangeboundBoostAura_ColdData);
    registry.register_cold_data(RuleId::PlageboundBoostAura, PlageboundBoostAura_ColdData);
    registry.register_cold_data(RuleId::DefensiveGrowthAura, DefensiveGrowthAura_ColdData);
    registry.register_cold_data(RuleId::MachineFogBoostAura, MachineFogBoostAura_ColdData);
    registry.register_cold_data(RuleId::ReanimationAura, ReanimationAura_ColdData);
    registry.register_cold_data(RuleId::LustboundBoostAura, LustboundBoostAura_ColdData);
    registry.register_cold_data(RuleId::RapidRushAura, RapidRushAura_ColdData);
    registry.register_cold_data(RuleId::GroundedReinforcementAura, GroundedReinforcementAura_ColdData);
    registry.register_cold_data(RuleId::HoldTheLineBoostAura, HoldTheLineBoostAura_ColdData);
    registry.register_cold_data(RuleId::CourageAura, CourageAura_ColdData);
    registry.register_cold_data(RuleId::DevoutBoostAura, DevoutBoostAura_ColdData);
    registry.register_cold_data(RuleId::HarassingBoostAura, HarassingBoostAura_ColdData);
    registry.register_cold_data(RuleId::SwiftAura, SwiftAura_ColdData);
    registry.register_cold_data(RuleId::RapidBlinkBoostAura, RapidBlinkBoostAura_ColdData);
    registry.register_cold_data(RuleId::RapidAdvanceAura, RapidAdvanceAura_ColdData);
    registry.register_cold_data(RuleId::BoundingAura, BoundingAura_ColdData);
    registry.register_cold_data(RuleId::HighbornBoostAura, HighbornBoostAura_ColdData);
    registry.register_cold_data(RuleId::SpeedFeatAura, SpeedFeatAura_ColdData);
    registry.register_cold_data(RuleId::GuerrillaBoostAura, GuerrillaBoostAura_ColdData);

    // === Category R: Remaining Buffs ===
    registry.register_cold_data(RuleId::StealthBuff, StealthBuff_ColdData);
    registry.register_cold_data(RuleId::ProtectiveDome, ProtectiveDome_ColdData);
    registry.register_cold_data(RuleId::GuardedBuff, GuardedBuff_ColdData);
    registry.register_cold_data(RuleId::RegenerationBuff, RegenerationBuff_ColdData);
    registry.register_cold_data(RuleId::EntrenchedBuff, EntrenchedBuff_ColdData);
    registry.register_cold_data(RuleId::SelfRepairBoostBuff, SelfRepairBoostBuff_ColdData);
    registry.register_cold_data(RuleId::CourageBuff, CourageBuff_ColdData);
    registry.register_cold_data(RuleId::SteadfastBuff, SteadfastBuff_ColdData);
    registry.register_cold_data(RuleId::NoRetreatBuff, NoRetreatBuff_ColdData);
    registry.register_cold_data(RuleId::RapidAdvanceBuff, RapidAdvanceBuff_ColdData);
    registry.register_cold_data(RuleId::SwiftBuff, SwiftBuff_ColdData);
    registry.register_cold_data(RuleId::SpeedBuff, SpeedBuff_ColdData);
    registry.register_cold_data(RuleId::IncreasedShootingRangeMark, IncreasedShootingRangeMark_ColdData);
    registry.register_cold_data(RuleId::ExtendedBuffRange, ExtendedBuffRange_ColdData);
    registry.register_cold_data(RuleId::IncreasedShootingRangeBuff, IncreasedShootingRangeBuff_ColdData);
    registry.register_cold_data(RuleId::CastingBuff, CastingBuff_ColdData);

    // === Category S: Remaining Debuffs ===
    registry.register_cold_data(RuleId::FatigueDebuff, FatigueDebuff_ColdData);
    registry.register_cold_data(RuleId::DefenseDebuff, DefenseDebuff_ColdData);
    registry.register_cold_data(RuleId::MoraleDebuff, MoraleDebuff_ColdData);
    registry.register_cold_data(RuleId::SpeedDebuff, SpeedDebuff_ColdData);
    registry.register_cold_data(RuleId::DangerousTerrainDebuff, DangerousTerrainDebuff_ColdData);
    registry.register_cold_data(RuleId::MindControl, MindControl_ColdData);
    registry.register_cold_data(RuleId::DifficultTerrainDebuff, DifficultTerrainDebuff_ColdData);
    registry.register_cold_data(RuleId::CastingDebuff, CastingDebuff_ColdData);

    // === Category T: Defense/Growth Rules ===
    registry.register_cold_data(RuleId::FortifiedGrowth, FortifiedGrowth_ColdData);
    registry.register_cold_data(RuleId::GroundedStealth, GroundedStealth_ColdData);
    registry.register_cold_data(RuleId::ProtectionFeat, ProtectionFeat_ColdData);

    // === Category U: Deployment Rules ===
    registry.register_cold_data(RuleId::Infiltrate, Infiltrate_ColdData);
    registry.register_cold_data(RuleId::Spawn, Spawn_ColdData);
    registry.register_cold_data(RuleId::ReDeployment, ReDeployment_ColdData);
    registry.register_cold_data(RuleId::RapidAmbush, RapidAmbush_ColdData);
    registry.register_cold_data(RuleId::AmbushBeacon, AmbushBeacon_ColdData);
    registry.register_cold_data(RuleId::AmbushReDeployment, AmbushReDeployment_ColdData);
    registry.register_cold_data(RuleId::RepelAmbushers, RepelAmbushers_ColdData);
    registry.register_cold_data(RuleId::Reinforcement, Reinforcement_ColdData);
    registry.register_cold_data(RuleId::Fanatic, Fanatic_ColdData);
    registry.register_cold_data(RuleId::Split, Split_ColdData);

    // === Category V: Movement Rules ===
    registry.register_cold_data(RuleId::Teleport, Teleport_ColdData);
    registry.register_cold_data(RuleId::Wolfborn, Wolfborn_ColdData);
    // Note: HitAndRunShooter already registered above
    registry.register_cold_data(RuleId::RapidBlink, RapidBlink_ColdData);
    registry.register_cold_data(RuleId::Bounding, Bounding_ColdData);

    // === Category W: Faction/Morale/Misc ===
    registry.register_cold_data(RuleId::DevoutBoost, DevoutBoost_ColdData);
    registry.register_cold_data(RuleId::Mend, Mend_ColdData);
    registry.register_cold_data(RuleId::HiveBond, HiveBond_ColdData);
    registry.register_cold_data(RuleId::RePositionArtillery, RePositionArtillery_ColdData);

    // === Category X: Spell & Targeting Rules ===
    registry.register_cold_data(RuleId::CasterGroup, CasterGroup_ColdData);
    registry.register_cold_data(RuleId::SpellConduit, SpellConduit_ColdData);
    registry.register_cold_data(RuleId::SpellAccumulator, SpellAccumulator_ColdData);
    registry.register_cold_data(RuleId::PrecisionTarget, PrecisionTarget_ColdData);
    registry.register_cold_data(RuleId::PiercingTarget, PiercingTarget_ColdData);
    registry.register_cold_data(RuleId::PrecisionSpotter, PrecisionSpotter_ColdData);
    registry.register_cold_data(RuleId::PiercingSpotter, PiercingSpotter_ColdData);
    registry.register_cold_data(RuleId::PrecisionTag, PrecisionTag_ColdData);
    registry.register_cold_data(RuleId::PiercingTag, PiercingTag_ColdData);

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

    // Category A: Simple Modifier effects
    registry.register_effects(RuleId::Evasive, Evasive_Effects);
    registry.register_effects(RuleId::Steadfast, Steadfast_Effects);
    registry.register_effects(RuleId::Swift, Swift_Effects);
    registry.register_effects(RuleId::Ferocious, Ferocious_Effects);
    registry.register_effects(RuleId::Lacerate, Lacerate_Effects);
    registry.register_effects(RuleId::Mischievous, Mischievous_Effects);
    registry.register_effects(RuleId::Scrapper, Scrapper_Effects);

    // Category B: Weapon Conditional effects
    registry.register_effects(RuleId::Bash, Bash_Effects);
    registry.register_effects(RuleId::Thrash, Thrash_Effects);
    registry.register_effects(RuleId::Crack, Crack_Effects);
    registry.register_effects(RuleId::Destructive, Destructive_Effects);
    registry.register_effects(RuleId::Fracture, Fracture_Effects);
    registry.register_effects(RuleId::Break, Break_Effects);
    registry.register_effects(RuleId::Slash, Slash_Effects);
    registry.register_effects(RuleId::Butcher, Butcher_Effects);
    registry.register_effects(RuleId::Slam, Slam_Effects);
    registry.register_effects(RuleId::Quake, Quake_Effects);
    registry.register_effects(RuleId::Tear, Tear_Effects);
    registry.register_effects(RuleId::Scratch, Scratch_Effects);
    registry.register_effects(RuleId::Puncture, Puncture_Effects);
    registry.register_effects(RuleId::Shatter, Shatter_Effects);
    registry.register_effects(RuleId::Demolish, Demolish_Effects);
    registry.register_effects(RuleId::Impale, Impale_Effects);
    registry.register_effects(RuleId::Skewer, Skewer_Effects);
    registry.register_effects(RuleId::Reap, Reap_Effects);
    registry.register_effects(RuleId::Disintegrate, Disintegrate_Effects);
    registry.register_effects(RuleId::Decimate, Decimate_Effects);
    registry.register_effects(RuleId::Fragment, Fragment_Effects);
    registry.register_effects(RuleId::Wreck, Wreck_Effects);

    // Category C: Movement Bonus effects
    registry.register_effects(RuleId::Lustbound, Lustbound_Effects);
    registry.register_effects(RuleId::Highborn, Highborn_Effects);
    registry.register_effects(RuleId::Scurry, Scurry_Effects);
    registry.register_effects(RuleId::Darkborn, Darkborn_Effects);
    registry.register_effects(RuleId::HitAndRunShooter, HitAndRunShooter_Effects);

    // Category D: Defense Modifier effects
    registry.register_effects(RuleId::Fortified, Fortified_Effects);
    registry.register_effects(RuleId::Guardian, Guardian_Effects);
    registry.register_effects(RuleId::GuardianBoost, GuardianBoost_Effects);
    registry.register_effects(RuleId::Sturdy, Sturdy_Effects);
    registry.register_effects(RuleId::Knightborn, Knightborn_Effects);
    registry.register_effects(RuleId::Plaguebound, Plaguebound_Effects);
    registry.register_effects(RuleId::Changebound, Changebound_Effects);

    // Category E: Extra Attack Generation effects
    registry.register_effects(RuleId::Bloodborn, Bloodborn_Effects);
    registry.register_effects(RuleId::TargetingVisor, TargetingVisor_Effects);
    registry.register_effects(RuleId::Havocbound, Havocbound_Effects);
    registry.register_effects(RuleId::Warbound, Warbound_Effects);
    registry.register_effects(RuleId::BrutalFighter, BrutalFighter_Effects);

    // Category F: Combat Choice & Retaliation effects
    registry.register_effects(RuleId::Unpredictable, Unpredictable_Effects);
    registry.register_effects(RuleId::UnpredictableFighter, UnpredictableFighter_Effects);
    registry.register_effects(RuleId::UnpredictableShooter, UnpredictableShooter_Effects);
    registry.register_effects(RuleId::Retaliate, Retaliate_Effects);
    registry.register_effects(RuleId::Deathstrike, Deathstrike_Effects);

    // Category G: Enhanced Combat Modifiers effects
    registry.register_effects(RuleId::FerociousBoost, FerociousBoost_Effects);
    registry.register_effects(RuleId::ChangeboundBoost, ChangeboundBoost_Effects);
    registry.register_effects(RuleId::WarboundBoost, WarboundBoost_Effects);
    registry.register_effects(RuleId::PlaegueboundBoost, PlaegueboundBoost_Effects);
    registry.register_effects(RuleId::MeleeSlayer, MeleeSlayer_Effects);
    registry.register_effects(RuleId::HeavyImpact, HeavyImpact_Effects);
    registry.register_effects(RuleId::Watchborn, Watchborn_Effects);

    // Category H: Dice-Based Special Attacks effects
    registry.register_effects(RuleId::Crush, Crush_Effects);
    registry.register_effects(RuleId::Ravage, Ravage_Effects);
    registry.register_effects(RuleId::Hazardous, Hazardous_Effects);
    registry.register_effects(RuleId::QuakeWhenShooting, QuakeWhenShooting_Effects);
    registry.register_effects(RuleId::Harassing, Harassing_Effects);
    registry.register_effects(RuleId::Guerrilla, Guerrilla_Effects);

    // === Category I: Aura Rules ===
    registry.register_effects(RuleId::VersatileReachAura, VersatileReachAura_Effects);
    registry.register_effects(RuleId::UnpredictableFighterAura, UnpredictableFighterAura_Effects);
    registry.register_effects(RuleId::PointBlankPiercingAura, PointBlankPiercingAura_Effects);
    registry.register_effects(RuleId::RangedSlayerAura, RangedSlayerAura_Effects);
    registry.register_effects(RuleId::TargetingVisorBoostAura, TargetingVisorBoostAura_Effects);
    registry.register_effects(RuleId::PiercingHunterAura, PiercingHunterAura_Effects);
    registry.register_effects(RuleId::ClanWarriorBoostAura, ClanWarriorBoostAura_Effects);
    registry.register_effects(RuleId::PrecisionFighterAura, PrecisionFighterAura_Effects);
    registry.register_effects(RuleId::MischievousBoostAura, MischievousBoostAura_Effects);
    registry.register_effects(RuleId::QuickShotAura, QuickShotAura_Effects);
    registry.register_effects(RuleId::HavocboundBoostAura, HavocboundBoostAura_Effects);
    registry.register_effects(RuleId::WarboundBoostAura, WarboundBoostAura_Effects);
    registry.register_effects(RuleId::InfectedBoostAura, InfectedBoostAura_Effects);
    registry.register_effects(RuleId::UnpredictableShooterAura, UnpredictableShooterAura_Effects);
    registry.register_effects(RuleId::ScrapperBoostAura, ScrapperBoostAura_Effects);
    registry.register_effects(RuleId::FerociousBoostAura, FerociousBoostAura_Effects);
    registry.register_effects(RuleId::PiercingFighterAura, PiercingFighterAura_Effects);
    registry.register_effects(RuleId::PrecisionChargeAura, PrecisionChargeAura_Effects);
    registry.register_effects(RuleId::GroundedPrecisionAura, GroundedPrecisionAura_Effects);
    registry.register_effects(RuleId::MeleeSlayerAura, MeleeSlayerAura_Effects);
    registry.register_effects(RuleId::PiercingShooterAura, PiercingShooterAura_Effects);

    // === Category J: Buff Rules ===
    registry.register_effects(RuleId::RighteousFury, RighteousFury_Effects);
    registry.register_effects(RuleId::PrecisionFighterBuff, PrecisionFighterBuff_Effects);
    registry.register_effects(RuleId::PrecisionShooterBuff, PrecisionShooterBuff_Effects);
    registry.register_effects(RuleId::BaneInMeleeBuff, BaneInMeleeBuff_Effects);
    registry.register_effects(RuleId::PrimalBoostBuff, PrimalBoostBuff_Effects);
    registry.register_effects(RuleId::PrecisionAttacksBuff, PrecisionAttacksBuff_Effects);

    // === Category K: Mark Rules ===
    registry.register_effects(RuleId::UnpredictableFighterMark, UnpredictableFighterMark_Effects);
    registry.register_effects(RuleId::UnstoppableShootingMark, UnstoppableShootingMark_Effects);
    registry.register_effects(RuleId::ShredMark, ShredMark_Effects);
    registry.register_effects(RuleId::PiercingShootingMark, PiercingShootingMark_Effects);
    registry.register_effects(RuleId::PrecisionFightingMark, PrecisionFightingMark_Effects);
    registry.register_effects(RuleId::SlayerMark, SlayerMark_Effects);
    registry.register_effects(RuleId::RelentlessMark, RelentlessMark_Effects);
    registry.register_effects(RuleId::RendingInMeleeMark, RendingInMeleeMark_Effects);
    registry.register_effects(RuleId::FuriousMark, FuriousMark_Effects);
    registry.register_effects(RuleId::IndirectMark, IndirectMark_Effects);
    registry.register_effects(RuleId::BaneMark, BaneMark_Effects);
    registry.register_effects(RuleId::PrecisionShootingMark, PrecisionShootingMark_Effects);
    registry.register_effects(RuleId::QuickShotMark, QuickShotMark_Effects);

    // === Category L: Debuff Rules ===
    registry.register_effects(RuleId::UnwieldyDebuff, UnwieldyDebuff_Effects);
    registry.register_effects(RuleId::PrecisionDebuff, PrecisionDebuff_Effects);
    registry.register_effects(RuleId::PiercingShootingDebuff, PiercingShootingDebuff_Effects);

    // === Category M: Scaling/Frenzy/Growth Rules ===
    registry.register_effects(RuleId::PiercingGrowth, PiercingGrowth_Effects);
    registry.register_effects(RuleId::RuinousFrenzy, RuinousFrenzy_Effects);
    registry.register_effects(RuleId::DevastatingFrenzy, DevastatingFrenzy_Effects);
    registry.register_effects(RuleId::PrecisionGrowth, PrecisionGrowth_Effects);
    registry.register_effects(RuleId::DestructiveFrenzy, DestructiveFrenzy_Effects);

    // === Category N: Storm AoE Rules ===
    registry.register_effects(RuleId::StormOfChange, StormOfChange_Effects);
    registry.register_effects(RuleId::StormOfLust, StormOfLust_Effects);
    registry.register_effects(RuleId::StormOfPlague, StormOfPlague_Effects);
    registry.register_effects(RuleId::StormOfWar, StormOfWar_Effects);

    // === Category O: Special Combat Rules ===
    registry.register_effects(RuleId::BreathAttack, BreathAttack_Effects);
    registry.register_effects(RuleId::IncreasedShootingRange, IncreasedShootingRange_Effects);
    registry.register_effects(RuleId::RegenerativeStrength, RegenerativeStrength_Effects);
    registry.register_effects(RuleId::Strafing, Strafing_Effects);
    registry.register_effects(RuleId::SurpriseAttack, SurpriseAttack_Effects);
    registry.register_effects(RuleId::TakedownStrike, TakedownStrike_Effects);
    registry.register_effects(RuleId::Instinctive, Instinctive_Effects);
    registry.register_effects(RuleId::CrossingAttack, CrossingAttack_Effects);
    registry.register_effects(RuleId::QuickReadjustment, QuickReadjustment_Effects);
    registry.register_effects(RuleId::CrossingStrike, CrossingStrike_Effects);
    registry.register_effects(RuleId::SurprisePiercingShot, SurprisePiercingShot_Effects);
    registry.register_effects(RuleId::MindWound, MindWound_Effects);
    registry.register_effects(RuleId::MobileArtillery, MobileArtillery_Effects);
    registry.register_effects(RuleId::Vengeance, Vengeance_Effects);

    // === Category P: Activation Rules ===
    registry.register_effects(RuleId::MartialProwess, MartialProwess_Effects);
    registry.register_effects(RuleId::Coordinate, Coordinate_Effects);
    registry.register_effects(RuleId::InquisitorialAgent, InquisitorialAgent_Effects);
    registry.register_effects(RuleId::DelayedAction, DelayedAction_Effects);

    // === Category Q: Remaining Boost Auras ===
    registry.register_effects(RuleId::SturdyBoostAura, SturdyBoostAura_Effects);
    registry.register_effects(RuleId::VersatileDefenseAura, VersatileDefenseAura_Effects);
    registry.register_effects(RuleId::ChangeboundBoostAura, ChangeboundBoostAura_Effects);
    registry.register_effects(RuleId::PlageboundBoostAura, PlageboundBoostAura_Effects);
    registry.register_effects(RuleId::DefensiveGrowthAura, DefensiveGrowthAura_Effects);
    registry.register_effects(RuleId::MachineFogBoostAura, MachineFogBoostAura_Effects);
    registry.register_effects(RuleId::ReanimationAura, ReanimationAura_Effects);
    registry.register_effects(RuleId::LustboundBoostAura, LustboundBoostAura_Effects);
    registry.register_effects(RuleId::RapidRushAura, RapidRushAura_Effects);
    registry.register_effects(RuleId::GroundedReinforcementAura, GroundedReinforcementAura_Effects);
    registry.register_effects(RuleId::HoldTheLineBoostAura, HoldTheLineBoostAura_Effects);
    registry.register_effects(RuleId::CourageAura, CourageAura_Effects);
    registry.register_effects(RuleId::DevoutBoostAura, DevoutBoostAura_Effects);
    registry.register_effects(RuleId::HarassingBoostAura, HarassingBoostAura_Effects);
    registry.register_effects(RuleId::SwiftAura, SwiftAura_Effects);
    registry.register_effects(RuleId::RapidBlinkBoostAura, RapidBlinkBoostAura_Effects);
    registry.register_effects(RuleId::RapidAdvanceAura, RapidAdvanceAura_Effects);
    registry.register_effects(RuleId::BoundingAura, BoundingAura_Effects);
    registry.register_effects(RuleId::HighbornBoostAura, HighbornBoostAura_Effects);
    registry.register_effects(RuleId::SpeedFeatAura, SpeedFeatAura_Effects);
    registry.register_effects(RuleId::GuerrillaBoostAura, GuerrillaBoostAura_Effects);

    // === Category R: Remaining Buffs ===
    registry.register_effects(RuleId::StealthBuff, StealthBuff_Effects);
    registry.register_effects(RuleId::ProtectiveDome, ProtectiveDome_Effects);
    registry.register_effects(RuleId::GuardedBuff, GuardedBuff_Effects);
    registry.register_effects(RuleId::RegenerationBuff, RegenerationBuff_Effects);
    registry.register_effects(RuleId::EntrenchedBuff, EntrenchedBuff_Effects);
    registry.register_effects(RuleId::SelfRepairBoostBuff, SelfRepairBoostBuff_Effects);
    registry.register_effects(RuleId::CourageBuff, CourageBuff_Effects);
    registry.register_effects(RuleId::SteadfastBuff, SteadfastBuff_Effects);
    registry.register_effects(RuleId::NoRetreatBuff, NoRetreatBuff_Effects);
    registry.register_effects(RuleId::RapidAdvanceBuff, RapidAdvanceBuff_Effects);
    registry.register_effects(RuleId::SwiftBuff, SwiftBuff_Effects);
    registry.register_effects(RuleId::SpeedBuff, SpeedBuff_Effects);
    registry.register_effects(RuleId::IncreasedShootingRangeMark, IncreasedShootingRangeMark_Effects);
    registry.register_effects(RuleId::ExtendedBuffRange, ExtendedBuffRange_Effects);
    registry.register_effects(RuleId::IncreasedShootingRangeBuff, IncreasedShootingRangeBuff_Effects);
    registry.register_effects(RuleId::CastingBuff, CastingBuff_Effects);

    // === Category S: Remaining Debuffs ===
    registry.register_effects(RuleId::FatigueDebuff, FatigueDebuff_Effects);
    registry.register_effects(RuleId::DefenseDebuff, DefenseDebuff_Effects);
    registry.register_effects(RuleId::MoraleDebuff, MoraleDebuff_Effects);
    registry.register_effects(RuleId::SpeedDebuff, SpeedDebuff_Effects);
    registry.register_effects(RuleId::DangerousTerrainDebuff, DangerousTerrainDebuff_Effects);
    registry.register_effects(RuleId::MindControl, MindControl_Effects);
    registry.register_effects(RuleId::DifficultTerrainDebuff, DifficultTerrainDebuff_Effects);
    registry.register_effects(RuleId::CastingDebuff, CastingDebuff_Effects);

    // === Category T: Defense/Growth Rules ===
    registry.register_effects(RuleId::FortifiedGrowth, FortifiedGrowth_Effects);
    registry.register_effects(RuleId::GroundedStealth, GroundedStealth_Effects);
    registry.register_effects(RuleId::ProtectionFeat, ProtectionFeat_Effects);

    // === Category U: Deployment Rules ===
    registry.register_effects(RuleId::Infiltrate, Infiltrate_Effects);
    registry.register_effects(RuleId::Spawn, Spawn_Effects);
    registry.register_effects(RuleId::ReDeployment, ReDeployment_Effects);
    registry.register_effects(RuleId::RapidAmbush, RapidAmbush_Effects);
    registry.register_effects(RuleId::AmbushBeacon, AmbushBeacon_Effects);
    registry.register_effects(RuleId::AmbushReDeployment, AmbushReDeployment_Effects);
    registry.register_effects(RuleId::RepelAmbushers, RepelAmbushers_Effects);
    registry.register_effects(RuleId::Reinforcement, Reinforcement_Effects);
    registry.register_effects(RuleId::Fanatic, Fanatic_Effects);
    registry.register_effects(RuleId::Split, Split_Effects);

    // === Category V: Movement Rules ===
    registry.register_effects(RuleId::Teleport, Teleport_Effects);
    registry.register_effects(RuleId::Wolfborn, Wolfborn_Effects);
    registry.register_effects(RuleId::HitAndRunShooter, HitAndRunShooter_Cat_Effects);
    registry.register_effects(RuleId::RapidBlink, RapidBlink_Effects);
    registry.register_effects(RuleId::Bounding, Bounding_Effects);

    // === Category W: Faction/Morale/Misc ===
    registry.register_effects(RuleId::DevoutBoost, DevoutBoost_Effects);
    registry.register_effects(RuleId::Mend, Mend_Effects);
    registry.register_effects(RuleId::HiveBond, HiveBond_Effects);
    registry.register_effects(RuleId::RePositionArtillery, RePositionArtillery_Effects);

    // === Category X: Spell & Targeting Rules ===
    registry.register_effects(RuleId::CasterGroup, CasterGroup_Effects);
    registry.register_effects(RuleId::SpellConduit, SpellConduit_Effects);
    registry.register_effects(RuleId::SpellAccumulator, SpellAccumulator_Effects);
    registry.register_effects(RuleId::PrecisionTarget, PrecisionTarget_Effects);
    registry.register_effects(RuleId::PiercingTarget, PiercingTarget_Effects);
    registry.register_effects(RuleId::PrecisionSpotter, PrecisionSpotter_Effects);
    registry.register_effects(RuleId::PiercingSpotter, PiercingSpotter_Effects);
    registry.register_effects(RuleId::PrecisionTag, PrecisionTag_Effects);
    registry.register_effects(RuleId::PiercingTag, PiercingTag_Effects);

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
    registry.register_alias("warbound", RuleId::Warbound);
    registry.register_alias("Warbound", RuleId::Warbound);
    // Infected has the same effect as Warbound (extra wound on defense 1s)
    registry.register_alias("infected", RuleId::Warbound);
    registry.register_alias("Infected", RuleId::Warbound);

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
