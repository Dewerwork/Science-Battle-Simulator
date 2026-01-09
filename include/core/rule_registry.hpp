#pragma once

#include "core/types.hpp"
#include "core/phases.hpp"
#include "core/traits.hpp"
#include "core/rule_data.hpp"
#include "core/rule_effect.hpp"
#include "core/rule_presence.hpp"
#include <unordered_map>
#include <string_view>
#include <optional>
#include <vector>
#include <algorithm>
#include <stdexcept>

namespace battle {

// ==============================================================================
// Rule Registry - Centralized rule data and effect storage
// ==============================================================================
// IMPORTANT: This class uses dependency injection, NOT singleton pattern.
// Create one instance and pass it explicitly to components that need it.
//
// Thread safety: This class is thread-safe for read operations after
// initialization. Do not modify after passing to combat engine.

// Forward declarations
struct Unit;
struct Weapon;
struct CombatContextCore;

// ==============================================================================
// Applicable Rule - A rule that applies in a specific context
// ==============================================================================

struct ApplicableRule {
    RuleId id;                          // Which rule
    u8 value;                           // Rule's value parameter
    const RuleHotData* hot_data;        // Pointer to hot data
    Target source;                      // Where the rule came from (attacker/defender/weapon)
};

// ==============================================================================
// Rule Registry Class
// ==============================================================================

class RuleRegistry {
public:
    // ==============================================================================
    // Construction - No default constructor, must initialize with data
    // ==============================================================================

    // Create registry with data store
    explicit RuleRegistry(RuleDataStore&& data_store);

    // Create empty registry (for testing)
    RuleRegistry();

    // Non-copyable (to prevent accidental copies)
    RuleRegistry(const RuleRegistry&) = delete;
    RuleRegistry& operator=(const RuleRegistry&) = delete;

    // Movable
    RuleRegistry(RuleRegistry&&) = default;
    RuleRegistry& operator=(RuleRegistry&&) = default;

    // ==============================================================================
    // Registration - Call during initialization only
    // ==============================================================================

    // Register hot data for a rule
    void register_hot_data(RuleId id, const RuleHotData& data);

    // Register cold data for a rule
    void register_cold_data(RuleId id, const RuleColdData& data);

    // Register effect entry for a rule
    void register_effects(RuleId id, const RuleEffectEntry& effects);

    // Register name alias for parsing
    void register_alias(std::string_view alias, RuleId id);

    // ==============================================================================
    // Queries - Thread-safe after initialization
    // ==============================================================================

    // Get hot data for a rule
    const RuleHotData& get_hot_data(RuleId id) const;

    // Get cold data for a rule
    const RuleColdData& get_cold_data(RuleId id) const;

    // Get effect entry for a rule
    const RuleEffectEntry& get_effects(RuleId id) const;

    // Parse rule name to RuleId
    std::optional<RuleId> parse_rule_name(std::string_view name) const;

    // ==============================================================================
    // Rule Collection - Gather applicable rules for a context
    // ==============================================================================

    // Collect all rules that apply in a combat sub-phase
    // Returns rules sorted by priority
    std::vector<ApplicableRule> collect_combat_rules(
        CombatSubPhase sub_phase,
        CombatType combat_type,
        const Unit& attacker,
        const Unit& defender,
        const Weapon& weapon
    ) const;

    // Collect rules from a single unit for a phase
    std::vector<ApplicableRule> collect_unit_rules(
        CombatSubPhase sub_phase,
        CombatType combat_type,
        const Unit& unit,
        Target source
    ) const;

    // Collect rules from a weapon for a phase
    std::vector<ApplicableRule> collect_weapon_rules(
        CombatSubPhase sub_phase,
        CombatType combat_type,
        const Weapon& weapon
    ) const;

    // ==============================================================================
    // Trait Queries
    // ==============================================================================

    // Get all rules with a specific trait
    const std::vector<RuleId>& get_rules_with_trait(RuleTrait trait) const;

    // Aggregate traits from a set of rules
    TraitMask aggregate_traits(const std::vector<ApplicableRule>& rules) const;

    // ==============================================================================
    // Validation - Call after all registration complete
    // ==============================================================================

    // Validate registry completeness and consistency
    // Throws std::runtime_error on validation failure
    void validate() const;

    // Check if a specific rule is registered
    bool is_registered(RuleId id) const;

    // Get count of registered rules
    size_t registered_count() const;

private:
    // Hot data indexed by RuleId
    RuleDataStore data_store_;

