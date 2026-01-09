#pragma once

#include "core/types.hpp"

namespace battle {

// ==============================================================================
// Rule Traits - Behavioral flags aggregated at combat start
// ==============================================================================
// Traits allow quick checking of behavioral properties without iterating
// through all active rules. They are aggregated once at combat start and
// stored in the combat context for fast access during resolution.

enum class RuleTrait : u32 {
    NONE                    = 0,

    // Defense bypass traits
    BYPASSES_REGENERATION   = 1 << 0,   // Bane, Rending, Rupture, Shred, Unstoppable
    BYPASSES_RESISTANCE     = 1 << 1,   // Reserved for future use
    FORCES_DEFENSE_REROLL   = 1 << 2,   // Poison, Bane - reroll defense 6s

    // Timing traits
    CHARGE_ONLY             = 1 << 3,   // Only active when charging (Furious, Impact, Lance)
    FIRST_TURN_ONLY         = 1 << 4,   // Only active on first turn

    // Combat type restrictions
    MELEE_ONLY              = 1 << 5,   // Only applies in melee (ShieldWall, BaneInMelee)
    RANGED_ONLY             = 1 << 6,   // Only applies when shooting (Stealth at >9")

    // Targeting traits
    REQUIRES_LOS            = 1 << 7,   // Requires line of sight (most attacks)
    IGNORES_COVER           = 1 << 8,   // Indirect
    PICK_TARGET_MODEL       = 1 << 9,   // Sniper, Takedown

    // Effect type traits
    AURA_EFFECT             = 1 << 10,  // Affects nearby units (Fear aura, etc.)
    SELF_EFFECT             = 1 << 11,  // Affects only the bearer
    WEAPON_EFFECT           = 1 << 12,  // Effect comes from weapon

    // Special behavior traits
    GENERATES_EXTRA_HITS    = 1 << 13,  // Relentless, Surge, Furious, PredatorFighter
    GENERATES_EXTRA_WOUNDS  = 1 << 14,  // Rupture, Shred
    MULTIPLIES_HITS         = 1 << 15,  // Blast
    MULTIPLIES_WOUNDS       = 1 << 16,  // Deadly

    // Stat modification traits
    MODIFIES_HIT_ROLL       = 1 << 17,  // Precise, Stealth, GoodShot, BadShot
    MODIFIES_DEFENSE        = 1 << 18,  // Shielded, ShieldWall
    MODIFIES_AP             = 1 << 19,  // AP, Lance, Rending, Thrust
    OVERRIDES_QUALITY       = 1 << 20,  // Reliable

    // Movement traits
    MODIFIES_MOVEMENT       = 1 << 21,  // Fast, Slow
    IGNORES_TERRAIN         = 1 << 22,  // Flying, Strider
    AFFECTS_CHARGE          = 1 << 23,  // Agile, RapidCharge

    // Deployment traits
    FORWARD_DEPLOY          = 1 << 24,  // Scout
    ANYWHERE_DEPLOY         = 1 << 25,  // Ambush

    // Morale traits
    AFFECTS_MORALE          = 1 << 26,  // Fearless, Fear, HoldTheLine, NoRetreat

    // Death trigger traits
    ON_DEATH_EFFECT         = 1 << 27,  // SelfDestruct

    // Value-carrying traits (rule has an associated numeric value)
    HAS_VALUE               = 1 << 28   // Blast(X), Deadly(X), AP(X), Impact(X), etc.
};

// Trait mask for combining multiple traits
using TraitMask = u32;

// Helper operators for combining traits
inline constexpr TraitMask operator|(RuleTrait a, RuleTrait b) {
    return static_cast<TraitMask>(a) | static_cast<TraitMask>(b);
}

inline constexpr TraitMask operator|(TraitMask a, RuleTrait b) {
    return a | static_cast<TraitMask>(b);
}

inline constexpr TraitMask operator&(TraitMask a, RuleTrait b) {
    return a & static_cast<TraitMask>(b);
}

inline constexpr bool has_trait(TraitMask mask, RuleTrait trait) {
    return (mask & static_cast<TraitMask>(trait)) != 0;
}

// ==============================================================================
// Common Trait Combinations
// ==============================================================================

// Rules that bypass regeneration
constexpr TraitMask REGEN_BYPASS_TRAITS =
    static_cast<TraitMask>(RuleTrait::BYPASSES_REGENERATION);

// Rules that modify hit rolls
constexpr TraitMask HIT_MODIFIER_TRAITS =
    static_cast<TraitMask>(RuleTrait::MODIFIES_HIT_ROLL);

// Rules that generate extra hits/wounds
constexpr TraitMask EXTRA_DAMAGE_TRAITS =
    static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_HITS) |
    static_cast<TraitMask>(RuleTrait::GENERATES_EXTRA_WOUNDS) |
    static_cast<TraitMask>(RuleTrait::MULTIPLIES_HITS) |
    static_cast<TraitMask>(RuleTrait::MULTIPLIES_WOUNDS);

// Rules active only during charges
constexpr TraitMask CHARGE_ONLY_TRAITS =
    static_cast<TraitMask>(RuleTrait::CHARGE_ONLY);

// ==============================================================================
// Trait Extraction Helpers
// ==============================================================================

// Extract combat-relevant traits into a compact flag byte for CombatContextCore
// Returns a u8 with bits:
//   bit 0: bypasses regeneration
//   bit 1: bypasses resistance
//   bit 2: forces defense reroll
inline constexpr u8 extract_combat_traits(TraitMask traits) {
    u8 flags = 0;
    if (traits & static_cast<TraitMask>(RuleTrait::BYPASSES_REGENERATION)) flags |= 0x01;
    if (traits & static_cast<TraitMask>(RuleTrait::BYPASSES_RESISTANCE)) flags |= 0x02;
    if (traits & static_cast<TraitMask>(RuleTrait::FORCES_DEFENSE_REROLL)) flags |= 0x04;
    return flags;
}

} // namespace battle
