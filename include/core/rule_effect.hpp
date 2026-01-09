#pragma once

#include "core/types.hpp"
#include "core/phases.hpp"

namespace battle {

// Forward declarations for context types
struct CombatContextCore;
struct CombatContextExtended;
struct MovementContext;
struct DeploymentContext;
struct EndRoundContext;
struct Unit;

// ==============================================================================
// Function Pointer Types for Rule Effects
// ==============================================================================
// CRITICAL: These are raw function pointers, NOT std::function.
// This is intentional for performance - no heap allocation or type erasure.
// All effect functions must be free functions or static member functions.

// Combat effect function - modifies combat context
// Parameters:
//   ctx_core: Core combat context (always available, 64 bytes)
//   ctx_ext: Extended context (allocated only if needed)
//   value: Rule's numeric value (e.g., 3 for Blast(3))
using CombatEffect = void(*)(CombatContextCore& ctx_core,
                              CombatContextExtended* ctx_ext,
                              u8 value);

// Condition check function - returns true if rule should apply
// Parameters:
//   ctx_core: Core combat context for checking conditions
// Note: Takes const reference since conditions shouldn't modify state
using ConditionCheck = bool(*)(const CombatContextCore& ctx_core);

// Movement effect function - modifies movement context
using MovementEffect = void(*)(MovementContext& ctx, u8 value);

// Deployment effect function - modifies deployment context
using DeploymentEffect = void(*)(DeploymentContext& ctx, u8 value);

// End round effect function - modifies end round context and unit
using EndRoundEffect = void(*)(EndRoundContext& ctx, Unit& unit, u8 value);

// ==============================================================================
// Rule Effect Table
// ==============================================================================
// Static table indexed by RuleId containing all effect function pointers.
// This allows O(1) lookup of effects without virtual dispatch.

struct RuleEffectEntry {
    // Combat effects indexed by CombatSubPhase
    CombatEffect combat_effects[COMBAT_SUB_PHASE_COUNT] = {};

    // Condition checks indexed by CombatSubPhase
    ConditionCheck conditions[COMBAT_SUB_PHASE_COUNT] = {};

    // Movement effects indexed by MoveSubPhase
    MovementEffect movement_effects[MOVE_SUB_PHASE_COUNT] = {};

    // Deployment effects indexed by DeploySubPhase
    DeploymentEffect deploy_effects[DEPLOY_SUB_PHASE_COUNT] = {};

    // End round effects indexed by EndRoundSubPhase
    EndRoundEffect endround_effects[ENDROUND_SUB_PHASE_COUNT] = {};
};

// Maximum rules supported (matches RuleId::COUNT limit)
constexpr size_t MAX_RULE_EFFECTS = 320;

// ==============================================================================
// Effect Registration Helpers
// ==============================================================================

// Helper struct for building effect entries at compile time
struct EffectBuilder {
    RuleEffectEntry entry{};

    constexpr EffectBuilder& combat(CombatSubPhase phase, CombatEffect effect) {
        entry.combat_effects[static_cast<size_t>(phase)] = effect;
        return *this;
    }

    constexpr EffectBuilder& condition(CombatSubPhase phase, ConditionCheck check) {
        entry.conditions[static_cast<size_t>(phase)] = check;
        return *this;
    }

    constexpr EffectBuilder& movement(MoveSubPhase phase, MovementEffect effect) {
        entry.movement_effects[static_cast<size_t>(phase)] = effect;
        return *this;
    }

    constexpr EffectBuilder& deploy(DeploySubPhase phase, DeploymentEffect effect) {
        entry.deploy_effects[static_cast<size_t>(phase)] = effect;
        return *this;
    }

    constexpr EffectBuilder& endround(EndRoundSubPhase phase, EndRoundEffect effect) {
        entry.endround_effects[static_cast<size_t>(phase)] = effect;
        return *this;
    }

    constexpr RuleEffectEntry build() const { return entry; }
};

// ==============================================================================
// Effect Application Helpers
// ==============================================================================

// Apply a combat effect if present
inline void apply_combat_effect(const RuleEffectEntry& entry,
                                CombatSubPhase phase,
                                CombatContextCore& ctx_core,
                                CombatContextExtended* ctx_ext,
                                u8 value) {
    auto effect = entry.combat_effects[static_cast<size_t>(phase)];
    if (effect) {
        effect(ctx_core, ctx_ext, value);
    }
}

// Check a combat condition if present (returns true if no condition)
inline bool check_combat_condition(const RuleEffectEntry& entry,
                                   CombatSubPhase phase,
                                   const CombatContextCore& ctx_core) {
    auto condition = entry.conditions[static_cast<size_t>(phase)];
    return !condition || condition(ctx_core);
}

// Apply a movement effect if present
inline void apply_movement_effect(const RuleEffectEntry& entry,
                                  MoveSubPhase phase,
                                  MovementContext& ctx,
                                  u8 value) {
    auto effect = entry.movement_effects[static_cast<size_t>(phase)];
    if (effect) {
        effect(ctx, value);
    }
}

} // namespace battle
