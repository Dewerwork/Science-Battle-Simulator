#pragma once

#include "core/types.hpp"
#include <cstdint>

namespace battle {

// ==============================================================================
// AI Hints - Behavioral guidance for rule-aware decision making
// ==============================================================================
// These hints are stored with each rule definition and aggregated during
// combat planning to guide AI decision trees.
//
// The AI system uses these hints to:
// - Choose optimal distance/engagement range
// - Decide when to charge vs. shoot
// - Evaluate risk/reward tradeoffs
// - Select targets and priorities

// ==============================================================================
// Distance Preference - Optimal engagement range
// ==============================================================================

enum class DistancePreference : u8 {
    NEUTRAL = 0,      // No preference (default)
    CLOSE = 1,        // Rule works best at close range (melee, short-range bonuses)
    FAR = 2,          // Rule works best at long range (Relentless, Stealth)
    VARIABLE = 3      // Effectiveness varies by situation
};

// ==============================================================================
// Engagement Preference - When to engage in combat
// ==============================================================================

enum class EngagementPreference : u8 {
    NEUTRAL = 0,      // No preference
    AGGRESSIVE = 1,   // Prefer charging, closing distance
    DEFENSIVE = 2,    // Prefer holding position, shooting
    OPPORTUNISTIC = 3 // Engage based on advantage
};

// ==============================================================================
// AI Hint Flags - Compact bitfield for common hints
// ==============================================================================

enum class AIHintFlag : u16 {
    NONE                    = 0,

    // Combat type preferences
    PREFERS_MELEE           = 1 << 0,   // Rule is better in melee
    PREFERS_SHOOTING        = 1 << 1,   // Rule is better when shooting
    BENEFITS_FROM_CHARGE    = 1 << 2,   // Rule activates or improves on charge

    // Tactical preferences
    WANTS_FIRST_STRIKE      = 1 << 3,   // Benefit from attacking first
    PREFERS_HOLDING         = 1 << 4,   // Better when holding position
    AVOID_BEING_CHARGED     = 1 << 5,   // Should avoid being charged

    // Targeting preferences
    PICK_WEAK_TARGETS       = 1 << 6,   // Better against low-defense targets
    PICK_TOUGH_TARGETS      = 1 << 7,   // Better against high-defense targets
    TARGET_HEROES           = 1 << 8,   // Effective against hero models

    // Risk/reward
    HIGH_RISK_HIGH_REWARD   = 1 << 9,   // Volatile outcomes (Deadly, Blast)
    CONSISTENT_VALUE        = 1 << 10,  // Reliable, steady damage

    // Movement considerations
    MOBILE_ADVANTAGE        = 1 << 11,  // Benefits from movement flexibility
    STATIC_ADVANTAGE        = 1 << 12,  // Benefits from staying in place

    // Defensive considerations
    DEFENSIVE_BONUS         = 1 << 13,  // Provides defensive benefits
    OFFENSIVE_BONUS         = 1 << 14,  // Provides offensive benefits

    // Special cases
    SYNERGY_DEPENDENT       = 1 << 15   // Value depends on other rules present
};

using AIHintMask = u16;

// Helper operators for combining AI hints
inline constexpr AIHintMask operator|(AIHintFlag a, AIHintFlag b) {
    return static_cast<AIHintMask>(a) | static_cast<AIHintMask>(b);
}

inline constexpr AIHintMask operator|(AIHintMask a, AIHintFlag b) {
    return a | static_cast<AIHintMask>(b);
}

inline constexpr AIHintMask operator&(AIHintMask a, AIHintFlag b) {
    return a & static_cast<AIHintMask>(b);
}

inline constexpr bool has_hint(AIHintMask mask, AIHintFlag hint) {
    return (mask & static_cast<AIHintMask>(hint)) != 0;
}

// ==============================================================================
// AI Hints Structure - Per-rule AI guidance
// ==============================================================================
// This structure is stored as part of cold data since it's only accessed
// during AI planning, not during combat resolution hot path.

struct AIHints {
    // Distance and engagement preferences
    DistancePreference distance_pref = DistancePreference::NEUTRAL;
    EngagementPreference engagement_pref = EngagementPreference::NEUTRAL;

