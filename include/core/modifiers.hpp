#pragma once

#include "core/types.hpp"
#include <array>
#include <bitset>

namespace battle {

// ==============================================================================
// Modifier Layer - Handles temporary and conditional rule effects
// ==============================================================================
// This system manages rules that are granted temporarily (auras, buffs, debuffs)
// as well as conditional effects that only apply in certain circumstances.

// ==============================================================================
// Modifier Source - Where the modifier came from
// ==============================================================================

enum class ModifierSource : u8 {
    PERMANENT = 0,      // From unit's base rules (never expires)
    AURA = 1,           // From nearby unit's aura (expires when out of range)
    BUFF = 2,           // From friendly ability (expires based on duration)
    DEBUFF = 3,         // From enemy ability (expires based on duration)
    TERRAIN = 4,        // From terrain feature (expires when leaving terrain)
    FACTION_RULE = 5    // From faction-specific rule
};

// ==============================================================================
// Modifier Duration - How long the modifier lasts
// ==============================================================================

enum class ModifierDuration : u8 {
    PERMANENT = 0,          // Never expires
    UNTIL_END_OF_PHASE = 1, // Expires at end of current phase
    UNTIL_END_OF_ROUND = 2, // Expires at end of current round
    UNTIL_END_OF_GAME = 3,  // Lasts entire game (but can be removed)
    UNTIL_CONDITION = 4,    // Lasts until a condition is met
    ONE_USE = 5             // Single use, then removed
};

// ==============================================================================
// Modifier Condition - When the modifier applies
// ==============================================================================

enum class ModifierCondition : u8 {
    ALWAYS = 0,             // Always applies when active
    MELEE_ONLY = 1,         // Only applies in melee combat
    SHOOTING_ONLY = 2,      // Only applies when shooting
    ON_CHARGE = 3,          // Only applies when charging
    ON_BEING_CHARGED = 4,   // Only applies when being charged
    WHILE_SHAKEN = 5,       // Only applies while unit is shaken
    VS_INFANTRY = 6,        // Only applies vs infantry
    VS_VEHICLES = 7,        // Only applies vs vehicles
    AT_HALF_STRENGTH = 8,   // Only applies at half strength or below
    IN_COVER = 9,           // Only applies while in cover
    WITHIN_RANGE = 10       // Only applies within specified range
};

// ==============================================================================
// Active Modifier - A currently active modifier on a unit
// ==============================================================================

struct ActiveModifier {
    // What rule this modifier grants or affects
    RuleId rule_id = RuleId::None;

    // Modifier value (for rules with values like Blast(X), AP(X))
    u8 value = 0;

    // Source and duration
    ModifierSource source = ModifierSource::PERMANENT;
    ModifierDuration duration = ModifierDuration::PERMANENT;
    ModifierCondition condition = ModifierCondition::ALWAYS;

    // For auras: which unit is the source
    u16 source_unit_id = 0;

    // For conditional modifiers: range or other parameter
    u8 condition_param = 0;

    // Tracking
    u8 rounds_remaining = 0;    // For timed durations
    bool used = false;          // For one-use modifiers

    constexpr ActiveModifier() = default;

    constexpr ActiveModifier(RuleId id, u8 val = 0)
        : rule_id(id), value(val) {}

    constexpr ActiveModifier(RuleId id, ModifierSource src, ModifierDuration dur)
        : rule_id(id), source(src), duration(dur) {}

    bool is_valid() const { return rule_id != RuleId::None; }
    bool is_permanent() const { return duration == ModifierDuration::PERMANENT; }
    bool is_from_aura() const { return source == ModifierSource::AURA; }

    // Check if this modifier should expire
    bool should_expire(bool end_of_phase, bool end_of_round) const {
        if (duration == ModifierDuration::PERMANENT) return false;
        if (duration == ModifierDuration::ONE_USE && used) return true;
        if (duration == ModifierDuration::UNTIL_END_OF_PHASE && end_of_phase) return true;
        if (duration == ModifierDuration::UNTIL_END_OF_ROUND && end_of_round) return true;
        if (duration == ModifierDuration::UNTIL_END_OF_GAME) return false;
        return false;
    }

