#pragma once

#include "core/types.hpp"
#include <array>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace battle {

// ==============================================================================
// Faction Rule Effect Types
// ==============================================================================

enum class FactionRuleType : u8 {
    ArmyWide = 0,       // Applies to all units in army
    Special = 1,        // Model/unit specific rule
    Aura = 2            // Aura effect (affects nearby units)
};

enum class FactionRuleCategory : u8 {
    None = 0,
    Weapon = 1,         // Affects weapon attacks
    Defense = 2,        // Affects defense rolls
    Movement = 3,       // Affects movement (not simulated in combat)
    Unit = 4,           // Affects unit behavior
    AuraEffect = 5      // Aura special rules
};

enum class TriggerTiming : u8 {
    Always = 0,              // Always active
    OncePerGame = 1,         // Can only be used once per game
    OncePerActivation = 2,   // Can be used once per unit activation
    OnCharge = 3,            // Triggers when charging
    OnBeingCharged = 4,      // Triggers when being charged
    StartOfRound = 5,        // Triggers at start of each round
    WhenShaken = 6,          // Triggers when becoming shaken
    WhenAttacking = 7,       // Triggers during attack resolution
    WhenDefending = 8,       // Triggers when being attacked
    OnModelKilled = 9        // Triggers when a model is killed
};

enum class TargetType : u8 {
    Self = 0,           // Affects the model/unit with the rule
    Unit = 1,           // Affects the entire unit
    FriendlyWithin = 2, // Affects friendly units within range
    EnemyWithin = 3,    // Affects enemy units within range
    AllWithin = 4       // Affects all units within range
};

// ==============================================================================
// Faction Rule Effect - Describes what a rule does
// ==============================================================================

struct FactionRuleEffect {
    // What the effect does
    RuleId grants_rule = RuleId::None;  // Grants this base rule
    i8 hit_modifier = 0;                // +/- to hit rolls
    i8 defense_modifier = 0;            // +/- to defense rolls
    i8 morale_modifier = 0;             // +/- to morale tests
    i8 ap_modifier = 0;                 // +/- to AP value
    u8 extra_attacks = 0;               // Additional attacks
    u8 extra_hits = 0;                  // Additional hits (flat)
    u8 extra_wounds = 0;                // Additional wounds
    u8 deals_hits = 0;                  // Deals this many hits to target
    u8 deals_wounds = 0;                // Deals this many wounds to target
    u8 ap_for_dealt_hits = 0;           // AP for dealt hits

    // Conditions
    bool melee_only = false;            // Only applies in melee
    bool shooting_only = false;         // Only applies when shooting
    bool on_6_to_hit = false;           // Only on unmodified 6s to hit
    bool on_1_to_defend = false;        // Only on unmodified 1s to defend
    bool vs_spells = false;             // Only vs spells (different behavior)
    bool ignores_regeneration = false;  // Bypasses regeneration

    // Targeting
    TargetType target = TargetType::Self;
    u8 range = 0;                       // Range in inches (0 = self only)

    constexpr FactionRuleEffect() = default;
};

// ==============================================================================
// Faction Rule - Complete rule definition
// ==============================================================================

struct FactionRule {
    static constexpr size_t MAX_NAME_LEN = 48;
    static constexpr size_t MAX_EFFECTS = 4;

    std::array<char, MAX_NAME_LEN> name{};
    u8 name_len = 0;

    FactionRuleType type = FactionRuleType::Special;
    FactionRuleCategory category = FactionRuleCategory::None;
    TriggerTiming trigger = TriggerTiming::Always;

    std::array<FactionRuleEffect, MAX_EFFECTS> effects{};
    u8 effect_count = 0;

    // For spells/abilities with casting cost
    u8 casting_cost = 0;

    // Tracking usage
    bool used_this_game = false;
    bool used_this_activation = false;

    FactionRule() = default;

    FactionRule(std::string_view rule_name, FactionRuleType t, FactionRuleCategory c)
        : type(t), category(c) {
        name_len = static_cast<u8>(std::min(rule_name.size(), MAX_NAME_LEN - 1));
        std::copy_n(rule_name.begin(), name_len, name.begin());
    }

    std::string_view get_name() const { return {name.data(), name_len}; }

    void add_effect(const FactionRuleEffect& effect) {
        if (effect_count < MAX_EFFECTS) {
            effects[effect_count++] = effect;
        }
    }

    void reset_usage() {
        used_this_game = false;
        used_this_activation = false;
    }

    bool can_use() const {
        if (trigger == TriggerTiming::OncePerGame && used_this_game) return false;
        if (trigger == TriggerTiming::OncePerActivation && used_this_activation) return false;
        return true;
    }