    // Compact hint flags
    AIHintMask hint_flags = 0;

    // Strategic weights (signed, -100 to +100)
    // Positive = offensive advantage, Negative = defensive advantage
    i8 offensive_weight = 0;   // How much this rule contributes to offense
    i8 defensive_weight = 0;   // How much this rule contributes to defense

    // Range thresholds (in inches)
    u8 optimal_range_min = 0;  // Minimum optimal engagement range
    u8 optimal_range_max = 36; // Maximum optimal engagement range

    // Priority adjustment for AI decision weight
    i8 priority_modifier = 0;  // Adjusts decision priority (-10 to +10)

    // Default constructor
    constexpr AIHints() = default;

    // Full constructor
    constexpr AIHints(
        DistancePreference dist,
        EngagementPreference engage,
        AIHintMask flags,
        i8 offense,
        i8 defense,
        u8 range_min = 0,
        u8 range_max = 36,
        i8 priority = 0
    )
        : distance_pref(dist)
        , engagement_pref(engage)
        , hint_flags(flags)
        , offensive_weight(offense)
        , defensive_weight(defense)
        , optimal_range_min(range_min)
        , optimal_range_max(range_max)
        , priority_modifier(priority) {}

    // Fluent builder methods
    constexpr AIHints& prefer_close() {
        distance_pref = DistancePreference::CLOSE;
        return *this;
    }

    constexpr AIHints& prefer_far() {
        distance_pref = DistancePreference::FAR;
        return *this;
    }

    constexpr AIHints& aggressive() {
        engagement_pref = EngagementPreference::AGGRESSIVE;
        return *this;
    }

    constexpr AIHints& defensive() {
        engagement_pref = EngagementPreference::DEFENSIVE;
        return *this;
    }

    constexpr AIHints& add_flag(AIHintFlag flag) {
        hint_flags |= static_cast<AIHintMask>(flag);
        return *this;
    }

    constexpr AIHints& offense(i8 weight) {
        offensive_weight = weight;
        return *this;
    }

    constexpr AIHints& defense(i8 weight) {
        defensive_weight = weight;
        return *this;
    }

    constexpr AIHints& optimal_range(u8 min, u8 max) {
        optimal_range_min = min;
        optimal_range_max = max;
        return *this;
    }

    constexpr AIHints& priority(i8 mod) {
        priority_modifier = mod;
        return *this;
    }

    // Check if hints suggest melee preference
    bool prefers_melee() const {
        return has_hint(hint_flags, AIHintFlag::PREFERS_MELEE) ||
               distance_pref == DistancePreference::CLOSE ||
               engagement_pref == EngagementPreference::AGGRESSIVE;
    }

    // Check if hints suggest shooting preference
    bool prefers_shooting() const {
        return has_hint(hint_flags, AIHintFlag::PREFERS_SHOOTING) ||
               distance_pref == DistancePreference::FAR ||
               engagement_pref == EngagementPreference::DEFENSIVE;
    }

    // Check if rule benefits from charging
    bool benefits_from_charge() const {
        return has_hint(hint_flags, AIHintFlag::BENEFITS_FROM_CHARGE);
    }
};

// Compile-time size check - keep it reasonably small
static_assert(sizeof(AIHints) <= 16, "AIHints should be 16 bytes or less");

// ==============================================================================
// AI Hint Builder - Fluent API for creating AI hints
// ==============================================================================

struct AIHintBuilder {
    AIHints hints{};

    constexpr AIHintBuilder& close_range() {
        hints.distance_pref = DistancePreference::CLOSE;
        return *this;
    }

    constexpr AIHintBuilder& long_range() {
        hints.distance_pref = DistancePreference::FAR;
        return *this;
    }

    constexpr AIHintBuilder& aggressive() {
        hints.engagement_pref = EngagementPreference::AGGRESSIVE;
        return *this;
    }

    constexpr AIHintBuilder& defensive() {
        hints.engagement_pref = EngagementPreference::DEFENSIVE;
        return *this;
    }

    constexpr AIHintBuilder& opportunistic() {
        hints.engagement_pref = EngagementPreference::OPPORTUNISTIC;
        return *this;
    }

