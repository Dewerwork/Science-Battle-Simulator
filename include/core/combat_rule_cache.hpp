#pragma once

// ==============================================================================
// Combat Rule Cache - Phase 8 Performance Optimization
// ==============================================================================
// This cache stores pre-computed rule data for a single combat resolution.
// It eliminates repeated rule lookups during the combat phases by caching:
// - Which rules are active for attacker, defender, and weapon
// - Pre-computed trait masks
// - Rule values for rules that carry values (Blast, Deadly, etc.)
//
// Usage:
//   CombatRuleCache cache;
//   cache.initialize(attacker, defender, weapon, combat_type, is_charge);
//   // Now use cache.has_rule(), cache.get_value() instead of unit.has_rule()

#include "core/types.hpp"
#include "core/unit.hpp"
#include "core/weapon.hpp"
#include "core/phases.hpp"
#include "core/traits.hpp"
#include "core/rule_presence.hpp"
#include <array>
#include <cstring>

namespace battle {

// ==============================================================================
// Combat Rule Cache
// ==============================================================================

class CombatRuleCache {
public:
    // Source of a rule (for logging/debugging)
    enum class RuleSource : u8 {
        NONE = 0,
        ATTACKER = 1,
        DEFENDER = 2,
        WEAPON = 3
    };

    CombatRuleCache() = default;

    // Initialize cache for a combat resolution
    void initialize(
        const Unit& attacker,
        const Unit& defender,
        const Weapon& weapon,
        CombatType combat_type,
        bool is_charge
    ) {
        // Clear previous data
        clear();

        // Cache pointers
        attacker_ = &attacker;
        defender_ = &defender;
        weapon_ = &weapon;
        combat_type_ = combat_type;
        is_charge_ = is_charge;

        // Cache attacker rules
        for (u8 i = 0; i < attacker.rule_count; ++i) {
            const auto& rule = attacker.rules[i];
            if (!rule.is_valid()) continue;
            set_rule(rule.id, rule.value, RuleSource::ATTACKER);
        }

        // Cache defender rules
        for (u8 i = 0; i < defender.rule_count; ++i) {
            const auto& rule = defender.rules[i];
            if (!rule.is_valid()) continue;
            set_rule(rule.id, rule.value, RuleSource::DEFENDER);
        }

        // Cache weapon rules
        for (u8 i = 0; i < weapon.rule_count; ++i) {
            const auto& rule = weapon.rules[i];
            if (!rule.is_valid()) continue;
            set_rule(rule.id, rule.value, RuleSource::WEAPON);
        }

        // Pre-compute common trait combinations
        compute_aggregated_traits();

        initialized_ = true;
    }

    // Clear the cache
    void clear() {
        attacker_ = nullptr;
        defender_ = nullptr;
        weapon_ = nullptr;
        combat_type_ = CombatType::SHOOTING;
        is_charge_ = false;
        initialized_ = false;

        std::memset(rule_values_.data(), 0, rule_values_.size());
        std::memset(rule_sources_.data(), 0, rule_sources_.size());
        presence_.clear();

        attacker_traits_ = 0;
        defender_traits_ = 0;
        weapon_traits_ = 0;
        combined_traits_ = 0;

        bypasses_regeneration_ = false;
        bypasses_resistance_ = false;
        forces_defense_reroll_ = false;
    }

    // ==============================================================================
    // Rule Queries
    // ==============================================================================

    // Check if a rule is active from any source
    bool has_rule(RuleId id) const {
        return presence_.has_rule(id);
    }

    // Check if attacker has a rule
    bool attacker_has_rule(RuleId id) const {
        return get_source(id) == RuleSource::ATTACKER;
    }

    // Check if defender has a rule
    bool defender_has_rule(RuleId id) const {
        return get_source(id) == RuleSource::DEFENDER;
    }

    // Check if weapon has a rule
    bool weapon_has_rule(RuleId id) const {
        return get_source(id) == RuleSource::WEAPON;
    }

    // Get rule value (0 if no value or rule not present)
    u8 get_value(RuleId id) const {
        auto idx = static_cast<size_t>(id);
        if (idx >= rule_values_.size()) return 0;
        return rule_values_[idx];
    }

    // Get rule source
    RuleSource get_source(RuleId id) const {
        auto idx = static_cast<size_t>(id);
        if (idx >= rule_sources_.size()) return RuleSource::NONE;
        return static_cast<RuleSource>(rule_sources_[idx]);
    }

    // ==============================================================================
    // Trait Queries
    // ==============================================================================

    // Get combined traits from all sources
    TraitMask combined_traits() const { return combined_traits_; }

    // Check if combined traits include a specific trait
    bool has_trait(RuleTrait trait) const {
        return (combined_traits_ & static_cast<TraitMask>(trait)) != 0;
    }

    // Pre-computed common checks
    bool bypasses_regeneration() const { return bypasses_regeneration_; }
    bool bypasses_resistance() const { return bypasses_resistance_; }
    bool forces_defense_reroll() const { return forces_defense_reroll_; }

    // ==============================================================================
    // Context Accessors
    // ==============================================================================

    const Unit* attacker() const { return attacker_; }
    const Unit* defender() const { return defender_; }
    const Weapon* weapon() const { return weapon_; }
    CombatType combat_type() const { return combat_type_; }
    bool is_charge() const { return is_charge_; }
    bool is_initialized() const { return initialized_; }

    // ==============================================================================
    // Rule Counting
    // ==============================================================================

