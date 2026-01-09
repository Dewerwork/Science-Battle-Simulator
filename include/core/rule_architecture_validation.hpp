#pragma once

// ==============================================================================
// Rule Architecture Validation
// ==============================================================================
// This header contains compile-time validation for the rule architecture.
// Include this in at least one compilation unit to verify size constraints.

#include "core/types.hpp"
#include "core/phases.hpp"
#include "core/traits.hpp"
#include "core/rule_effect.hpp"
#include "core/rule_data.hpp"
#include "core/contexts.hpp"
#include "core/rule_presence.hpp"

namespace battle {
namespace validation {

// ==============================================================================
// Size Validation
// ==============================================================================

// RuleHotData must be exactly 16 bytes for cache efficiency
// 71 rules * 16 bytes = 1,136 bytes (fits in L1 cache)
static_assert(sizeof(RuleHotData) == 16,
    "RuleHotData must be exactly 16 bytes");

// CombatContextCore must fit in a cache line (64 bytes)
static_assert(sizeof(CombatContextCore) <= 64,
    "CombatContextCore must fit in 64 bytes (one cache line)");

// CompactRule size (changed from 2 to 4 bytes)
static_assert(sizeof(CompactRule) == 4,
    "CompactRule must be 4 bytes");

// ==============================================================================
// RuleId Validation
// ==============================================================================

// PRIMARY_COUNT must be exactly 64 for PrimaryRuleMask to work correctly
static_assert(static_cast<size_t>(RuleId::PRIMARY_COUNT) == 64,
    "PRIMARY_COUNT must be exactly 64");

// Total rule count must not exceed extended capacity
static_assert(static_cast<size_t>(RuleId::COUNT) <= 320,
    "RuleId::COUNT must not exceed 320");

// Verify the reserved slots are in the expected position
static_assert(static_cast<size_t>(RuleId::_Reserved59) == 59,
    "_Reserved59 must be at position 59");
static_assert(static_cast<size_t>(RuleId::_Reserved63) == 63,
    "_Reserved63 must be at position 63");

// ==============================================================================
// Enum Size Validation
// ==============================================================================

// All phase enums should be u8
static_assert(sizeof(GamePhase) == 1, "GamePhase should be 1 byte");
static_assert(sizeof(CombatSubPhase) == 1, "CombatSubPhase should be 1 byte");
static_assert(sizeof(MoveSubPhase) == 1, "MoveSubPhase should be 1 byte");
static_assert(sizeof(DeploySubPhase) == 1, "DeploySubPhase should be 1 byte");
static_assert(sizeof(EndRoundSubPhase) == 1, "EndRoundSubPhase should be 1 byte");

// Other compact enums
static_assert(sizeof(CombatType) == 1, "CombatType should be 1 byte");
static_assert(sizeof(Target) == 1, "Target should be 1 byte");
static_assert(sizeof(Trigger) == 1, "Trigger should be 1 byte");
static_assert(sizeof(RulePriority) == 1, "RulePriority should be 1 byte");

// RuleId is now u16
static_assert(sizeof(RuleId) == 2, "RuleId should be 2 bytes");

// TraitMask is u32
static_assert(sizeof(TraitMask) == 4, "TraitMask should be 4 bytes");

// ==============================================================================
// Mask Validation
// ==============================================================================

// PrimaryRuleMask must be 64 bits
static_assert(sizeof(PrimaryRuleMask) == 8,
    "PrimaryRuleMask must be 8 bytes (64 bits)");

// PRIMARY_RULE_LIMIT must match RuleId::PRIMARY_COUNT
static_assert(PRIMARY_RULE_LIMIT == static_cast<size_t>(RuleId::PRIMARY_COUNT),
    "PRIMARY_RULE_LIMIT must match RuleId::PRIMARY_COUNT");

// ==============================================================================
// Phase Count Validation
// ==============================================================================

static_assert(COMBAT_SUB_PHASE_COUNT == 8,
    "COMBAT_SUB_PHASE_COUNT should be 8");
static_assert(MOVE_SUB_PHASE_COUNT == 6,
    "MOVE_SUB_PHASE_COUNT should be 6");
static_assert(DEPLOY_SUB_PHASE_COUNT == 4,
    "DEPLOY_SUB_PHASE_COUNT should be 4");
static_assert(ENDROUND_SUB_PHASE_COUNT == 4,
    "ENDROUND_SUB_PHASE_COUNT should be 4");

// ==============================================================================
// Function Pointer Size Validation (sanity check)
// ==============================================================================

static_assert(sizeof(CombatEffect) == sizeof(void*),
    "CombatEffect should be pointer-sized");
static_assert(sizeof(ConditionCheck) == sizeof(void*),
    "ConditionCheck should be pointer-sized");

// ==============================================================================
// Validation Function (called at runtime in debug builds)
// ==============================================================================

#ifndef NDEBUG
inline void validate_rule_architecture() {
    // Runtime validation can be added here
    // For now, all validation is compile-time
}
#endif

} // namespace validation
} // namespace battle