    constexpr AIHintBuilder& melee_focused() {
        hints.hint_flags |= static_cast<AIHintMask>(AIHintFlag::PREFERS_MELEE);
        return *this;
    }

    constexpr AIHintBuilder& shooting_focused() {
        hints.hint_flags |= static_cast<AIHintMask>(AIHintFlag::PREFERS_SHOOTING);
        return *this;
    }

    constexpr AIHintBuilder& charge_bonus() {
        hints.hint_flags |= static_cast<AIHintMask>(AIHintFlag::BENEFITS_FROM_CHARGE);
        return *this;
    }

    constexpr AIHintBuilder& first_strike() {
        hints.hint_flags |= static_cast<AIHintMask>(AIHintFlag::WANTS_FIRST_STRIKE);
        return *this;
    }

    constexpr AIHintBuilder& holding_bonus() {
        hints.hint_flags |= static_cast<AIHintMask>(AIHintFlag::PREFERS_HOLDING);
        return *this;
    }

    constexpr AIHintBuilder& avoid_charges() {
        hints.hint_flags |= static_cast<AIHintMask>(AIHintFlag::AVOID_BEING_CHARGED);
        return *this;
    }

    constexpr AIHintBuilder& target_weak() {
        hints.hint_flags |= static_cast<AIHintMask>(AIHintFlag::PICK_WEAK_TARGETS);
        return *this;
    }

    constexpr AIHintBuilder& target_tough() {
        hints.hint_flags |= static_cast<AIHintMask>(AIHintFlag::PICK_TOUGH_TARGETS);
        return *this;
    }

    constexpr AIHintBuilder& high_variance() {
        hints.hint_flags |= static_cast<AIHintMask>(AIHintFlag::HIGH_RISK_HIGH_REWARD);
        return *this;
    }

    constexpr AIHintBuilder& consistent() {
        hints.hint_flags |= static_cast<AIHintMask>(AIHintFlag::CONSISTENT_VALUE);
        return *this;
    }

    constexpr AIHintBuilder& mobile() {
        hints.hint_flags |= static_cast<AIHintMask>(AIHintFlag::MOBILE_ADVANTAGE);
        return *this;
    }

    constexpr AIHintBuilder& static_bonus() {
        hints.hint_flags |= static_cast<AIHintMask>(AIHintFlag::STATIC_ADVANTAGE);
        return *this;
    }

    constexpr AIHintBuilder& offensive_bonus() {
        hints.hint_flags |= static_cast<AIHintMask>(AIHintFlag::OFFENSIVE_BONUS);
        return *this;
    }

    constexpr AIHintBuilder& defensive_bonus() {
        hints.hint_flags |= static_cast<AIHintMask>(AIHintFlag::DEFENSIVE_BONUS);
        return *this;
    }

    constexpr AIHintBuilder& offense(i8 weight) {
        hints.offensive_weight = weight;
        return *this;
    }

    constexpr AIHintBuilder& defense(i8 weight) {
        hints.defensive_weight = weight;
        return *this;
    }

    constexpr AIHintBuilder& optimal_range(u8 min, u8 max) {
        hints.optimal_range_min = min;
        hints.optimal_range_max = max;
        return *this;
    }

    constexpr AIHintBuilder& priority(i8 mod) {
        hints.priority_modifier = mod;
        return *this;
    }

    constexpr AIHints build() const { return hints; }

    // Implicit conversion to AIHints
    constexpr operator AIHints() const { return hints; }
};

// ==============================================================================
// Aggregated AI Profile - Combined hints from all active rules
// ==============================================================================
// This is computed once during AI planning phase by aggregating hints
// from all active rules on a unit.

struct AIProfile {
    // Aggregated weights (can exceed -100/+100 range when combined)
    i32 total_offensive = 0;
    i32 total_defensive = 0;

    // Combined hint flags
    AIHintMask combined_flags = 0;

    // Preference counts (for voting-style decisions)
    u8 close_preference_count = 0;
    u8 far_preference_count = 0;
    u8 aggressive_count = 0;
    u8 defensive_count = 0;

    // Optimal engagement range (intersection of all rule ranges)
    u8 optimal_min = 0;
    u8 optimal_max = 36;