    // Count rules from each source
    u8 attacker_rule_count() const {
        u8 count = 0;
        for (size_t i = 0; i < rule_sources_.size(); ++i) {
            if (rule_sources_[i] == static_cast<u8>(RuleSource::ATTACKER)) count++;
        }
        return count;
    }

    u8 defender_rule_count() const {
        u8 count = 0;
        for (size_t i = 0; i < rule_sources_.size(); ++i) {
            if (rule_sources_[i] == static_cast<u8>(RuleSource::DEFENDER)) count++;
        }
        return count;
    }

    u8 weapon_rule_count() const {
        u8 count = 0;
        for (size_t i = 0; i < rule_sources_.size(); ++i) {
            if (rule_sources_[i] == static_cast<u8>(RuleSource::WEAPON)) count++;
        }
        return count;
    }

private:
    void set_rule(RuleId id, u8 value, RuleSource source) {
        auto idx = static_cast<size_t>(id);
        if (idx >= rule_values_.size()) return;

        presence_.set_rule(id);
        rule_values_[idx] = value;
        rule_sources_[idx] = static_cast<u8>(source);
    }

    void compute_aggregated_traits() {
        // Check for regen bypass rules
        bypasses_regeneration_ =
            has_rule(RuleId::Bane) ||
            has_rule(RuleId::Rending) ||
            has_rule(RuleId::Rupture) ||
            has_rule(RuleId::Shred) ||
            has_rule(RuleId::Unstoppable);

        // BaneInMelee only counts in melee
        if (combat_type_ == CombatType::MELEE && attacker_has_rule(RuleId::BaneInMelee)) {
            bypasses_regeneration_ = true;
        }

        // Check for defense reroll (Poison, Bane)
        forces_defense_reroll_ =
            has_rule(RuleId::Poison) ||
            has_rule(RuleId::Bane);

        // BaneInMelee also forces reroll in melee
        if (combat_type_ == CombatType::MELEE && attacker_has_rule(RuleId::BaneInMelee)) {
            forces_defense_reroll_ = true;
        }

        // Combine trait masks (would need trait data from registry for full implementation)
        if (bypasses_regeneration_) {
            combined_traits_ |= static_cast<TraitMask>(RuleTrait::BYPASSES_REGENERATION);
        }
        if (forces_defense_reroll_) {
            combined_traits_ |= static_cast<TraitMask>(RuleTrait::FORCES_DEFENSE_REROLL);
        }
    }

    // Cached pointers
    const Unit* attacker_ = nullptr;
    const Unit* defender_ = nullptr;
    const Weapon* weapon_ = nullptr;

    // Combat context
    CombatType combat_type_ = CombatType::SHOOTING;
    bool is_charge_ = false;
    bool initialized_ = false;

    // Rule presence (using tiered bitset)
    RulePresence presence_;

    // Rule values indexed by RuleId
    std::array<u8, MAX_RULES> rule_values_{};

    // Rule sources indexed by RuleId
    std::array<u8, MAX_RULES> rule_sources_{};

    // Pre-computed trait masks
    TraitMask attacker_traits_ = 0;
    TraitMask defender_traits_ = 0;
    TraitMask weapon_traits_ = 0;
    TraitMask combined_traits_ = 0;

    // Pre-computed common checks
    bool bypasses_regeneration_ = false;
    bool bypasses_resistance_ = false;
    bool forces_defense_reroll_ = false;
};

// ==============================================================================
// Combat Rule Cache Pool - Reusable cache instances
// ==============================================================================
// For high-performance scenarios (parallel simulation), use a pool to avoid
// repeated allocation.

template<size_t PoolSize = 16>
class CombatRuleCachePool {
public:
    CombatRuleCache* acquire() {
        for (size_t i = 0; i < PoolSize; ++i) {
            if (!in_use_[i]) {
                in_use_[i] = true;
                caches_[i].clear();
                return &caches_[i];
            }
        }
        // Pool exhausted - return nullptr (caller should handle)
        return nullptr;
    }

    void release(CombatRuleCache* cache) {
        for (size_t i = 0; i < PoolSize; ++i) {
            if (&caches_[i] == cache) {
                in_use_[i] = false;
                return;
            }
        }
    }

    size_t available() const {
        size_t count = 0;
        for (size_t i = 0; i < PoolSize; ++i) {
            if (!in_use_[i]) count++;
        }
        return count;
    }

private:
    std::array<CombatRuleCache, PoolSize> caches_{};
    std::array<bool, PoolSize> in_use_{};
};

// ==============================================================================
// RAII Cache Guard
// ==============================================================================
// Ensures cache is released back to pool when scope exits

template<size_t PoolSize>
class CacheGuard {
public:
    CacheGuard(CombatRuleCachePool<PoolSize>& pool)
        : pool_(pool), cache_(pool.acquire()) {}

    ~CacheGuard() {
        if (cache_) {
            pool_.release(cache_);
        }
    }

    CombatRuleCache* get() { return cache_; }
    CombatRuleCache* operator->() { return cache_; }
    CombatRuleCache& operator*() { return *cache_; }
    explicit operator bool() const { return cache_ != nullptr; }

    // Non-copyable
    CacheGuard(const CacheGuard&) = delete;
    CacheGuard& operator=(const CacheGuard&) = delete;

    // Movable
    CacheGuard(CacheGuard&& other) noexcept
        : pool_(other.pool_), cache_(other.cache_) {
        other.cache_ = nullptr;
    }

private:
    CombatRuleCachePool<PoolSize>& pool_;
    CombatRuleCache* cache_;
};

} // namespace battle