    // Effect tables indexed by RuleId
    std::array<RuleEffectEntry, MAX_RULES> effects_{};

    // Alias map for parsing
    std::unordered_map<std::string_view, RuleId> alias_map_;

    // Rules indexed by trait (precomputed for fast lookup)
    mutable std::unordered_map<u32, std::vector<RuleId>> trait_index_;
    mutable bool trait_index_dirty_ = true;

    // Helper to rebuild trait index
    void rebuild_trait_index() const;

    // Helper to check if rule applies to combat type
    bool applies_to_combat_type(const RuleHotData& data, CombatType type) const {
        return data.combat_type == CombatType::BOTH || data.combat_type == type;
    }

    // Helper to check if rule is in correct phase
    bool is_in_phase(const RuleHotData& data, CombatSubPhase phase) const {
        return data.game_phase == GamePhase::COMBAT &&
               data.combat_sub_phase() == phase;
    }
};

// ==============================================================================
// Implementation (header-only for now, can be moved to .cpp)
// ==============================================================================

inline RuleRegistry::RuleRegistry(RuleDataStore&& data_store)
    : data_store_(std::move(data_store)) {}

inline RuleRegistry::RuleRegistry() = default;

inline void RuleRegistry::register_hot_data(RuleId id, const RuleHotData& data) {
    data_store_.register_hot(id, data);
    trait_index_dirty_ = true;
}

inline void RuleRegistry::register_cold_data(RuleId id, const RuleColdData& data) {
    data_store_.register_cold(id, data);
}

inline void RuleRegistry::register_effects(RuleId id, const RuleEffectEntry& effects) {
    auto idx = static_cast<size_t>(id);
    if (idx < effects_.size()) {
        effects_[idx] = effects;
    }
}

inline void RuleRegistry::register_alias(std::string_view alias, RuleId id) {
    alias_map_[alias] = id;
}

inline const RuleHotData& RuleRegistry::get_hot_data(RuleId id) const {
    return data_store_.hot(id);
}

inline const RuleColdData& RuleRegistry::get_cold_data(RuleId id) const {
    return data_store_.cold(id);
}

inline const RuleEffectEntry& RuleRegistry::get_effects(RuleId id) const {
    auto idx = static_cast<size_t>(id);
    if (idx < effects_.size()) {
        return effects_[idx];
    }
    static RuleEffectEntry empty;
    return empty;
}

inline std::optional<RuleId> RuleRegistry::parse_rule_name(std::string_view name) const {
    auto it = alias_map_.find(name);
    if (it != alias_map_.end()) {
        return it->second;
    }
    return std::nullopt;
}

inline std::vector<ApplicableRule> RuleRegistry::collect_combat_rules(
    CombatSubPhase sub_phase,
    CombatType combat_type,
    const Unit& attacker,
    const Unit& defender,
    const Weapon& weapon
) const {
    std::vector<ApplicableRule> result;
    result.reserve(16);  // Typical max rules per combat

    // Collect from attacker
    auto attacker_rules = collect_unit_rules(sub_phase, combat_type, attacker, Target::ATTACKER);
    result.insert(result.end(), attacker_rules.begin(), attacker_rules.end());

    // Collect from defender
    auto defender_rules = collect_unit_rules(sub_phase, combat_type, defender, Target::DEFENDER);
    result.insert(result.end(), defender_rules.begin(), defender_rules.end());

    // Collect from weapon
    auto weapon_rules = collect_weapon_rules(sub_phase, combat_type, weapon);
    result.insert(result.end(), weapon_rules.begin(), weapon_rules.end());

    // Sort by priority (stable sort preserves order within same priority)
    std::stable_sort(result.begin(), result.end(),
        [](const ApplicableRule& a, const ApplicableRule& b) {
            return static_cast<u8>(a.hot_data->priority) <
                   static_cast<u8>(b.hot_data->priority);
        });

    return result;
}

inline const std::vector<RuleId>& RuleRegistry::get_rules_with_trait(RuleTrait trait) const {
    if (trait_index_dirty_) {
        rebuild_trait_index();
    }
    auto it = trait_index_.find(static_cast<u32>(trait));
    if (it != trait_index_.end()) {
        return it->second;
    }
    static std::vector<RuleId> empty;
    return empty;
}

inline TraitMask RuleRegistry::aggregate_traits(const std::vector<ApplicableRule>& rules) const {
    TraitMask result = 0;
    for (const auto& rule : rules) {
        result |= rule.hot_data->traits;
    }
    return result;
}

inline void RuleRegistry::validate() const {
    // Check that frequently-used rules have hot data registered
    for (size_t i = 1; i < static_cast<size_t>(RuleId::PRIMARY_COUNT); ++i) {
        const auto& hot = data_store_.hot(static_cast<RuleId>(i));
        // Skip reserved slots
        if (i >= 59 && i < 64) continue;

        if (hot.id == RuleId::None && i < static_cast<size_t>(RuleId::PRIMARY_COUNT)) {
            // Rule not registered - this is a warning, not an error
            // Some rules may be legitimately unused
        }
    }
}

inline bool RuleRegistry::is_registered(RuleId id) const {
    return data_store_.hot(id).id != RuleId::None;
}

inline size_t RuleRegistry::registered_count() const {
    size_t count = 0;
    for (size_t i = 1; i < static_cast<size_t>(RuleId::COUNT); ++i) {
        if (data_store_.hot(static_cast<RuleId>(i)).id != RuleId::None) {
            count++;
        }
    }
    return count;
}

inline void RuleRegistry::rebuild_trait_index() const {
    trait_index_.clear();

    for (size_t i = 1; i < static_cast<size_t>(RuleId::COUNT); ++i) {
        const auto& hot = data_store_.hot(static_cast<RuleId>(i));
        if (hot.id == RuleId::None) continue;

        // Index each trait bit
        for (u32 bit = 0; bit < 32; ++bit) {
            u32 trait_mask = 1u << bit;
            if (hot.traits & trait_mask) {
                trait_index_[trait_mask].push_back(static_cast<RuleId>(i));
            }
        }
    }

    trait_index_dirty_ = false;
}

// ==============================================================================
// Rule Collection Implementations
// ==============================================================================

// Forward declare Unit and Weapon for implementation
// These are defined in unit.hpp and weapon.hpp
} // namespace battle