    // Check if condition is met for this modifier to apply
    bool condition_met(bool is_melee, bool is_charging, bool is_being_charged,
                       bool is_shaken, bool in_cover, u8 distance) const {
        switch (condition) {
            case ModifierCondition::ALWAYS:
                return true;
            case ModifierCondition::MELEE_ONLY:
                return is_melee;
            case ModifierCondition::SHOOTING_ONLY:
                return !is_melee;
            case ModifierCondition::ON_CHARGE:
                return is_charging;
            case ModifierCondition::ON_BEING_CHARGED:
                return is_being_charged;
            case ModifierCondition::WHILE_SHAKEN:
                return is_shaken;
            case ModifierCondition::IN_COVER:
                return in_cover;
            case ModifierCondition::WITHIN_RANGE:
                return distance <= condition_param;
            default:
                return true;
        }
    }
};

static_assert(sizeof(ActiveModifier) <= 16, "ActiveModifier should be compact");

// ==============================================================================
// Stat Modifiers - Numerical modifiers to unit stats
// ==============================================================================

struct StatModifiers {
    i8 hit_modifier = 0;        // +/- to hit rolls
    i8 defense_modifier = 0;    // +/- to defense
    i8 morale_modifier = 0;     // +/- to morale tests
    i8 ap_modifier = 0;         // +/- to AP value
    i8 quality_modifier = 0;    // +/- to quality (lower is better)
    u8 extra_attacks = 0;       // Additional attacks
    u8 move_modifier = 0;       // +/- to movement (in inches)

    // Combine with another set of modifiers
    void combine(const StatModifiers& other) {
        hit_modifier += other.hit_modifier;
        defense_modifier += other.defense_modifier;
        morale_modifier += other.morale_modifier;
        ap_modifier += other.ap_modifier;
        quality_modifier += other.quality_modifier;
        extra_attacks += other.extra_attacks;
        move_modifier += other.move_modifier;
    }

    void reset() {
        hit_modifier = 0;
        defense_modifier = 0;
        morale_modifier = 0;
        ap_modifier = 0;
        quality_modifier = 0;
        extra_attacks = 0;
        move_modifier = 0;
    }

    bool is_empty() const {
        return hit_modifier == 0 && defense_modifier == 0 &&
               morale_modifier == 0 && ap_modifier == 0 &&
               quality_modifier == 0 && extra_attacks == 0 &&
               move_modifier == 0;
    }
};

// ==============================================================================
// Unit Modifiers - All active modifiers on a single unit
// ==============================================================================

class UnitModifiers {
public:
    static constexpr size_t MAX_ACTIVE_MODIFIERS = 32;

    UnitModifiers() = default;

    // Add a new modifier
    bool add_modifier(const ActiveModifier& mod) {
        if (modifier_count_ >= MAX_ACTIVE_MODIFIERS) return false;
        modifiers_[modifier_count_++] = mod;
        invalidate_cache();
        return true;
    }

    // Remove modifiers from a specific source unit (for aura expiration)
    void remove_modifiers_from_source(u16 source_unit_id) {
        size_t write = 0;
        for (size_t read = 0; read < modifier_count_; ++read) {
            if (modifiers_[read].source_unit_id != source_unit_id) {
                modifiers_[write++] = modifiers_[read];
            }
        }
        if (write != modifier_count_) {
            modifier_count_ = static_cast<u8>(write);
            invalidate_cache();
        }
    }

    // Remove modifiers of a specific source type
    void remove_modifiers_by_source(ModifierSource source) {
        size_t write = 0;
        for (size_t read = 0; read < modifier_count_; ++read) {
            if (modifiers_[read].source != source) {
                modifiers_[write++] = modifiers_[read];
            }
        }
        if (write != modifier_count_) {
            modifier_count_ = static_cast<u8>(write);
            invalidate_cache();
        }
    }

    // Expire modifiers at phase/round end
    void expire_modifiers(bool end_of_phase, bool end_of_round) {
        size_t write = 0;
        for (size_t read = 0; read < modifier_count_; ++read) {
            if (!modifiers_[read].should_expire(end_of_phase, end_of_round)) {
                modifiers_[write++] = modifiers_[read];
            }
        }
        if (write != modifier_count_) {
            modifier_count_ = static_cast<u8>(write);
            invalidate_cache();
        }
    }

    // Check if unit has a specific rule (including from modifiers)
    bool has_rule(RuleId id) const {
        for (size_t i = 0; i < modifier_count_; ++i) {
            if (modifiers_[i].rule_id == id) return true;
        }
        return false;
    }

    // Get the value for a rule (highest value if multiple)
    u8 get_rule_value(RuleId id) const {
        u8 max_value = 0;
        for (size_t i = 0; i < modifier_count_; ++i) {
            if (modifiers_[i].rule_id == id && modifiers_[i].value > max_value) {
                max_value = modifiers_[i].value;
            }
        }
        return max_value;
    }

