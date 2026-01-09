#pragma once

#include "core/types.hpp"
#include "core/phases.hpp"
#include "core/traits.hpp"
#include "core/rule_presence.hpp"  // For MAX_RULES
#include <vector>
#include <array>

namespace battle {

// ==============================================================================
// Rule Hot Data - Compact, cache-friendly data used during combat
// ==============================================================================
// CRITICAL: This struct must be exactly 16 bytes for cache efficiency.
// 71 rules * 16 bytes = 1,136 bytes, fits in L1 cache (~32KB).
//
// All frequently-accessed data during combat resolution is here.
// Cold data (names, descriptions, AI hints) is stored separately.

struct RuleHotData {
    RuleId id;                  // 2 bytes - Rule identifier
    GamePhase game_phase;       // 1 byte  - Which game phase
    u8 sub_phase;               // 1 byte  - Which sub-phase (cast from enum)
    CombatType combat_type;     // 1 byte  - SHOOTING, MELEE, or BOTH
    Target target;              // 1 byte  - ATTACKER, DEFENDER, WEAPON, or SELF
    Trigger trigger;            // 1 byte  - ALWAYS, ON_CHARGE, etc.
    RulePriority priority;      // 1 byte  - Execution order within phase
    TraitMask traits;           // 4 bytes - Behavioral flags
    u8 _padding[4];             // 4 bytes - Padding to 16 bytes

    // Default constructor
    constexpr RuleHotData()
        : id(RuleId::None)
        , game_phase(GamePhase::PASSIVE)
        , sub_phase(0)
        , combat_type(CombatType::BOTH)
        , target(Target::SELF)
        , trigger(Trigger::ALWAYS)
        , priority(RulePriority::NORMAL)
        , traits(0)
        , _padding{} {}

    // Full constructor
    constexpr RuleHotData(
        RuleId id_,
        GamePhase phase_,
        u8 sub_phase_,
        CombatType combat_type_,
        Target target_,
        Trigger trigger_,
        RulePriority priority_,
        TraitMask traits_
    )
        : id(id_)
        , game_phase(phase_)
        , sub_phase(sub_phase_)
        , combat_type(combat_type_)
        , target(target_)
        , trigger(trigger_)
        , priority(priority_)
        , traits(traits_)
        , _padding{} {}

    // Helper to check combat sub-phase
    CombatSubPhase combat_sub_phase() const {
        return static_cast<CombatSubPhase>(sub_phase);
    }

    // Helper to check movement sub-phase
    MoveSubPhase move_sub_phase() const {
        return static_cast<MoveSubPhase>(sub_phase);
    }

    // Helper to check deployment sub-phase
    DeploySubPhase deploy_sub_phase() const {
        return static_cast<DeploySubPhase>(sub_phase);
    }

    // Helper to check end round sub-phase
    EndRoundSubPhase endround_sub_phase() const {
        return static_cast<EndRoundSubPhase>(sub_phase);
    }

    // Check if rule applies to a specific combat type
    bool applies_to(CombatType type) const {
        return combat_type == CombatType::BOTH || combat_type == type;
    }

    // Check if rule has a specific trait
    bool has_trait(RuleTrait trait) const {
        return (traits & static_cast<TraitMask>(trait)) != 0;
    }
};

// Compile-time size validation
static_assert(sizeof(RuleHotData) == 16, "RuleHotData must be exactly 16 bytes");

// ==============================================================================
// Rule Cold Data - Rarely accessed data, loaded on demand
// ==============================================================================
// This data is used during:
//   - Parsing (name, aliases)
//   - Logging (log_format)
//   - Documentation (description)
//   - AI decision making (ai_hints)
//
// NOT used during combat hot path, so size doesn't matter as much.

struct RuleColdData {
    const char* name;                 // Display name
    const char* const* aliases;       // Array of alias strings
    u8 alias_count;                   // Number of aliases
    const char* log_format;           // Format string for logging
    const char* description;          // Human-readable description

    // Default constructor
    constexpr RuleColdData()
        : name(nullptr)
        , aliases(nullptr)
        , alias_count(0)
        , log_format(nullptr)
        , description(nullptr) {}

    // Full constructor
    constexpr RuleColdData(
        const char* name_,
        const char* const* aliases_,
        u8 alias_count_,
        const char* log_format_,
        const char* description_
    )
        : name(name_)
        , aliases(aliases_)
        , alias_count(alias_count_)
        , log_format(log_format_)
        , description(description_) {}
};

// ==============================================================================
// Rule Data Store - Combined hot and cold data access
// ==============================================================================
// Provides unified interface to access rule data.
// Hot data is always available; cold data can be lazily loaded.

class RuleDataStore {
public:
    // Get hot data for a rule (always available)
    const RuleHotData& hot(RuleId id) const {
        auto idx = static_cast<size_t>(id);
        if (idx >= hot_data_.size()) {
            static RuleHotData empty;
            return empty;
        }
        return hot_data_[idx];
    }

