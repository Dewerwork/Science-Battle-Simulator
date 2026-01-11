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
    // Unlike Unpredictable, this is player choice not dice roll
    // For simulation, use same versatile_roll logic
    if (ext && ext->versatile_roll > 0) {
        if (ext->versatile_roll <= 3) {
            ctx.ap_modifier += 1;
        } else {
            ctx.hit_modifier += 1;
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
    Target::WEAPON,
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
    static_cast<u8>(CombatSubPhase::HIT_MODIFIERS),
    CombatType::BOTH,
    Target::SELF,
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
    .combat(CombatSubPhase::HIT_MODIFIERS, watchborn_effect)
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