#include "core/unit.hpp"
#include "core/weapon.hpp"

namespace battle {

inline std::vector<ApplicableRule> RuleRegistry::collect_unit_rules(
    CombatSubPhase sub_phase,
    CombatType combat_type,
    const Unit& unit,
    Target source
) const {
    std::vector<ApplicableRule> result;

    for (u8 i = 0; i < unit.rule_count; ++i) {
        const auto& compact_rule = unit.rules[i];
        if (!compact_rule.is_valid()) continue;

        const auto& hot_data = get_hot_data(compact_rule.id);

        // Check if rule is in correct phase and combat type
        if (!is_in_phase(hot_data, sub_phase)) continue;
        if (!applies_to_combat_type(hot_data, combat_type)) continue;

        // Check target matches
        if (source == Target::ATTACKER && hot_data.target != Target::ATTACKER &&
            hot_data.target != Target::SELF) continue;
        if (source == Target::DEFENDER && hot_data.target != Target::DEFENDER) continue;

        result.push_back(ApplicableRule{
            .id = compact_rule.id,
            .value = compact_rule.value,
            .hot_data = &hot_data,
            .source = source
        });
    }

    return result;
}

inline std::vector<ApplicableRule> RuleRegistry::collect_weapon_rules(
    CombatSubPhase sub_phase,
    CombatType combat_type,
    const Weapon& weapon
) const {
    std::vector<ApplicableRule> result;

    for (u8 i = 0; i < weapon.rule_count; ++i) {
        const auto& compact_rule = weapon.rules[i];
        if (!compact_rule.is_valid()) continue;

        const auto& hot_data = get_hot_data(compact_rule.id);

        // Check if rule is in correct phase and combat type
        if (!is_in_phase(hot_data, sub_phase)) continue;
        if (!applies_to_combat_type(hot_data, combat_type)) continue;

        // Weapon rules should have target = WEAPON
        if (hot_data.target != Target::WEAPON && hot_data.target != Target::ATTACKER) continue;

        result.push_back(ApplicableRule{
            .id = compact_rule.id,
            .value = compact_rule.value,
            .hot_data = &hot_data,
            .source = Target::WEAPON
        });
    }

    return result;
}

// ==============================================================================
// Factory Functions - For creating registries
// ==============================================================================

// Create the default registry with all game rules registered
// Implementation in combat_rules.cpp
RuleRegistry create_default_registry();

// Create a test registry with only specified rules
// Useful for unit testing specific rule interactions
inline RuleRegistry create_test_registry(std::initializer_list<RuleHotData> rules) {
    RuleRegistry registry;
    for (const auto& rule : rules) {
        registry.register_hot_data(rule.id, rule);
    }
    return registry;
}

} // namespace battle