    // Get cold data for a rule (may trigger lazy load)
    const RuleColdData& cold(RuleId id) const {
        auto idx = static_cast<size_t>(id);
        if (idx >= cold_data_.size()) {
            static RuleColdData empty;
            return empty;
        }
        return cold_data_[idx];
    }

    // Register hot data for a rule
    void register_hot(RuleId id, const RuleHotData& data) {
        auto idx = static_cast<size_t>(id);
        if (idx < hot_data_.size()) {
            hot_data_[idx] = data;
        }
    }

    // Register cold data for a rule
    void register_cold(RuleId id, const RuleColdData& data) {
        auto idx = static_cast<size_t>(id);
        if (idx < cold_data_.size()) {
            cold_data_[idx] = data;
        }
    }

    // Bulk register (for initialization)
    template<size_t N>
    void register_all_hot(const std::array<RuleHotData, N>& data) {
        for (size_t i = 0; i < N && i < hot_data_.size(); ++i) {
            hot_data_[i] = data[i];
        }
    }

    template<size_t N>
    void register_all_cold(const std::array<RuleColdData, N>& data) {
        for (size_t i = 0; i < N && i < cold_data_.size(); ++i) {
            cold_data_[i] = data[i];
        }
    }

    // Get all rules with a specific trait
    void get_rules_with_trait(RuleTrait trait, std::vector<RuleId>& out) const {
        for (size_t i = 1; i < static_cast<size_t>(RuleId::COUNT); ++i) {
            if (hot_data_[i].has_trait(trait)) {
                out.push_back(static_cast<RuleId>(i));
            }
        }
    }

    // Get all rules for a specific phase and sub-phase
    void get_rules_for_phase(GamePhase phase, u8 sub_phase, std::vector<RuleId>& out) const {
        for (size_t i = 1; i < static_cast<size_t>(RuleId::COUNT); ++i) {
            const auto& data = hot_data_[i];
            if (data.game_phase == phase && data.sub_phase == sub_phase) {
                out.push_back(static_cast<RuleId>(i));
            }
        }
    }

private:
    std::array<RuleHotData, MAX_RULES> hot_data_{};
    std::array<RuleColdData, MAX_RULES> cold_data_{};
};

// ==============================================================================
// Rule Definition Builder - Fluent API for creating rule data
// ==============================================================================

struct RuleDefinitionBuilder {
    RuleHotData hot{};
    RuleColdData cold{};

    constexpr RuleDefinitionBuilder& id(RuleId id_) {
        hot.id = id_;
        return *this;
    }

    constexpr RuleDefinitionBuilder& phase(GamePhase phase_) {
        hot.game_phase = phase_;
        return *this;
    }

    constexpr RuleDefinitionBuilder& combat_phase(CombatSubPhase phase_) {
        hot.game_phase = GamePhase::COMBAT;
        hot.sub_phase = static_cast<u8>(phase_);
        return *this;
    }

    constexpr RuleDefinitionBuilder& move_phase(MoveSubPhase phase_) {
        hot.game_phase = GamePhase::MOVEMENT;
        hot.sub_phase = static_cast<u8>(phase_);
        return *this;
    }

    constexpr RuleDefinitionBuilder& combat_type(CombatType type_) {
        hot.combat_type = type_;
        return *this;
    }

    constexpr RuleDefinitionBuilder& target(Target target_) {
        hot.target = target_;
        return *this;
    }

    constexpr RuleDefinitionBuilder& trigger(Trigger trigger_) {
        hot.trigger = trigger_;
        return *this;
    }

    constexpr RuleDefinitionBuilder& priority(RulePriority priority_) {
        hot.priority = priority_;
        return *this;
    }

    constexpr RuleDefinitionBuilder& traits(TraitMask traits_) {
        hot.traits = traits_;
        return *this;
    }

    constexpr RuleDefinitionBuilder& add_trait(RuleTrait trait_) {
        hot.traits |= static_cast<TraitMask>(trait_);
        return *this;
    }

    constexpr RuleDefinitionBuilder& name(const char* name_) {
        cold.name = name_;
        return *this;
    }

    constexpr RuleDefinitionBuilder& log_format(const char* format_) {
        cold.log_format = format_;
        return *this;
    }

    constexpr RuleDefinitionBuilder& description(const char* desc_) {
        cold.description = desc_;
        return *this;
    }
};

} // namespace battle