    void mark_used() {
        if (trigger == TriggerTiming::OncePerGame) used_this_game = true;
        if (trigger == TriggerTiming::OncePerActivation) used_this_activation = true;
    }
};

// ==============================================================================
// Faction Army Rules - All rules for a specific faction
// ==============================================================================

struct FactionArmyRules {
    static constexpr size_t MAX_FACTION_NAME = 48;
    static constexpr size_t MAX_ARMY_WIDE_RULES = 8;
    static constexpr size_t MAX_SPECIAL_RULES = 64;
    static constexpr size_t MAX_AURA_RULES = 64;

    std::array<char, MAX_FACTION_NAME> faction_name{};
    u8 faction_name_len = 0;

    // Army-wide rules (apply to all units)
    std::array<FactionRule, MAX_ARMY_WIDE_RULES> army_wide_rules{};
    u8 army_wide_count = 0;

    // Special rules available to units
    std::array<FactionRule, MAX_SPECIAL_RULES> special_rules{};
    u8 special_count = 0;

    // Aura special rules (spells, abilities)
    std::array<FactionRule, MAX_AURA_RULES> aura_rules{};
    u8 aura_count = 0;

    FactionArmyRules() = default;

    explicit FactionArmyRules(std::string_view name) {
        faction_name_len = static_cast<u8>(std::min(name.size(), MAX_FACTION_NAME - 1));
        std::copy_n(name.begin(), faction_name_len, faction_name.begin());
    }

    std::string_view get_faction_name() const {
        return {faction_name.data(), faction_name_len};
    }

    void add_army_wide_rule(const FactionRule& rule) {
        if (army_wide_count < MAX_ARMY_WIDE_RULES) {
            army_wide_rules[army_wide_count++] = rule;
        }
    }

    void add_special_rule(const FactionRule& rule) {
        if (special_count < MAX_SPECIAL_RULES) {
            special_rules[special_count++] = rule;
        }
    }

    void add_aura_rule(const FactionRule& rule) {
        if (aura_count < MAX_AURA_RULES) {
            aura_rules[aura_count++] = rule;
        }
    }

    // Find a rule by name
    const FactionRule* find_rule(std::string_view name) const {
        for (u8 i = 0; i < army_wide_count; ++i) {
            if (army_wide_rules[i].get_name() == name) return &army_wide_rules[i];
        }
        for (u8 i = 0; i < special_count; ++i) {
            if (special_rules[i].get_name() == name) return &special_rules[i];
        }
        for (u8 i = 0; i < aura_count; ++i) {
            if (aura_rules[i].get_name() == name) return &aura_rules[i];
        }
        return nullptr;
    }
};

// ==============================================================================
// Faction Rules Registry - Global storage for all faction rules
// ==============================================================================

class FactionRulesRegistry {
public:
    static constexpr size_t MAX_FACTIONS = 64;

    static FactionRulesRegistry& instance() {
        static FactionRulesRegistry registry;
        return registry;
    }

    // Register a faction's rules
    void register_faction(const FactionArmyRules& faction_rules) {
        if (faction_count_ < MAX_FACTIONS) {
            factions_[faction_count_++] = faction_rules;
            // Update name map
            std::string name(faction_rules.get_faction_name());
            faction_map_[name] = faction_count_ - 1;
        }
    }

    // Get rules for a faction by name
    const FactionArmyRules* get_faction(std::string_view name) const {
        auto it = faction_map_.find(std::string(name));
        if (it != faction_map_.end()) {
            return &factions_[it->second];
        }
        return nullptr;
    }

    // Get all registered factions
    const std::array<FactionArmyRules, MAX_FACTIONS>& get_all_factions() const {
        return factions_;
    }

    u8 faction_count() const { return faction_count_; }

    // Clear all registered factions
    void clear() {
        faction_count_ = 0;
        faction_map_.clear();
    }

    // Check if registry is initialized
    bool is_initialized() const { return initialized_; }
    void set_initialized(bool val) { initialized_ = val; }

private:
    FactionRulesRegistry() = default;

    std::array<FactionArmyRules, MAX_FACTIONS> factions_{};
    std::unordered_map<std::string, u8> faction_map_;
    u8 faction_count_ = 0;
    bool initialized_ = false;
};

// Convenience function
inline FactionRulesRegistry& get_faction_registry() {
    return FactionRulesRegistry::instance();
}

// Initialize all faction rules from data
// Must be called before using the registry
void initialize_faction_rules();

} // namespace battle