    // Get aggregated stat modifiers for current combat context
    StatModifiers get_stat_modifiers(bool is_melee, bool is_charging,
                                      bool is_being_charged, bool is_shaken,
                                      bool in_cover, u8 distance) const {
        StatModifiers result;

        for (size_t i = 0; i < modifier_count_; ++i) {
            const auto& mod = modifiers_[i];
            if (!mod.condition_met(is_melee, is_charging, is_being_charged,
                                   is_shaken, in_cover, distance)) {
                continue;
            }

            // Apply stat effects based on rule
            // This will be extended as we integrate more faction rules
            switch (mod.rule_id) {
                case RuleId::Precise:
                case RuleId::GoodShot:
                    result.hit_modifier += 1;
                    break;
                case RuleId::BadShot:
                    result.hit_modifier -= 1;
                    break;
                case RuleId::Stealth:
                    // Handled by defender, not here
                    break;
                case RuleId::Shielded:
                    result.defense_modifier += 1;
                    break;
                case RuleId::MoraleBoost:
                    result.morale_modifier += static_cast<i8>(mod.value);
                    break;
                default:
                    // Other rules don't provide stat modifiers
                    break;
            }
        }

        return result;
    }

    // Get all active modifiers (for debugging/logging)
    const std::array<ActiveModifier, MAX_ACTIVE_MODIFIERS>& get_modifiers() const {
        return modifiers_;
    }

    u8 modifier_count() const { return modifier_count_; }

    // Mark a one-use modifier as used
    void mark_used(RuleId id) {
        for (size_t i = 0; i < modifier_count_; ++i) {
            if (modifiers_[i].rule_id == id &&
                modifiers_[i].duration == ModifierDuration::ONE_USE) {
                modifiers_[i].used = true;
            }
        }
    }

    // Clear all modifiers
    void clear() {
        modifier_count_ = 0;
        invalidate_cache();
    }

private:
    std::array<ActiveModifier, MAX_ACTIVE_MODIFIERS> modifiers_{};
    u8 modifier_count_ = 0;

    // Cache invalidation (for future optimization)
    mutable bool cache_valid_ = false;

    void invalidate_cache() { cache_valid_ = false; }
};

// ==============================================================================
// Aura Definition - Describes an aura effect
// ==============================================================================

struct AuraDefinition {
    RuleId source_rule = RuleId::None;  // The rule that provides this aura
    RuleId grants_rule = RuleId::None;  // The rule this aura grants
    u8 grants_value = 0;                // Value for the granted rule
    u8 range = 12;                      // Range in inches
    bool affects_friendly = true;       // Affects friendly units
    bool affects_enemy = false;         // Affects enemy units
    bool affects_self = false;          // Affects the source unit
    ModifierCondition condition = ModifierCondition::ALWAYS;

    constexpr AuraDefinition() = default;

    constexpr AuraDefinition(RuleId source, RuleId grants, u8 val = 0, u8 r = 12)
        : source_rule(source), grants_rule(grants), grants_value(val), range(r) {}
};

// ==============================================================================
// Faction Rule ID Extension
// ==============================================================================
// These are faction-specific rules that will be added to the extended RuleId range

enum class FactionRuleId : u16 {
    // Base offset for faction rules (after PRIMARY_COUNT + existing extended)
    FACTION_BASE = 128,

    // Alien Hives faction rules
    AH_HiveBond = FACTION_BASE,
    AH_BreathAttack,
    AH_PiercingGrowth,
    AH_StealthBuff,
    AH_SurpriseAttack,
    AH_TakedownStrike,
    AH_FuriousAura,
    AH_HiveBondBoostAura,
    AH_ShieldedAura,
    AH_AnimateSpirit,
    AH_OverwhelmingStrike,
    AH_InfuseBloodthirst,
    AH_PsychicBlast,
    AH_TerrorSeeker,
    AH_HiveShriek,

    // Battle Brothers faction rules
    BB_Battleborn,  // Already in base rules, but tracked here for faction mapping
    BB_Mend,
    BB_ReDeployment,
    BB_RePositionArtillery,
    BB_UnstoppableShootingMark,
    BB_BaneInMeleeAura,
    BB_BaneWhenShootingAura,
    BB_CourageAura,
    BB_MeleeShroudingAura,
    BB_RapidRushAura,
    BB_RegenerationAura,
    BB_VersatileReachAura,
    BB_AdvancedSight,
    BB_CerebralTrauma,
    BB_BlessedAmmo,
    BB_LightningFog,
    BB_ProtectiveDome,
    BB_PsychicTerror,

    // More factions will be added here...
    // Each faction gets a block of IDs

    FACTION_COUNT
};

// Helper to convert FactionRuleId to RuleId for storage
inline RuleId to_rule_id(FactionRuleId fid) {
    return static_cast<RuleId>(static_cast<u16>(fid));
}

} // namespace battle