    // Priority accumulator
    i16 priority_total = 0;

    // Add hints from a rule
    void add_hints(const AIHints& hints) {
        total_offensive += hints.offensive_weight;
        total_defensive += hints.defensive_weight;
        combined_flags |= hints.hint_flags;

        switch (hints.distance_pref) {
            case DistancePreference::CLOSE: close_preference_count++; break;
            case DistancePreference::FAR: far_preference_count++; break;
            default: break;
        }

        switch (hints.engagement_pref) {
            case EngagementPreference::AGGRESSIVE: aggressive_count++; break;
            case EngagementPreference::DEFENSIVE: defensive_count++; break;
            default: break;
        }

        // Narrow optimal range to intersection
        optimal_min = std::max(optimal_min, hints.optimal_range_min);
        optimal_max = std::min(optimal_max, hints.optimal_range_max);

        priority_total += hints.priority_modifier;
    }

    // Get recommended distance preference
    DistancePreference get_distance_preference() const {
        if (close_preference_count > far_preference_count) {
            return DistancePreference::CLOSE;
        } else if (far_preference_count > close_preference_count) {
            return DistancePreference::FAR;
        }
        return DistancePreference::NEUTRAL;
    }

    // Get recommended engagement preference
    EngagementPreference get_engagement_preference() const {
        if (aggressive_count > defensive_count) {
            return EngagementPreference::AGGRESSIVE;
        } else if (defensive_count > aggressive_count) {
            return EngagementPreference::DEFENSIVE;
        }
        return EngagementPreference::OPPORTUNISTIC;
    }

    // Check if profile suggests offensive focus
    bool is_offensive() const {
        return total_offensive > total_defensive;
    }

    // Check if profile suggests defensive focus
    bool is_defensive() const {
        return total_defensive > total_offensive;
    }

    // Check if profile benefits from charging
    bool should_charge() const {
        return has_hint(combined_flags, AIHintFlag::BENEFITS_FROM_CHARGE) ||
               aggressive_count > defensive_count;
    }

    // Check if in optimal engagement range
    bool in_optimal_range(u8 distance) const {
        return distance >= optimal_min && distance <= optimal_max;
    }
};

// ==============================================================================
// Preset AI Hint Combinations
// ==============================================================================

namespace ai_presets {

// Melee-focused attacker (Impact, Lance, Furious)
constexpr AIHints MELEE_ATTACKER = AIHintBuilder()
    .close_range()
    .aggressive()
    .melee_focused()
    .charge_bonus()
    .offense(30)
    .optimal_range(0, 12);

// Ranged attacker (Relentless, Sniper)
constexpr AIHints RANGED_ATTACKER = AIHintBuilder()
    .long_range()
    .defensive()
    .shooting_focused()
    .holding_bonus()
    .offense(25)
    .optimal_range(12, 36);

// Defensive unit (Shielded, Regeneration)
constexpr AIHints DEFENSIVE = AIHintBuilder()
    .defensive()
    .defensive_bonus()
    .static_bonus()
    .defense(30);

// Fast assault unit (Fast, Agile)
constexpr AIHints FAST_ASSAULT = AIHintBuilder()
    .aggressive()
    .mobile()
    .charge_bonus()
    .first_strike()
    .offense(20);

// Heavy hitter (Deadly, Blast)
constexpr AIHints HEAVY_HITTER = AIHintBuilder()
    .high_variance()
    .offensive_bonus()
    .target_tough()
    .offense(40);

// Skirmisher (Hit & Run, Stealth)
constexpr AIHints SKIRMISHER = AIHintBuilder()
    .opportunistic()
    .mobile()
    .avoid_charges()
    .offense(15)
    .defense(15);

// Tank (Tough, Shielded)
constexpr AIHints TANK = AIHintBuilder()
    .defensive()
    .consistent()
    .defensive_bonus()
    .defense(40);

// Charge specialist (Impact, Furious, Lance)
constexpr AIHints CHARGE_SPECIALIST = AIHintBuilder()
    .close_range()
    .aggressive()
    .charge_bonus()
    .first_strike()
    .offense(35)
    .optimal_range(0, 16);

} // namespace ai_presets

} // namespace battle
