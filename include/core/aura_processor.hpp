#pragma once

#include "core/types.hpp"
#include "core/modifiers.hpp"
#include "core/unit.hpp"
#include <vector>
#include <functional>

namespace battle {

// ==============================================================================
// Aura Processor - Handles aura application and removal
// ==============================================================================
// This class manages the application of aura effects to units within range.
// It processes all active auras at the start of each phase and removes
// aura effects when units move out of range or the source dies.

class AuraProcessor {
public:
    // Registered aura definitions
    struct RegisteredAura {
        RuleId source_rule;         // Rule that provides the aura
        RuleId grants_rule;         // Rule granted by the aura
        u8 grants_value;            // Value for granted rule
        u8 range;                   // Range in inches
        bool affects_friendly;      // Affects friendly units
        bool affects_enemy;         // Affects enemy units
        bool affects_self;          // Affects self
        ModifierCondition condition;// Additional condition
    };

    AuraProcessor() = default;

    // Register an aura definition
    void register_aura(const AuraDefinition& aura) {
        if (aura_count_ < MAX_AURAS) {
            RegisteredAura reg;
            reg.source_rule = aura.source_rule;
            reg.grants_rule = aura.grants_rule;
            reg.grants_value = aura.grants_value;
            reg.range = aura.range;
            reg.affects_friendly = aura.affects_friendly;
            reg.affects_enemy = aura.affects_enemy;
            reg.affects_self = aura.affects_self;
            reg.condition = aura.condition;
            auras_[aura_count_++] = reg;
        }
    }

    // Process auras for a pair of units (1v1 simulation)
    // Returns number of modifiers applied
    u32 process_auras_1v1(
        Unit& unit_a,
        Unit& unit_b,
        UnitModifiers& mods_a,
        UnitModifiers& mods_b,
        i8 pos_a,
        i8 pos_b,
        bool same_faction
    ) {
        u32 applied = 0;

        // Calculate distance between units
        u8 distance = static_cast<u8>(std::abs(pos_a - pos_b));

        // Clear previous aura modifiers
        mods_a.remove_modifiers_by_source(ModifierSource::AURA);
        mods_b.remove_modifiers_by_source(ModifierSource::AURA);

        // Check each registered aura
        for (size_t i = 0; i < aura_count_; ++i) {
            const auto& aura = auras_[i];

            // Check if unit_a has the aura source rule
            if (unit_a.has_rule(aura.source_rule)) {
                // Apply to unit_b if in range and conditions met
                if (distance <= aura.range) {
                    bool should_apply = false;

                    if (same_faction && aura.affects_friendly) {
                        should_apply = true;
                    }
                    if (!same_faction && aura.affects_enemy) {
                        should_apply = true;
                    }

                    if (should_apply) {
                        ActiveModifier mod(aura.grants_rule, aura.grants_value);
                        mod.source = ModifierSource::AURA;
                        mod.duration = ModifierDuration::UNTIL_END_OF_PHASE;
                        mod.source_unit_id = static_cast<u16>(unit_a.unit_id);
                        mod.condition = aura.condition;
                        mods_b.add_modifier(mod);
                        ++applied;
                    }
                }

                // Apply to self if configured
                if (aura.affects_self) {
                    ActiveModifier mod(aura.grants_rule, aura.grants_value);
                    mod.source = ModifierSource::AURA;
                    mod.duration = ModifierDuration::UNTIL_END_OF_PHASE;
                    mod.source_unit_id = static_cast<u16>(unit_a.unit_id);
                    mod.condition = aura.condition;
                    mods_a.add_modifier(mod);
                    ++applied;
                }
            }

            // Check if unit_b has the aura source rule
            if (unit_b.has_rule(aura.source_rule)) {
                // Apply to unit_a if in range and conditions met
                if (distance <= aura.range) {
                    bool should_apply = false;

                    if (same_faction && aura.affects_friendly) {
                        should_apply = true;
                    }
                    if (!same_faction && aura.affects_enemy) {
                        should_apply = true;
                    }

                    if (should_apply) {
                        ActiveModifier mod(aura.grants_rule, aura.grants_value);
                        mod.source = ModifierSource::AURA;
                        mod.duration = ModifierDuration::UNTIL_END_OF_PHASE;
                        mod.source_unit_id = static_cast<u16>(unit_b.unit_id);
                        mod.condition = aura.condition;
                        mods_a.add_modifier(mod);
                        ++applied;
                    }
                }

                // Apply to self if configured
                if (aura.affects_self) {
                    ActiveModifier mod(aura.grants_rule, aura.grants_value);
                    mod.source = ModifierSource::AURA;
                    mod.duration = ModifierDuration::UNTIL_END_OF_PHASE;
                    mod.source_unit_id = static_cast<u16>(unit_b.unit_id);
                    mod.condition = aura.condition;
                    mods_b.add_modifier(mod);
                    ++applied;
                }
            }
        }

        return applied;
    }

    // Remove all aura effects from a specific source (when source dies)
    void remove_auras_from_source(UnitModifiers& mods, u16 source_unit_id) {
        mods.remove_modifiers_from_source(source_unit_id);
    }

    // Get number of registered auras
    size_t aura_count() const { return aura_count_; }

    // Clear all registered auras
    void clear() { aura_count_ = 0; }

private:
    static constexpr size_t MAX_AURAS = 256;
    std::array<RegisteredAura, MAX_AURAS> auras_{};
    size_t aura_count_ = 0;
};

// ==============================================================================
// Default Aura Definitions
// ==============================================================================
// These are auras from base rules that grant effects to nearby units

namespace default_auras {

// Fear aura - grants Fear to self (affects enemy morale)
inline AuraDefinition fear_aura() {
    AuraDefinition aura;
    aura.source_rule = RuleId::Fear;
    aura.grants_rule = RuleId::Fear;
    aura.range = 0;  // Self only
    aura.affects_self = true;
    aura.affects_friendly = false;
    aura.affects_enemy = false;
    return aura;
}

// MoraleBoost aura - grants morale bonus to nearby friendlies
inline AuraDefinition morale_boost_aura(u8 value = 1, u8 range = 12) {
    AuraDefinition aura;
    aura.source_rule = RuleId::MoraleBoost;
    aura.grants_rule = RuleId::MoraleBoost;
    aura.grants_value = value;
    aura.range = range;
    aura.affects_friendly = true;
    aura.affects_enemy = false;
    aura.affects_self = true;
    return aura;
}

// HoldTheLine aura - grants morale reroll to nearby friendlies
inline AuraDefinition hold_the_line_aura(u8 range = 12) {
    AuraDefinition aura;
    aura.source_rule = RuleId::HoldTheLine;
    aura.grants_rule = RuleId::HoldTheLine;
    aura.range = range;
    aura.affects_friendly = true;
    aura.affects_enemy = false;
    aura.affects_self = true;
    return aura;
}

} // namespace default_auras

// ==============================================================================
// Initialize default auras in processor
// ==============================================================================

inline void register_default_auras(AuraProcessor& processor) {
    processor.register_aura(default_auras::fear_aura());
    processor.register_aura(default_auras::morale_boost_aura());
    processor.register_aura(default_auras::hold_the_line_aura());
}

} // namespace battle
